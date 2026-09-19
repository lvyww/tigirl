#include "SentenceDecoder.h"
#include "SentenceCharacterRanks.h"
#include "Grapheme.h"
#include "Unicode.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <deque>
namespace tiger {
namespace {
constexpr std::u16string_view bos=u"\x02",eos=u"\x03";
constexpr double supplementEarlyScale=.05,supplementEarlyCap=.75;
constexpr double learningEarlyScale=.075,learningEarlyCap=.75,personalizedEarlyCap=.80;
inline double learningMaturity(double score) {
    if(score<=9)return 0;
    const double weight=std::exp((score-9)/2);
    return std::clamp((weight-1)/2,0.0,1.0);
}
inline double learningEarlyContribution(double score) {
    return std::min(learningEarlyCap,std::max(0.0,score)*learningMaturity(score)*learningEarlyScale);
}
inline double supplementEarlyContribution(double score) {
    return std::min(supplementEarlyCap,std::max(0.0,score)*supplementEarlyScale);
}
inline double earlyScore(const SentenceCandidate& c) {
    return std::isnan(c.earlyCommitConfidenceScore)?c.confidenceScore:c.earlyCommitConfidenceScore;
}
struct State {
    double score=0,mass=0,supplementScore=0,learningScore=0,learningPotential=0,learningEarlyCommitBonus=0,codeScore=0;
    std::u16string text;
    unsigned source=SentenceSourceNone;
    int directRank=std::numeric_limits<int>::max();
    // Context tokens refer to immutable mapped lexicon text (or static BOS).
    // The decoder retains that lexicon for the whole lattice lifetime.
    std::u16string_view previous2=bos,previous1=bos;
    int supplementState=0,rank=1;
    std::shared_ptr<const SentencePathBoundary> boundary;
};
bool better(const State& a,const State& b,bool scoreFirst) {
    if(scoreFirst && a.score!=b.score)return a.score>b.score;
    if(a.rank!=b.rank)return a.rank<b.rank;
    if(a.score!=b.score)return a.score>b.score;
    return a.text<b.text;
}
struct Bucket {
    bool truncated=false,frozen=false;
    std::vector<State> values;
    // Store hashes and stable vector indices, not another owned copy of every
    // candidate string. Full text equality below resolves all hash collisions.
    std::unordered_multimap<std::size_t,std::size_t> indices;
    void add(State item) {
        const auto hashText=[](std::u16string_view text){return std::hash<std::u16string_view>{}(text);};
        if(frozen) {
            for(std::size_t i=0;i<values.size();++i)indices.emplace(hashText(values[i].text),i);
            frozen=false;
        }
        const auto hash=hashText(item.text);
        const auto range=indices.equal_range(hash);
        auto it=range.first;
        while(it!=range.second && values[it->second].text!=item.text)++it;
        if(it==range.second){indices.emplace(hash,values.size());values.push_back(std::move(item));return;}
        auto& old=values[it->second];
        double top=std::max(old.mass,item.mass);
        double combined=top+std::log(std::exp(old.mass-top)+std::exp(item.mass-top));
        unsigned source=old.source|item.source;
        int directRank=std::min(old.directRank,item.directRank);
        // Keep a legal learned path rather than another segmentation of the
        // same text which loses the remembered raw/text boundary alignment.
        bool learned=item.learningScore>0 || old.learningScore>0 || item.learningPotential>0 || old.learningPotential>0;
        const bool itemDirect=(item.source&SentenceSourceDirect)!=0,oldDirect=(old.source&SentenceSourceDirect)!=0;
        const bool replace=itemDirect!=oldDirect?itemDirect:
            ((learned && item.score+item.learningPotential>old.score+old.learningPotential) ||
             (!learned && (item.rank<old.rank || (item.rank==old.rank && item.score>old.score))));
        if(replace)old=std::move(item);
        old.mass=combined;old.source=source;old.directRank=directRank;
    }
    void limit(int width,bool scoreFirst) {
        if(frozen)return;
        frozen=true;
        auto order=[=](const State& a,const State& b){return better(a,b,scoreFirst);};
        if(values.size()>static_cast<std::size_t>(width)) {
            truncated=true;
            std::nth_element(values.begin(),values.begin()+width,values.end(),order);
            std::sort(values.begin(),values.begin()+width,order);
            // Four extra legal states may finish a learned multi-edge span.
            // Potential is never added to score/mass and disappears if the
            // remaining raw code cannot complete that span.
            auto first=values.begin()+width;
            auto reserveOrder=[](const State& a,const State& b) {
                if((a.learningPotential>0)!=(b.learningPotential>0))return a.learningPotential>0;
                return a.score+a.learningPotential>b.score+b.learningPotential;
            };
            auto extra=std::min<std::size_t>(4,values.size()-width);
            std::partial_sort(first,first+extra,values.end(),reserveOrder);
            std::size_t retained=width;
            while(retained<static_cast<std::size_t>(width)+extra && values[retained].learningPotential>0)++retained;
            values.resize(retained);
        }else std::sort(values.begin(),values.end(),order);
        // Compact only grossly oversized frozen buckets. Never change the beam,
        // tie order or candidate mass; avoid reallocating modest working slack.
        if(values.capacity()>values.size()*4 && values.capacity()-values.size()>4096) {
            std::vector<State> compact;compact.reserve(values.size());
            for(auto& value:values)compact.push_back(std::move(value));
            values.swap(compact);
        }
        // Keep processed positions compact for the whole composition.
        decltype(indices)().swap(indices);
    }
};
bool digit(char16_t c){return unicode::isDecimalDigit(c);}
int suffix(std::u16string_view raw,int end,int& rank) {
    rank=0;if(end==static_cast<int>(raw.size()))return end;
    char16_t c=raw[end];
    if(c==u';'){rank=2;return end+1;}
    if(c==u'\''){rank=3;return end+1;}
    if(!digit(c))return end;
    int finish=end;while(finish<static_cast<int>(raw.size()) && digit(raw[finish]))++finish;
    if(finish==end+1 && c==u'0'){rank=10;return finish;}
    for(int i=end;i<finish;++i) {
        // .NET char.IsDigit accepts non-ASCII Nd, but invariant int.Parse does not.
        if(raw[i]<u'0' || raw[i]>u'9')throw std::invalid_argument("Non-ASCII sentence selector");
        int n=raw[i]-u'0';
        if(rank>(std::numeric_limits<int>::max()-n)/10)throw std::out_of_range("Sentence selector overflow");
        rank=rank*10+n;
    }
    return finish;
}
std::u16string segmented(std::u16string_view raw,std::shared_ptr<const SentencePathBoundary> b) {
    std::vector<int> ends;for(;b;b=b->previous)ends.push_back(b->rawLength);
    std::reverse(ends.begin(),ends.end());std::u16string result;int start=0;
    for(int end:ends){if(start)result+=u' ';result+=raw.substr(start,end-start);start=end;}
    return result;
}
}
struct SentenceDecoder::Lattice {
    struct ScoreEntry {std::array<std::uint64_t,3> key{};double score=0;};
    // Direct-mapped, exact-key cache: bounded independently of sentence length.
    // Long graphemes bypass it. Owned by the lattice, never shared or global.
    std::unique_ptr<std::array<ScoreEntry,4096>> scores;
    bool learningAffected=false;
    struct Positions {
        int first=0;
        std::deque<Bucket> values;
        explicit Positions(int size):values(size){}
        Bucket& operator[](int absolute){return values.at(static_cast<std::size_t>(absolute-first));}
        const Bucket& operator[](int absolute) const {return values.at(static_cast<std::size_t>(absolute-first));}
        void resize(int absoluteSize){values.resize(static_cast<std::size_t>(absoluteSize-first));}
        void discardBefore(int floor){while(first<floor){values.pop_front();++first;}}
    };
    Positions states;
    // Per-generation evaluations only. Cleared whenever raw changes; not one
    // cached copy per historical position. Shared completed pool is immutable.
    std::map<int,std::shared_ptr<const std::vector<SentenceCandidate>>> evaluated;
    std::map<const SentencePathBoundary*,std::u16string> segments;
    explicit Lattice(int length):states(length+1){states[0].add(State{});}
};
struct SentenceDecoder::Cache {
    std::u16string raw,required;
    int limit=0;
    bool evidence=false;
    std::unique_ptr<Lattice> lattice;
    SentenceDecodeResult result;
    std::shared_ptr<const SentenceLockedPrefix> locked; // owns seed string_views
};
SentenceDecoder::~SentenceDecoder()=default;
void SentenceDecoder::setLearning(std::shared_ptr<const SentenceLearningSnapshot> snapshot,std::u16string mode) {
    std::lock_guard<std::mutex> lock(decodeMutex_);
    if(learning_!=snapshot || learningMode_!=mode)cache_.reset();
    learning_=std::move(snapshot);learningMode_=std::move(mode);
}
void SentenceDecoder::resetDecodeCache(){std::lock_guard<std::mutex> lock(decodeMutex_);cache_.reset();}
void SentenceDecoder::checkCancelled() const {
    if(cancellation_ && cancellation_->load(std::memory_order_relaxed))throw SentenceDecodeCancelled{};
}
SentenceDecoderMemory SentenceDecoder::memoryStatus() const {
    std::lock_guard<std::mutex> lock(decodeMutex_);SentenceDecoderMemory result;
    if(!cache_ || !cache_->lattice)return result;
    result.positions=cache_->lattice->states.values.size();
    for(const auto& b:cache_->lattice->states.values){result.states+=b.values.size();result.stateCapacity+=b.values.capacity();}
    result.stateBytes=result.stateCapacity*sizeof(State);return result;
}
void SentenceDecoder::retainCommittedHistory(std::u16string_view input,int committedRaw) {
    std::lock_guard<std::mutex> lock(decodeMutex_);
    if(!cache_ || !cache_->lattice || committedRaw<=0 || cache_->raw!=normalizeRawCode(input))return;
    int maxCode=1;for(int n:lexicon_->codeLengths())maxCode=std::max(maxCode,n);
    int tail=0;for(auto i=cache_->raw.rbegin();i!=cache_->raw.rend() && (digit(*i) || *i==u';' || *i==u'\'');++i)++tail;
    // Keep all frontier hypotheses and extra history for code/selector lookback.
    // Absolute offsets and the original boundary chains deliberately stay intact.
    const int floor=std::min(committedRaw,static_cast<int>(cache_->raw.size())-maxCode-tail);
    auto& states=cache_->lattice->states;
    if(floor>states.first && floor-states.first>=64)states.discardBefore(floor);
}
SentenceDecodeResult SentenceDecoder::decode(std::u16string_view input,int limit,bool evidence,std::u16string_view required,
    std::shared_ptr<const SentenceLockedPrefix> lockedPrefix,std::shared_ptr<const std::atomic<bool>> cancellation) {
    std::lock_guard<std::mutex> lock(decodeMutex_);
    cancellation_=cancellation.get();
    struct Reset {const std::atomic<bool>*& value;~Reset(){value=nullptr;}} reset{cancellation_};
    checkCancelled();
    auto raw=normalizeRawCode(input);
    if(raw.empty() || !std::any_of(raw.begin(),raw.end(),[](char16_t c){return unicode::isLetter(c)!=0;})){cache_.reset();return {};}
    if(raw.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()-1))throw std::length_error("Sentence raw length");
    const int length=static_cast<int>(raw.size());
    const auto lockedRaw=lockedPrefix?normalizeRawCode(lockedPrefix->rawCode):std::u16string{};
    const int start=static_cast<int>(lockedRaw.size());
    if(lockedPrefix && (lockedRaw.empty() || raw.substr(0,lockedRaw.size())!=lockedRaw)){cache_.reset();return {};}
    if(cache_ && cache_->locked!=lockedPrefix)cache_.reset();
    if(cache_ && cache_->raw==raw && cache_->limit==limit && cache_->evidence==evidence && cache_->required==required) {
        auto result=cache_->result;result.expandedStates=0;return result;
    }
    // Take ownership before mutation. Exceptions/cancellation destroy partially
    // updated state and cannot leave a stale cache entry or publish a half result.
    auto previous=std::move(cache_);auto next=std::make_unique<Cache>();next->locked=lockedPrefix;
    int expanded=0,maxCode=1;for(int n:lexicon_->codeLengths())maxCode=std::max(maxCode,n);
    auto selector=[](char16_t c){return digit(c) || c==u';' || c==u'\'';};
    auto selectorTail=[&](std::u16string_view value){int n=0;for(auto i=value.rbegin();i!=value.rend() && selector(*i);++i)++n;return n;};
    auto fresh=[&] {
        next->lattice=std::make_unique<Lattice>(length);auto& lattice=*next->lattice;
        if(lockedPrefix) {
            lattice.states[0]=Bucket{};State seed;seed.text=lockedPrefix->text;
            std::vector<std::shared_ptr<const SentencePathBoundary>> boundaries;
            for(auto b=lockedPrefix->boundary;b;b=b->previous)boundaries.push_back(b);
            for(auto i=boundaries.rbegin();i!=boundaries.rend();++i)
                seed.boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{
                    seed.boundary,(*i)->textLength,(*i)->rawLength,0,(*i)->codeScore,
                    (*i)->protectsRareCharacter,(*i)->codeLength});
            seed.codeScore=lockedPrefix->boundary?lockedPrefix->boundary->codeScore:0;
            std::size_t offset=0;
            for(const auto& element:wordTextElements(lockedPrefix->text)) {
                checkCancelled();const auto target=std::u16string_view(lockedPrefix->text).substr(offset,element.size());offset+=element.size();
                seed.score+=transition(lattice,seed.previous2,seed.previous1,target)+options_.emittedCharacterReward;
                if(supplement_ && !supplement_->empty()) {
                    double reward=0;seed.supplementState=supplement_->advance(seed.supplementState,target,reward);
                    seed.score+=reward;seed.supplementScore+=reward;
                }
                seed.previous2=seed.previous1;seed.previous1=target;
            }
            seed.mass=seed.score-seed.supplementScore;lattice.states[start].add(std::move(seed));
        }
        expanded=expand(raw,lattice,start);
    };
    if(previous && previous->raw==raw)next->lattice=std::move(previous->lattice);
    else if(previous && previous->lattice && !previous->lattice->learningAffected) {
        const int oldLength=static_cast<int>(previous->raw.size());
        // Whole-input eligibility/reward can include an arbitrarily long numeric
        // selector. Never reuse such a generation as an internal sentence edge.
        const bool safeWhole=lockedPrefix || (static_cast<std::int64_t>(oldLength)>static_cast<std::int64_t>(maxCode)+selectorTail(previous->raw) &&
            static_cast<std::int64_t>(length)>static_cast<std::int64_t>(maxCode)+selectorTail(raw));
        const int from=static_cast<int>(std::max<std::int64_t>(start,static_cast<std::int64_t>(oldLength)+1-maxCode-selectorTail(raw)));
        if(safeWhole && length>oldLength && raw.compare(0,oldLength,previous->raw)==0 &&
           !selector(raw[oldLength]) && from>=previous->lattice->states.first) {
            next->lattice=std::move(previous->lattice);next->lattice->states.resize(length+1);
            next->lattice->evaluated.clear();next->lattice->segments.clear();
            expanded=expand(raw,*next->lattice,from,oldLength);
            // Newly reached learning hints can change global beam ordering. An
            // influenced generation is always reconstructed with fresh order.
            if(next->lattice->learningAffected)next->lattice.reset();
        }else if(safeWhole && length<oldLength && previous->raw.compare(0,length,raw)==0 &&
                 static_cast<std::int64_t>(length)>=static_cast<std::int64_t>(previous->lattice->states.first)+maxCode &&
                 std::all_of(previous->raw.begin()+length,previous->raw.end(),[](char16_t c){return c>=u'a' && c<=u'z';}) &&
                 !selector(raw.back())) {
            next->lattice=std::move(previous->lattice);next->lattice->states.resize(length+1);
            next->lattice->evaluated.clear();next->lattice->segments.clear();
        }
    }
    previous.reset();
    if(!next->lattice)fresh();
    next->result=emit(raw,*next->lattice,limit,expanded,evidence,required);checkCancelled();
    next->raw=std::move(raw);next->required=required;next->limit=limit;next->evidence=evidence;
    cache_=std::move(next);return cache_->result;
}
bool SentenceDecoder::isProperCodePrefix(std::u16string_view raw) const {
    return lexicon_->isProperCodePrefix(normalizeRawCode(raw));
}
bool SentenceDecoder::hasCompleteCandidate(std::u16string_view input,std::u16string_view required,
    std::optional<std::u16string_view> excluded,bool groupEligibleOnly,const SentenceLockedPrefix* lockedPrefix) const {
    auto raw=normalizeRawCode(input);
    if(raw.empty() || !std::any_of(raw.begin(),raw.end(),[](char16_t c){return unicode::isLetter(c)!=0;}))return false;
    if(raw.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()-1))throw std::length_error("Sentence raw length");
    bool firstOnly=groupEligibleOnly && !std::any_of(raw.begin(),raw.end(),[](char16_t c){return digit(c) || c==u';' || c==u'\'';});
    int length=static_cast<int>(raw.size());std::vector<std::set<std::pair<int,int>>> states(length+1);
    int start=0,matchedRequired=0,matchedExcluded=0;
    if(lockedPrefix) {
        const auto lockedRaw=normalizeRawCode(lockedPrefix->rawCode);
        if(lockedRaw.empty() || raw.substr(0,lockedRaw.size())!=lockedRaw)return false;
        const auto count=std::min(required.size(),lockedPrefix->text.size());
        if(required.substr(0,count)!=std::u16string_view(lockedPrefix->text).substr(0,count))return false;
        start=static_cast<int>(lockedRaw.size());matchedRequired=static_cast<int>(count);
        if(excluded)matchedExcluded=excluded->substr(0,lockedPrefix->text.size())==lockedPrefix->text?static_cast<int>(lockedPrefix->text.size()):-1;
        if(start==length)return count==required.size() && (!excluded || matchedExcluded!=static_cast<int>(excluded->size()));
    }
    states[start].emplace(matchedRequired,matchedExcluded);
    for(int position=start;position<length;++position) {
        if(states[position].empty())continue;
        for(int codeLength:lexicon_->codeLengths()) {
            if(codeLength>length-position || (position>0 && (raw[position]==u';' || raw[position]==u'/' || raw[position]==u'[')))continue;
            auto metadata=lexicon_->candidateView(std::u16string_view(raw).substr(position,codeLength));const auto& candidates=*metadata;if(candidates.empty())continue;
            int selected=0;int consumed=suffix(raw,position+codeLength,selected);bool whole=position==0 && consumed==length;
            if(length>1 && consumed-position<2)continue;
            for(const auto& matched:states[position])for(const auto& c:candidates) {
                if(firstOnly && c.rank>1 &&
                   !(options_.allowDuplicateSingleCharacters && c.textElements.size()==1))continue;
                if(selected>0 ? c.rank!=static_cast<unsigned>(selected) :
                    !(c.rank==1 || whole || (options_.allowDuplicateSingleCharacters && c.textElements.size()==1)))continue;
                int nextRequired=matched.first;
                if(static_cast<std::size_t>(nextRequired)<required.size()) {
                    auto size=std::min(c.text.size(),required.size()-nextRequired);
                    if(!size || required.substr(nextRequired,size)!=c.text.substr(0,size))continue;
                    nextRequired+=static_cast<int>(size);
                }
                int nextExcluded=0;
                if(excluded) {
                    nextExcluded=matched.second;
                    if(nextExcluded>=0)nextExcluded=static_cast<std::size_t>(nextExcluded)+c.text.size()<=excluded->size() &&
                        excluded->substr(nextExcluded,c.text.size())==c.text?nextExcluded+static_cast<int>(c.text.size()):-1;
                }
                if(consumed==length && static_cast<std::size_t>(nextRequired)==required.size() &&
                    (!excluded || nextExcluded<0 || static_cast<std::size_t>(nextExcluded)!=excluded->size()))return true;
                states[consumed].emplace(nextRequired,nextExcluded);
            }
        }
    }
    return false;
}
SentenceDecoder::SentenceDecoder(std::shared_ptr<const SentenceLexicon> lexicon,
    std::shared_ptr<const SentenceLanguageModel> model,SentenceDecoderOptions options,
    std::shared_ptr<const MappedSentenceSupplement> supplement,
    std::shared_ptr<const SentenceLexicalPrior> lexicalPrior)
    :lexicon_(std::move(lexicon)),model_(std::move(model)),options_(options),supplement_(std::move(supplement)),
     lexicalPrior_(std::move(lexicalPrior)) {
    if(!lexicon_)throw std::invalid_argument("Sentence decoder needs a lexicon");
    ngram_=dynamic_cast<const SentenceNgram*>(model_.get());
    options_.beamWidth=std::max(1,options_.beamWidth);
    options_.rankPenalty=std::max(0.0,options_.rankPenalty);
    options_.emittedCharacterReward=std::max(0.0,options_.emittedCharacterReward);
    options_.wholeInputSingleCharacterReward=std::max(0.0,options_.wholeInputSingleCharacterReward);
    options_.canonicalCodeReward=model_?std::max(0.0,options_.canonicalCodeReward):0;
    options_.canonicalIsolationFactor=model_?std::clamp(options_.canonicalIsolationFactor,0.0,1.0):1;
    options_.canonicalIsolationMinCodeLength=std::max(2,options_.canonicalIsolationMinCodeLength);
    options_.lexicalPriorWeight=model_?std::max(0.0,options_.lexicalPriorWeight):0;
    options_.lexicalCandidateLimit=std::max(1,options_.lexicalCandidateLimit);
    if(!model_)lexicalPrior_.reset();
}
std::u16string SentenceDecoder::normalizeRawCode(std::u16string_view raw) {
    std::u16string result;result.reserve(raw.size());
    for(char16_t c:raw)if(!unicode::isWhitespace(c))result+=static_cast<char16_t>(c==0x0130?c:unicode::toLower(c));
    return result;
}
double SentenceDecoder::transition(Lattice& lattice,std::u16string_view a,std::u16string_view b,std::u16string_view c) const {
    if(!model_)return 0;
    bool include=true;
    if(!options_.scoreSentenceBoundaries && (a==bos || b==bos || c==eos) && ngram_)include=false;
    if(ngram_ && a.size()<=2 && b.size()<=2 && c.size()<=2) {
        auto pack=[](std::u16string_view s){return (static_cast<std::uint64_t>(s.size()+1)<<32) |
            (s.empty()?0:static_cast<std::uint64_t>(s[0])) |
            (s.size()<2?0:static_cast<std::uint64_t>(s[1])<<16);};
        const std::array<std::uint64_t,3> key{pack(a),pack(b),pack(c)};
        std::uint64_t hash=14695981039346656037ull;
        for(auto k:key){hash^=k;hash*=1099511628211ull;hash^=hash>>32;}
        if(!lattice.scores)lattice.scores=std::make_unique<std::array<Lattice::ScoreEntry,4096>>();
        auto& entry=(*lattice.scores)[hash&4095];
        if(entry.key==key)return entry.score;
        const double score=model_->logProbability(a,b,c,include);
        entry={key,score};return score;
    }
    return model_->logProbability(a,b,c,include);
}
bool SentenceDecoder::observed(Lattice& lattice,std::u16string_view a,std::u16string_view b) const {
    if(!model_)return false;
    if(!ngram_ || a.size()>2 || b.size()>2)return model_->hasObservedBigram(a,b);
    auto pack=[](std::u16string_view s){return (static_cast<std::uint64_t>(s.size()+1)<<32) |
        (s.empty()?0:static_cast<std::uint64_t>(s[0])) | (s.size()<2?0:static_cast<std::uint64_t>(s[1])<<16);};
    // Domain-separated exact keys share the EXISTING 128-KiB score table.
    // Presence is cached, not probability>0: zero-probability records exist.
    const std::array<std::uint64_t,3> key{UINT64_MAX,pack(a),pack(b)};
    std::uint64_t hash=14695981039346656037ull;
    for(auto k:key){hash^=k;hash*=1099511628211ull;hash^=hash>>32;}
    if(!lattice.scores)lattice.scores=std::make_unique<std::array<Lattice::ScoreEntry,4096>>();
    auto& entry=(*lattice.scores)[hash&4095];if(entry.key==key)return entry.score!=0;
    const bool value=model_->hasObservedBigram(a,b);entry={key,value?1.0:0.0};return value;
}
double SentenceDecoder::isolation(Lattice& lattice,std::u16string_view text) const {
    if(options_.isolationRankThreshold<=0 || options_.isolationLambda<=0)return 0;
    // These ranges are individual graphemes when the entire text consists of
    // them. Avoid allocating two vectors and a string per character for every
    // beam candidate. All other Unicode text keeps the original segmentation.
    const bool simple=std::all_of(text.begin(),text.end(),[](char16_t c){
        return (c>=0x4e00 && c<=0x9fff) || (c>=u'a' && c<=u'z') || (c>=u'A' && c<=u'Z');
    });
    std::vector<std::u16string> elements;if(!simple)elements=wordTextElements(text);
    const auto count=simple?text.size():elements.size();
    auto element=[&](std::size_t i)->std::u16string_view{return simple?text.substr(i,1):std::u16string_view(elements[i]);};
    double penalty=0;
    for(std::size_t i=0;i<count;++i) {
        int rank=sentenceCharacterRank(element(i));if(rank<=options_.isolationRankThreshold)continue;
        if(model_ && ((i && observed(lattice,element(i-1),element(i))) ||
            (i+1<count && observed(lattice,element(i),element(i+1)))))continue;
        penalty+=options_.isolationUseLogRank?options_.isolationLambda*std::log(
            std::max(static_cast<double>(rank),static_cast<double>(options_.isolationRankThreshold)+1)/options_.isolationRankThreshold):options_.isolationLambda;
    }
    return penalty;
}
double SentenceDecoder::pathIsolation(Lattice& lattice,std::u16string_view text,
    std::shared_ptr<const SentencePathBoundary> boundary) const {
    if(options_.canonicalIsolationFactor>=1 || !boundary)return isolation(lattice,text);
    if(options_.isolationRankThreshold<=0 || options_.isolationLambda<=0)return 0;
    std::vector<std::shared_ptr<const SentencePathBoundary>> chain;
    for(auto current=boundary;current;current=current->previous)chain.push_back(current);
    std::reverse(chain.begin(),chain.end());
    double penalty=0,previousWeight=0;std::u16string previous;std::size_t textStart=0;
    const auto weight=[&](int rank,double factor) {
        if(rank<=options_.isolationRankThreshold)return 0.0;
        const double base=options_.isolationUseLogRank?options_.isolationLambda*std::log(
            std::max(static_cast<double>(rank),static_cast<double>(options_.isolationRankThreshold)+1)/
            options_.isolationRankThreshold):options_.isolationLambda;
        return base*factor;
    };
    const auto consume=[&](std::u16string_view edge,double factor) {
        for(const auto& current:wordTextElements(edge)) {
            const double currentWeight=weight(sentenceCharacterRank(current),factor);
            const bool linked=!previous.empty() && (previousWeight>0 || currentWeight>0) && observed(lattice,previous,current);
            if(previousWeight>0 && linked)penalty-=previousWeight;
            previousWeight=currentWeight>0 && !linked?currentWeight:0;
            if(previousWeight>0)penalty+=previousWeight;
            previous=current;
        }
    };
    for(const auto& current:chain) {
        const auto textEnd=std::min(text.size(),std::max(textStart,static_cast<std::size_t>(std::max(0,current->textLength))));
        const double factor=current->protectsRareCharacter &&
            current->codeLength>=options_.canonicalIsolationMinCodeLength?options_.canonicalIsolationFactor:1;
        consume(text.substr(textStart,textEnd-textStart),factor);textStart=textEnd;
    }
    if(textStart<text.size())consume(text.substr(textStart),1);
    return penalty;
}
SentenceDecodeResult SentenceDecoder::decodeFull(std::u16string_view input,int candidateLimit,
    bool includeEarlyCommitEvidence,std::u16string_view requiredTextPrefix) const {
    std::lock_guard<std::mutex> lock(decodeMutex_);
    auto raw=normalizeRawCode(input);
    // char.IsLetter operates on UTF-16 code units, not supplementary scalars.
    if(raw.empty() || !std::any_of(raw.begin(),raw.end(),[](char16_t c){return unicode::isLetter(c)!=0;}))return {};
    if(raw.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()-1))throw std::length_error("Sentence raw length");
    Lattice lattice(static_cast<int>(raw.size()));
    int expanded=expand(raw,lattice,0);
    return emit(raw,lattice,candidateLimit,expanded,includeEarlyCommitEvidence,requiredTextPrefix);
}
int SentenceDecoder::expand(std::u16string_view raw,Lattice& lattice,int from,int minimumEnd) const {
    auto& states=lattice.states;int length=static_cast<int>(raw.size()),expanded=0;
    for(int position=from;position<length;++position) {
        checkCancelled();auto& bucket=states[position];bucket.limit(options_.beamWidth,options_.allowDuplicateSingleCharacters || lattice.learningAffected);
        if(bucket.values.empty())continue;
        for(int codeLength:lexicon_->codeLengths()) {
            if(codeLength>length-position)continue;int end=position+codeLength;
            if(position>0 && (raw[position]==u';' || raw[position]==u'/' || raw[position]==u'['))continue;
            auto metadata=lexicon_->candidateView(std::u16string_view(raw).substr(position,codeLength));const auto& candidates=*metadata;if(candidates.empty())continue;
            int selected=0;int consumed=suffix(raw,end,selected);bool whole=position==0 && consumed==length;
            if(consumed<=minimumEnd || (length>1 && consumed-position<2))continue;
            for(const auto& item:bucket.values)for(const auto& c:candidates) {
                if(selected>0 ? c.rank!=static_cast<unsigned>(selected) :
                    !(c.rank==1 || whole || (options_.allowDuplicateSingleCharacters && c.textElements.size()==1)))continue;
                if((expanded&255)==0)checkCancelled();
                State next=item;double supplementAdded=0;
                std::size_t targetOffset=0;
                for(const auto& element:c.textElements) {
                    const auto target=c.text.substr(targetOffset,element.size());
                    targetOffset+=element.size();
                    next.score+=transition(lattice,next.previous2,next.previous1,target);
                    next.score+=options_.emittedCharacterReward;
                    if(supplement_ && !supplement_->empty()) {
                        double reward=0;next.supplementState=supplement_->advance(next.supplementState,target,reward);
                        next.score+=reward;supplementAdded+=reward;
                    }
                    next.previous2=next.previous1;next.previous1=target;
                }
                if(selected==0)next.score-=options_.rankPenalty*c.logRank;
                double wholeReward=whole && selected==0 && c.optimalSingleCharacterCode && c.textElements.size()==1?options_.wholeInputSingleCharacterReward:0;
                next.score+=wholeReward;next.mass=item.mass+(next.score-item.score-supplementAdded-wholeReward);
                const double codeReward=selected==0 && c.primarySingleCharacterCode && c.textElements.size()==1?
                    options_.canonicalCodeReward*codeLength:0;
                next.codeScore=item.codeScore+codeReward;
                next.text+=c.text;next.supplementScore+=supplementAdded;next.rank=std::max(item.rank,static_cast<int>(c.rank));
                const bool directEdge=!item.boundary && position==0 && whole;
                next.source=directEdge?SentenceSourceDirect:SentenceSourceComposed;
                next.directRank=directEdge?static_cast<int>(c.rank):std::numeric_limits<int>::max();
                next.learningPotential=0;
                double chosenLearningReward=0;int chosenLearningRawStart=0,chosenLearningTextStart=0;
                if(!directEdge && learning_ && !learning_->empty()) {
                    // Consider only suffixes with real raw/text boundaries. A DP
                    // maximum prevents overlapping learnt fragments being counted twice.
                    auto start=item.boundary;
                    for(;;) {
                        int rawStart=start?start->rawLength:0,textStart=start?start->textLength:0;
                        auto fragment=std::u16string_view(next.text).substr(textStart);
                        if(learningCharacters(fragment)>16)break;
                        const auto context=learningContext(std::u16string_view(next.text).substr(0,textStart));
                        auto reward=learning_->score(learningMode_,raw.substr(rawStart,consumed-rawStart),fragment,context);
                        double potential=learning_->prefixScore(learningMode_,raw.substr(rawStart,consumed-rawStart),fragment,
                            context);
                        if(potential>0){lattice.learningAffected=true;next.learningPotential=std::max(next.learningPotential,potential);}
                        if(reward>0) {
                            lattice.learningAffected=true;
                            const double candidateLearning=(start?start->learningScore:0)+reward;
                            const double candidateBonus=std::max(item.learningEarlyCommitBonus,learningEarlyContribution(reward));
                            if(candidateLearning>next.learningScore ||
                               (candidateLearning==next.learningScore && candidateBonus>next.learningEarlyCommitBonus)) {
                                next.learningScore=candidateLearning;next.learningEarlyCommitBonus=candidateBonus;
                                chosenLearningReward=reward;chosenLearningRawStart=rawStart;chosenLearningTextStart=textStart;
                            }
                        }
                        if(!start)break;start=start->previous;
                    }
                    next.score+=next.learningScore-item.learningScore;
                }
                next.boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{
                    item.boundary,static_cast<int>(next.text.size()),consumed,next.learningScore,next.codeScore,
                    options_.canonicalIsolationFactor<1 && c.textElements.size()==1 &&
                        (c.primarySingleCharacterCode || selected>0),codeLength,chosenLearningReward,
                    chosenLearningRawStart,chosenLearningTextStart});
                states[consumed].truncated|=bucket.truncated;
                states[consumed].add(std::move(next));++expanded;
            }
        }
    }
    return expanded;
}
void SentenceDecoder::applyFusionOrdering(std::u16string_view raw,std::vector<SentenceCandidate>& candidates) const {
    if(candidates.size()<2)return;
    std::map<std::u16string,std::size_t,std::less<>> base;
    for(std::size_t i=0;i<candidates.size();++i)base.try_emplace(candidates[i].text,i);
    std::vector<SentenceCandidate> direct,composed;
    for(const auto& c:candidates)((c.source&SentenceSourceDirect)!=0?direct:composed).push_back(c);
    std::stable_sort(direct.begin(),direct.end(),[&](const auto& a,const auto& b) {
        if(a.directRank!=b.directRank)return a.directRank<b.directRank;
        return base[a.text]<base[b.text];
    });
    if(direct.empty() || composed.empty()) {
        if(composed.empty())candidates=std::move(direct);
        return;
    }
    std::vector<SentenceCandidate> merged;merged.reserve(candidates.size());
    std::size_t di=0,ci=0;
    while(di<direct.size() && ci<composed.size()) {
        const auto& d=direct[di];const auto& c=composed[ci];
        double directPrefix=0,composedPrefix=0;
        for(std::size_t i=di;i<direct.size();++i)
            directPrefix=std::max(directPrefix,SentenceFusionPreference::signedScore(
                learning_,learningMode_,raw,direct[i].text,c.text));
        for(std::size_t i=ci;i<composed.size();++i)
            composedPrefix=std::max(composedPrefix,-SentenceFusionPreference::signedScore(
                learning_,learningMode_,raw,d.text,composed[i].text));
        bool takeDirect;
        if(directPrefix>0 || composedPrefix>0) {
            if(std::abs(directPrefix-composedPrefix)>1e-12)takeDirect=directPrefix>composedPrefix;
            else takeDirect=base[d.text]<base[c.text];
        } else takeDirect=base[d.text]<base[c.text];
        merged.push_back(takeDirect?direct[di++]:composed[ci++]);
    }
    while(di<direct.size())merged.push_back(direct[di++]);
    while(ci<composed.size())merged.push_back(composed[ci++]);
    candidates=std::move(merged);
}

