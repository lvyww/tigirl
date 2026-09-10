#include "SentenceDecoder.h"
#include "SentenceCharacterRanks.h"
#include "Grapheme.h"
#include "Unicode.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
namespace tiger {
namespace {
constexpr std::u16string_view bos=u"\x02",eos=u"\x03";
struct State {
    double score=0,mass=0,supplementScore=0;
    std::u16string text;
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
        // Duplicate representatives always prefer rank, including score-first beams.
        if(item.rank<old.rank || (item.rank==old.rank && item.score>old.score))old=std::move(item);
        old.mass=combined;
    }
    void limit(int width,bool scoreFirst) {
        if(frozen)return;
        frozen=true;
        auto order=[=](const State& a,const State& b){return better(a,b,scoreFirst);};
        if(values.size()>static_cast<std::size_t>(width)) {
            truncated=true;
            std::partial_sort(values.begin(),values.begin()+width,values.end(),order);
            values.resize(width);
        }else std::sort(values.begin(),values.end(),order);
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
    std::vector<Bucket> states;
    explicit Lattice(int length):states(length+1){states[0].add(State{});}
};
struct SentenceDecoder::Cache {
    std::u16string raw,required;
    int limit=0;
    bool evidence=false;
    std::unique_ptr<Lattice> lattice;
    SentenceDecodeResult result;
};
SentenceDecoder::~SentenceDecoder()=default;
void SentenceDecoder::resetDecodeCache(){std::lock_guard<std::mutex> lock(decodeMutex_);cache_.reset();}
SentenceDecodeResult SentenceDecoder::decode(std::u16string_view input,int limit,bool evidence,std::u16string_view required,
    std::shared_ptr<const SentenceLockedPrefix> lockedPrefix) {
    std::lock_guard<std::mutex> lock(decodeMutex_);
    auto raw=normalizeRawCode(input);
    if(raw.empty() || !std::any_of(raw.begin(),raw.end(),[](char16_t c){return unicode::isLetter(c)!=0;})){cache_.reset();return {};}
    if(raw.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()-1))throw std::length_error("Sentence raw length");
    if(lockedPrefix) {
        // Decode only the tail. Reconstruct model and supplement context from
        // the chosen text; no lattice edge may cross its fixed raw boundary.
        const auto lockedRaw=normalizeRawCode(lockedPrefix->rawCode);
        if(lockedRaw.empty() || raw.substr(0,lockedRaw.size())!=lockedRaw)return {};
        Lattice lattice(static_cast<int>(raw.size()));lattice.states[0]=Bucket{};
        State seed;seed.text=lockedPrefix->text;seed.boundary=lockedPrefix->boundary;
        std::size_t offset=0;
        for(const auto& element:wordTextElements(lockedPrefix->text)) {
            const auto target=std::u16string_view(lockedPrefix->text).substr(offset,element.size());offset+=element.size();
            seed.score+=transition(lattice,seed.previous2,seed.previous1,target)+options_.emittedCharacterReward;
            if(supplement_ && !supplement_->empty()) {
                double reward=0;seed.supplementState=supplement_->advance(seed.supplementState,target,reward);
                seed.score+=reward;seed.supplementScore+=reward;
            }
            seed.previous2=seed.previous1;seed.previous1=target;
        }
        seed.mass=seed.score-seed.supplementScore;
        lattice.states[lockedRaw.size()].add(std::move(seed));
        const auto expanded=expand(raw,lattice,static_cast<int>(lockedRaw.size()));
        return emit(raw,lattice,limit,expanded,evidence,required);
    }
    if(cache_ && cache_->raw==raw && cache_->limit==limit && cache_->evidence==evidence && cache_->required==required)return cache_->result;
    int length=static_cast<int>(raw.size()),expanded=0;
    auto next=std::make_unique<Cache>();
    if(cache_ && cache_->raw==raw)next->lattice=std::move(cache_->lattice);
    else if(cache_ && cache_->lattice && cache_->raw.size()>4 && length>4) {
        int oldLength=static_cast<int>(cache_->raw.size());
        if(length>oldLength && raw.compare(0,oldLength,cache_->raw)==0) {
            int maxCode=1;for(int n:lexicon_->codeLengths())maxCode=std::max(maxCode,n);
            int tail=0;for(int i=length-1;i>=0 && (digit(raw[i]) || raw[i]==u';' || raw[i]==u'\'');--i)++tail;
            next->lattice=std::move(cache_->lattice);
            auto& states=next->lattice->states;states.resize(length+1);
            for(int i=oldLength+1;i<=length;++i)states[i]=Bucket{};
            expanded=expand(raw,*next->lattice,std::max(0,oldLength+1-maxCode-tail),oldLength);
        }else if(length<oldLength && cache_->raw.compare(0,length,raw)==0) {
            next->lattice=std::move(cache_->lattice);next->lattice->states.resize(length+1);
        }
    }
    if(!next->lattice){next->lattice=std::make_unique<Lattice>(length);expanded=expand(raw,*next->lattice,0);}
    next->result=emit(raw,*next->lattice,limit,expanded,evidence,required);
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
            auto candidates=lexicon_->candidates(std::u16string_view(raw).substr(position,codeLength));if(candidates.empty())continue;
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
    std::shared_ptr<const MappedSentenceSupplement> supplement)
    :lexicon_(std::move(lexicon)),model_(std::move(model)),options_(options),supplement_(std::move(supplement)) {
    if(!lexicon_)throw std::invalid_argument("Sentence decoder needs a lexicon");
    ngram_=dynamic_cast<const SentenceNgram*>(model_.get());
    options_.beamWidth=std::max(1,options_.beamWidth);
    options_.rankPenalty=std::max(0.0,options_.rankPenalty);
    options_.emittedCharacterReward=std::max(0.0,options_.emittedCharacterReward);
    options_.wholeInputSingleCharacterReward=std::max(0.0,options_.wholeInputSingleCharacterReward);
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
double SentenceDecoder::isolation(std::u16string_view text) const {
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
        if(model_ && ((i && model_->hasObservedBigram(element(i-1),element(i))) ||
            (i+1<count && model_->hasObservedBigram(element(i),element(i+1)))))continue;
        penalty+=options_.isolationUseLogRank?options_.isolationLambda*std::log(
            std::max(static_cast<double>(rank),static_cast<double>(options_.isolationRankThreshold)+1)/options_.isolationRankThreshold):options_.isolationLambda;
    }
    return penalty;
}
SentenceDecodeResult SentenceDecoder::decodeFull(std::u16string_view input,int candidateLimit,
    bool includeEarlyCommitEvidence,std::u16string_view requiredTextPrefix) const {
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
        auto& bucket=states[position];bucket.limit(options_.beamWidth,options_.allowDuplicateSingleCharacters);
        if(bucket.values.empty())continue;
        for(int codeLength:lexicon_->codeLengths()) {
            int end=position+codeLength;if(end>length)continue;
            if(position>0 && (raw[position]==u';' || raw[position]==u'/' || raw[position]==u'['))continue;
            auto candidates=lexicon_->candidates(std::u16string_view(raw).substr(position,codeLength));if(candidates.empty())continue;
            int selected=0;int consumed=suffix(raw,end,selected);bool whole=position==0 && consumed==length;
            if(consumed<=minimumEnd || (length>1 && consumed-position<2))continue;
            for(const auto& item:bucket.values)for(const auto& c:candidates) {
                if(selected>0 ? c.rank!=static_cast<unsigned>(selected) :
                    !(c.rank==1 || whole || (options_.allowDuplicateSingleCharacters && c.textElements.size()==1)))continue;
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
                next.text+=c.text;next.supplementScore+=supplementAdded;next.rank=std::max(item.rank,static_cast<int>(c.rank));
                next.boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{item.boundary,static_cast<int>(next.text.size()),consumed});
                states[consumed].add(std::move(next));++expanded;
            }
        }
    }
    return expanded;
}
SentenceDecodeResult SentenceDecoder::emit(std::u16string_view raw,Lattice& lattice,int candidateLimit,int expanded,
    bool includeEarlyCommitEvidence,std::u16string_view requiredTextPrefix) const {
    auto& states=lattice.states;int length=static_cast<int>(raw.size());
    SentenceDecodeResult result;result.rawCode=raw;result.expandedStates=expanded;
    auto& completed=states[length];completed.limit(options_.beamWidth,options_.allowDuplicateSingleCharacters);
    bool scoreFirst=false;
    auto evaluate=[&](const State& state) {
        double adjustment=transition(lattice,state.previous2,state.previous1,eos)-isolation(state.text);
        SentenceCandidate c;c.text=state.text;c.baseScore=c.finalScore=state.score+adjustment;
        c.confidenceScore=state.mass+adjustment;c.supplementScore=state.supplementScore;c.maxLexiconRank=std::max(1,state.rank);c.boundary=state.boundary;
        c.eligibleDuplicateSinglePath=options_.allowDuplicateSingleCharacters &&
            ((c.boundary && c.boundary->previous) || wordTextElements(c.text).size()==1);
        return c;
    };
    for(const auto& state:completed.values) {
        auto c=evaluate(state);
        if(options_.allowDuplicateSingleCharacters && c.boundary && c.boundary->previous)scoreFirst=true;
        result.candidates.push_back(std::move(c));
    }
    auto order=[=](const SentenceCandidate& a,const SentenceCandidate& b) {
        if(scoreFirst && a.finalScore!=b.finalScore)return a.finalScore>b.finalScore;
        if(a.maxLexiconRank!=b.maxLexiconRank)return a.maxLexiconRank<b.maxLexiconRank;
        if(a.finalScore!=b.finalScore)return a.finalScore>b.finalScore;
        return a.text<b.text;
    };
    std::sort(result.candidates.begin(),result.candidates.end(),order);
    if(result.candidates.size()>static_cast<std::size_t>(std::max(1,candidateLimit)))result.candidates.resize(std::max(1,candidateLimit));
    for(auto& c:result.candidates)c.segmentedCode=segmented(raw,c.boundary);
    if(includeEarlyCommitEvidence) {
        auto& evidence=result.earlyCommitEvidence;
        evidence.confidenceTruncated=completed.truncated;
        auto required=[&](std::u16string_view text) {
            return requiredTextPrefix.empty() || (!text.empty() && text.substr(0,requiredTextPrefix.size())==requiredTextPrefix);
        };
        using Key=std::pair<std::u16string,int>;
        std::vector<SentenceCandidate> visible,pool;
        std::map<Key,std::size_t> poolIndex;
        auto add=[&](const SentenceCandidate& c) {
            if(c.text.empty())return;
            auto [it,inserted]=poolIndex.emplace(Key{c.text,c.boundary?c.boundary->rawLength:0},pool.size());
            if(inserted){pool.push_back(c);return;}
            auto& old=pool[it->second];double top=std::max(old.confidenceScore,c.confidenceScore);
            double combined=top+std::log(std::exp(old.confidenceScore-top)+std::exp(c.confidenceScore-top));
            if(c.confidenceScore>old.confidenceScore)old=c;
            old.confidenceScore=combined;
        };
        for(const auto& c:result.candidates)if(required(c.text)){visible.push_back(c);add(c);}
        int maxCode=1;for(int n:lexicon_->codeLengths())maxCode=std::max(maxCode,n);
        for(int tailLength=1;tailLength<=std::min(maxCode-1,length-1);++tailLength) {
            int consumed=length-tailLength;auto tail=std::u16string_view(raw).substr(consumed);
            if(!std::all_of(tail.begin(),tail.end(),[](char16_t c){return unicode::isLetter(c)!=0;}) ||
                !lexicon_->isProperCodePrefix(tail) || (tailLength>=2 && !lexicon_->candidates(tail).empty()))continue;
            auto& partial=states[consumed];partial.limit(options_.beamWidth,options_.allowDuplicateSingleCharacters);
            bool added=false;
            for(const auto& state:partial.values) {
                auto c=evaluate(state);if(!required(c.text))continue;add(c);added=true;
            }
            if(added){evidence.mergedIncompleteTail=true;evidence.confidenceTruncated|=partial.truncated;}
        }
        evidence.neutralIncompleteTail=visible.empty() && evidence.mergedIncompleteTail;
        if(!visible.empty()) {
            double maximum=visible.front().confidenceScore,total=0;
            for(const auto& c:visible)maximum=std::max(maximum,c.confidenceScore);
            for(const auto& c:visible)total+=std::exp(c.confidenceScore-maximum);
            evidence.neutralLowConfidence=total>0 && 1/total<0.995;
        }
        if(!pool.empty()) {
            double maximum=pool.front().confidenceScore,total=0;
            for(const auto& c:pool)maximum=std::max(maximum,c.confidenceScore);
            std::map<Key,std::size_t> massIndex;std::vector<std::pair<Key,double>> mass;
            std::map<int,double> boundaryMass;
            for(const auto& c:pool) {
                double weight=std::exp(c.confidenceScore-maximum);total+=weight;std::set<int> boundaries;
                for(auto b=c.boundary;b;b=b->previous)if(b->textLength>0 && b->textLength<=static_cast<int>(c.text.size())) {
                    Key key{c.text.substr(0,b->textLength),b->rawLength};
                    auto [it,inserted]=massIndex.emplace(key,mass.size());
                    if(inserted)mass.push_back({std::move(key),0});
                    mass[it->second].second+=weight;boundaries.insert(b->rawLength);
                }
                for(int b:boundaries)boundaryMass[b]+=weight;
            }
            if(total>0)for(const auto& item:mass) {
                double boundaryShare=boundaryMass[item.first.second]/total;
                evidence.prefixes.push_back({item.first.first,item.first.second,item.second/total,boundaryShare,boundaryShare>=0.99999});
            }
        }
        const SentencePrefixEvidence* longest=nullptr;std::size_t longestElements=0;
        std::map<std::u16string,const SentencePrefixEvidence*> closed;
        for(const auto& prefix:evidence.prefixes)if(prefix.boundaryClosed) {
            auto found=closed.find(prefix.text);
            if(found==closed.end() || prefix.share>found->second->share ||
                (prefix.share==found->second->share && prefix.rawLength<found->second->rawLength))closed[prefix.text]=&prefix;
            if(prefix.share<0.995)continue;
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