SentenceDecodeResult SentenceDecoder::emit(std::u16string_view raw,Lattice& lattice,int candidateLimit,int expanded,
    bool includeEarlyCommitEvidence,std::u16string_view requiredTextPrefix) const {
    auto& states=lattice.states;int length=static_cast<int>(raw.size());
    SentenceDecodeResult result;result.rawCode=raw;result.expandedStates=expanded;
    auto& completed=states[length];completed.limit(options_.beamWidth,options_.allowDuplicateSingleCharacters || lattice.learningAffected);
    bool scoreFirst=false;
    auto evaluate=[&](const State& state) {
        const double eosScore=transition(lattice,state.previous2,state.previous1,eos);
        double adjustment=eosScore-pathIsolation(lattice,state.text,state.boundary)+state.codeScore;
        const double confidenceAdjustment=eosScore-isolation(lattice,state.text);
        SentenceCandidate c;c.text=state.text;c.baseScore=state.score-state.learningScore+adjustment;
        const bool direct=(state.source&SentenceSourceDirect)!=0;
        c.learningScore=direct?0:state.learningScore;
        c.finalScore=direct?c.baseScore:c.baseScore+state.learningScore;
        c.confidenceScore=state.mass+confidenceAdjustment;
        const double personalization=std::min(personalizedEarlyCap,
            supplementEarlyContribution(state.supplementScore)+(direct?0:state.learningEarlyCommitBonus));
        c.earlyCommitConfidenceScore=c.confidenceScore+personalization;
        c.supplementScore=state.supplementScore;c.codeScore=state.codeScore;
        c.maxLexiconRank=std::max(1,state.rank);c.source=state.source;c.directRank=state.directRank;c.boundary=state.boundary;
        c.eligibleDuplicateSinglePath=options_.allowDuplicateSingleCharacters &&
            ((c.boundary && c.boundary->previous) || wordTextElements(c.text).size()==1);
        return c;
    };
    // Incomplete-tail evidence never participates in final ranking. Avoid the
    // path-isolation/final-score work performed by evaluate(); confidence uses
    // the same EOS and isolation terms as the full candidate path.
    const auto evaluateEvidence=[&](const State& state) {
        SentenceCandidate c;c.text=state.text;c.boundary=state.boundary;
        const double eosScore=transition(lattice,state.previous2,state.previous1,eos);
        c.confidenceScore=state.mass+eosScore-isolation(lattice,state.text);
        const bool direct=(state.source&SentenceSourceDirect)!=0;
        const double personalization=std::min(personalizedEarlyCap,
            supplementEarlyContribution(state.supplementScore)+(direct?0:state.learningEarlyCommitBonus));
        c.earlyCommitConfidenceScore=c.confidenceScore+personalization;
        return c;
    };
    auto cached=lattice.evaluated.find(length);
    if(cached==lattice.evaluated.end()) {
        auto all=std::make_shared<std::vector<SentenceCandidate>>();all->reserve(completed.values.size());
        for(const auto& state:completed.values) {
            checkCancelled();auto c=evaluate(state);
            if(c.learningScore>0 || (options_.allowDuplicateSingleCharacters && c.boundary && c.boundary->previous))scoreFirst=true;
            all->push_back(std::move(c));
        }
        auto order=[=](const SentenceCandidate& a,const SentenceCandidate& b) {
            if(scoreFirst && a.finalScore!=b.finalScore)return a.finalScore>b.finalScore;
            if(a.maxLexiconRank!=b.maxLexiconRank)return a.maxLexiconRank<b.maxLexiconRank;
            if(a.finalScore!=b.finalScore)return a.finalScore>b.finalScore;
            return a.text<b.text;
        };
        std::sort(all->begin(),all->end(),order);
        cached=lattice.evaluated.emplace(length,std::move(all)).first;
    }
    result.confidenceCandidates=cached->second;
    const auto& all=*result.confidenceCandidates;
    result.candidates.assign(all.begin(),all.begin()+std::min(all.size(),static_cast<std::size_t>(std::max(1,candidateLimit))));
    for(auto& c:result.candidates) {
        auto it=lattice.segments.find(c.boundary.get());
        if(it==lattice.segments.end())it=lattice.segments.emplace(c.boundary.get(),segmented(raw,c.boundary)).first;
        c.segmentedCode=it->second;
    }
    if(result.candidates.size()>1 && lexicalPrior_ && options_.lexicalPriorWeight>0) {
        std::map<std::u16string,bool,std::less<>> lookupCache;
        const auto lexicalLimit=std::min(result.candidates.size(),static_cast<std::size_t>(options_.lexicalCandidateLimit));
        for(std::size_t i=0;i<lexicalLimit;++i) {
            auto& candidate=result.candidates[i];candidate.lexicalScore=lexicalPrior_->score(candidate.text,&lookupCache)*options_.lexicalPriorWeight;
            candidate.baseScore+=candidate.lexicalScore;candidate.finalScore+=candidate.lexicalScore;
        }
        const bool lexicalScoreFirst=std::any_of(all.begin(),all.end(),[&](const SentenceCandidate& candidate) {
            return candidate.learningScore>0 || (options_.allowDuplicateSingleCharacters && candidate.boundary && candidate.boundary->previous);
        });
        std::sort(result.candidates.begin(),result.candidates.end(),[=](const SentenceCandidate& a,const SentenceCandidate& b) {
            if(lexicalScoreFirst && a.finalScore!=b.finalScore)return a.finalScore>b.finalScore;
            if(a.maxLexiconRank!=b.maxLexiconRank)return a.maxLexiconRank<b.maxLexiconRank;
            if(a.finalScore!=b.finalScore)return a.finalScore>b.finalScore;
            return a.text<b.text;
        });
    }
    applyFusionOrdering(raw,result.candidates);
    result.learningAffected=lattice.learningAffected;result.learningMode=learningMode_;
    result.earlyCommitEvidence.confidenceTruncated=completed.truncated;
    if(includeEarlyCommitEvidence && (!completed.truncated || options_.preserveTruncatedEarlyCommitEvidence)) {
        auto& evidence=result.earlyCommitEvidence;
        evidence.confidenceTruncated=completed.truncated;
        auto required=[&](std::u16string_view text) {
            return requiredTextPrefix.empty() || (!text.empty() && text.substr(0,requiredTextPrefix.size())==requiredTextPrefix);
        };
        using Key=std::pair<std::u16string_view,int>;
        std::vector<const SentenceCandidate*> visible;std::vector<SentenceCandidate> pool;
        std::map<Key,std::size_t> poolIndex;
        auto add=[&](const SentenceCandidate& c) {
            if(c.text.empty())return;
            auto [it,inserted]=poolIndex.emplace(Key{c.text,c.boundary?c.boundary->rawLength:0},pool.size());
            if(inserted){pool.push_back(c);return;}
            auto& old=pool[it->second];
            double top=std::max(old.confidenceScore,c.confidenceScore);
            double combined=top+std::log(std::exp(old.confidenceScore-top)+std::exp(c.confidenceScore-top));
            const double oldEarly=earlyScore(old),newEarly=earlyScore(c);
            double earlyTop=std::max(oldEarly,newEarly);
            double combinedEarly=earlyTop+std::log(std::exp(oldEarly-earlyTop)+std::exp(newEarly-earlyTop));
            if(newEarly>oldEarly)old=c;
            old.confidenceScore=combined;old.earlyCommitConfidenceScore=combinedEarly;
        };
        for(const auto& c:all)if(required(c.text)){visible.push_back(&c);add(c);}
        int maxCode=1;for(int n:lexicon_->codeLengths())maxCode=std::max(maxCode,n);
        for(int tailLength=1;tailLength<=std::min(maxCode-1,length-1);++tailLength) {
            int consumed=length-tailLength;auto tail=std::u16string_view(raw).substr(consumed);
            if(!std::all_of(tail.begin(),tail.end(),[](char16_t c){return unicode::isLetter(c)!=0;}) ||
                !lexicon_->isProperCodePrefix(tail) || (tailLength>=2 && !lexicon_->candidateView(tail)->empty()))continue;
            auto& partial=states[consumed];partial.limit(options_.beamWidth,options_.allowDuplicateSingleCharacters);
            bool added=false;
            auto found=lattice.evaluated.find(consumed);
            if(found==lattice.evaluated.end()) {
                auto evaluated=std::make_shared<std::vector<SentenceCandidate>>();evaluated->reserve(partial.values.size());
                for(const auto& state:partial.values){checkCancelled();evaluated->push_back(evaluateEvidence(state));}
                found=lattice.evaluated.emplace(consumed,std::move(evaluated)).first;
            }
            for(const auto& c:*found->second){if(!required(c.text))continue;add(c);added=true;}
            if(added){evidence.mergedIncompleteTail=true;evidence.confidenceTruncated|=partial.truncated;}
        }
        evidence.neutralIncompleteTail=visible.empty() && evidence.mergedIncompleteTail;
        // Preserve retained prefix mass when the product opts into the
        // strong-truncated policy. The engine still requires model-only strong
        // evidence and the current generation before committing it.
        if(evidence.confidenceTruncated && !options_.preserveTruncatedEarlyCommitEvidence)return result;
        if(!visible.empty()) {
            double maximum=visible.front()->confidenceScore,total=0;
            for(const auto* c:visible)maximum=std::max(maximum,c->confidenceScore);
            for(const auto* c:visible)total+=std::exp(c->confidenceScore-maximum);
            evidence.neutralLowConfidence=total>0 && 1/total<0.99;
        }
        if(!pool.empty()) {
            double baseMaximum=pool.front().confidenceScore,earlyMaximum=earlyScore(pool.front());
            for(const auto& c:pool){baseMaximum=std::max(baseMaximum,c.confidenceScore);earlyMaximum=std::max(earlyMaximum,earlyScore(c));}
            double baseTotal=0,earlyTotal=0;
            struct Mass {Key key;double base=0,early=0;};
            struct PrefixHash {
                std::size_t operator()(const Key& key) const noexcept {
                    auto h=std::hash<std::u16string_view>{}(key.first);
                    return h^(static_cast<std::size_t>(key.second)+0x9e3779b97f4a7c15ull+(h<<6)+(h>>2));
                }
            };
            std::unordered_map<Key,std::size_t,PrefixHash> massIndex;std::vector<Mass> mass;
            massIndex.reserve(std::min<std::size_t>(65536,pool.size()*8+1));
            std::vector<double> boundaryMass(static_cast<std::size_t>(length)+1,0.0);
            for(const auto& c:pool) {
                checkCancelled();
                double baseWeight=std::exp(c.confidenceScore-baseMaximum);
                double earlyWeight=std::exp(earlyScore(c)-earlyMaximum);
                baseTotal+=baseWeight;earlyTotal+=earlyWeight;
                for(auto b=c.boundary;b;b=b->previous)if(b->textLength>0 && b->textLength<=static_cast<int>(c.text.size())) {
                    Key key{std::u16string_view(c.text).substr(0,b->textLength),b->rawLength};
                    auto [it,inserted]=massIndex.emplace(key,mass.size());
                    if(inserted)mass.push_back({key,0,0});
                    mass[it->second].base+=baseWeight;mass[it->second].early+=earlyWeight;
                    if(b->rawLength>=0 && b->rawLength<=length)boundaryMass[static_cast<std::size_t>(b->rawLength)]+=baseWeight;
                }
            }
            if(baseTotal>0 && earlyTotal>0)for(const auto& item:mass) {
                double boundaryShare=item.key.second>=0 && item.key.second<=length?boundaryMass[static_cast<std::size_t>(item.key.second)]/baseTotal:0;
                evidence.prefixes.push_back({std::u16string(item.key.first),item.key.second,item.early/earlyTotal,
                    boundaryShare,boundaryShare>=0.99999,item.base/baseTotal});
            }
        }
        const SentencePrefixEvidence* longest=nullptr;std::size_t longestElements=0;
        std::map<std::u16string_view,const SentencePrefixEvidence*> closed;
        for(const auto& prefix:evidence.prefixes)if(prefix.boundaryClosed) {
            auto found=closed.find(prefix.text);
            if(found==closed.end() || prefix.share>found->second->share ||
                (prefix.share==found->second->share && prefix.rawLength<found->second->rawLength))closed[prefix.text]=&prefix;
            if(prefix.share<0.99)continue;
            auto count=wordTextElements(prefix.text).size();
            if(!longest || count>longestElements || (count==longestElements &&
                (prefix.share>longest->share || (prefix.share==longest->share && prefix.rawLength<longest->rawLength)))) {
                longest=&prefix;longestElements=count;
            }
        }
        if(longest){evidence.proposal=longest->text;evidence.proposalShare=longest->share;}
        for(const auto& item:closed)evidence.rawLengths.emplace(item.first,item.second->rawLength);
    }
    return result;
}
}
