#include "SentenceSession.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include "Unicode.h"
namespace tiger {
namespace {
std::atomic<std::uint64_t> nextIdentity{1};
bool starts(std::u16string_view text,std::u16string_view prefix){return text.substr(0,prefix.size())==prefix;}
std::u16string trimAfter(std::u16string_view segmented,int count) {
    std::size_t i=0;for(;i<segmented.size() && count>0;++i)if(segmented[i]!=u' ')--count;
    while(i<segmented.size() && segmented[i]==u' ')++i;
    return std::u16string(segmented.substr(i));
}
}
SentenceSession::SentenceSession():identity_(nextIdentity.fetch_add(1,std::memory_order_relaxed)){}
void SentenceSession::clear() {
    learningBaseline_.reset();pendingLearning_.clear();readyLearning_.clear();
    resetAutomaticState();lastAutoCommitRaw_=0;tabPending_=false;locks_.clear();
    raw_.clear();committed_.clear();committedRaw_=selected_=0;suspended_=continuation_=hasResult_=false;
    result_=std::make_shared<SentenceDecodeResult>();++generation_;
}
void SentenceSession::start(std::u16string raw,std::uint64_t resources,bool continuation) {
    clear();resources_=resources;raw_=std::move(raw);continuation_=continuation;edited();
}
void SentenceSession::invalidatePending(bool discardResult){
    resetAutomaticState();
    if(discardResult){hasResult_=false;result_=std::make_shared<SentenceDecodeResult>();}
    if(active())edited();
}
void SentenceSession::edited(){learningBaseline_.reset();selected_=0;++generation_;}
bool SentenceSession::append(char16_t code) {
    if(raw_.size()-committedRaw_>=128)return false;
    raw_+=static_cast<char16_t>(code==0x0130?code:unicode::toLower(code));edited();return true;
}
void SentenceSession::backspace() {
    resetAutomaticState();
    if(raw_.size()<=static_cast<std::size_t>(committedRaw_+1)){clear();return;}
    raw_.pop_back();tabPending_=false;learningBaseline_.reset();
    pendingLearning_.erase(std::remove_if(pendingLearning_.begin(),pendingLearning_.end(),[&](const auto& e){
        return static_cast<int>(raw_.size())<=e.rawEnd;
    }),pendingLearning_.end());
    while(!locks_.empty() && locks_.back()->rawCode.size()>static_cast<std::size_t>(committedRaw_) && raw_.size()<=locks_.back()->rawCode.size())locks_.pop_back();
    edited();
}
void SentenceSession::changeResources(std::uint64_t resources) {
    if(resources_==resources)return;
    resetAutomaticState();
    tabPending_=false;locks_.clear();learningBaseline_.reset();pendingLearning_.clear();readyLearning_.clear();
    resources_=resources;hasResult_=false;result_=std::make_shared<SentenceDecodeResult>();edited();
}
std::optional<SentenceDecodeTicket> SentenceSession::request() const {
    if(!active())return {};
    return SentenceDecodeTicket{identity_,generation_,resources_,raw_,committed_,activeLock(),committedRaw_};
}
bool SentenceSession::matches(const SentenceDecodeTicket& ticket) const {
    return active() && ticket.session==identity_ && ticket.generation==generation_ && ticket.resources==resources_ &&
        ticket.raw==raw_ && ticket.requiredPrefix==committed_ && ticket.lockedPrefix==activeLock() && ticket.committedRaw==committedRaw_;
}
bool SentenceSession::current() const {
    return active() && hasResult_ && appliedGeneration_==generation_ && resultResources_==resources_ && result_->rawCode==raw_;
}
bool SentenceSession::apply(const SentenceDecodeTicket& ticket,SentenceDecodeResult result) {
    if(!matches(ticket) || current())return false;
    // Failure/empty results must still consume the request generation. Raw
    // identity retains original casing; the decoder normalizes only lookup.
    result.rawCode=ticket.raw;
    bool implicit=continuation_ && !std::any_of(raw_.begin(),raw_.end(),[](char16_t c){return unicode::isDecimalDigit(c) || c==u';' || c==u'\'';});
    auto& list=result.candidates;
    list.erase(std::remove_if(list.begin(),list.end(),[&](const SentenceCandidate& c){
        // Segmented paths already passed the decoder's duplicate-single,
        // optimal-code and word-rank rules. Do not discard them merely because
        // an earlier segment was automatically committed (e.g. 反 + 刍).
        // With duplicate singles disabled the decoder emits no such implicit
        // paths. Whole-input non-first edges remain hidden on continuation.
        const bool segmented=c.boundary && c.boundary->previous && c.boundary->previous->textLength>0;
        return !starts(c.text,committed_) || (implicit && c.maxLexiconRank>1 && !segmented);
    }),list.end());
    result_=std::make_shared<SentenceDecodeResult>(std::move(result));appliedGeneration_=generation_;resultResources_=resources_;hasResult_=true;selected_=0;return true;
}
void SentenceSession::moveSelection(int delta,int pageSize,bool tab) {
    if(tab && !tabPending_ && current() && !result_->candidates.empty())learningBaseline_=result_->candidates.front();
    if(tab)tabPending_=true;
    resetEmptyCodePending();
    suspended_=true;
    int count=std::min(static_cast<int>(result_->candidates.size()),std::clamp(pageSize,1,10));
    if(count>0)selected_=((selected_+delta)%count+count)%count;
}
std::u16string SentenceSession::liveRaw() const {return raw_.substr(committedRaw_);}
std::u16string SentenceSession::candidateText(int index) const {
    if(index<0 || index>=static_cast<int>(result_->candidates.size()) || resultResources_!=resources_)return {};
    const auto& text=result_->candidates[index].text;
    return starts(text,committed_)?text.substr(committed_.size()):std::u16string{};
}
std::optional<std::u16string> SentenceSession::commitCandidate(int index) {
    // The adapter must wait for the current generation before selection.
    if(!current() || index<0 || index>=static_cast<int>(result_->candidates.size()))return {};
    captureFusionLearning(index);
    captureLearning(index);
    auto chosen=result_->candidates[index];auto events=learningThrough(chosen.text,static_cast<int>(raw_.size()));
    if(index==0 && events.empty())events=reinforceLearning(chosen,static_cast<int>(raw_.size()));
    auto text=candidateText(index);clear();readyLearning_=std::move(events);return text;
}
std::u16string SentenceSession::commitWithSuffix(std::u16string_view suffix) {
    auto text=current() && selected_<static_cast<int>(result_->candidates.size())?candidateText(selected_):liveRaw();
    std::vector<SentenceLearningEvent> events;
    if(current() && selected_<static_cast<int>(result_->candidates.size())) {
        captureFusionLearning(selected_);
        captureLearning(selected_);const auto chosen=result_->candidates[selected_];
        events=learningThrough(chosen.text,static_cast<int>(raw_.size()));
        if(selected_==0 && events.empty())events=reinforceLearning(chosen,static_cast<int>(raw_.size()));
    }
    text+=suffix;clear();readyLearning_=std::move(events);return text;
}
std::u16string SentenceSession::commitRaw(bool clearOnly) {auto text=clearOnly?std::u16string{}:liveRaw();clear();return text;}
std::optional<std::u16string> SentenceSession::commitPrefix(std::u16string_view text,int rawLength) {
    if(!current() || rawLength<=committedRaw_ || rawLength>static_cast<int>(raw_.size()) ||
        text.size()<=committed_.size() || !starts(text,committed_))return {};
    bool boundary=false;
    for(const auto& c:result_->candidates)if(starts(c.text,text))for(auto b=c.boundary;b;b=b->previous)
        if(b->rawLength==rawLength && b->textLength==static_cast<int>(text.size()))boundary=true;
    if(!boundary)return {};
    return applyPrefix(text,rawLength);
}
std::u16string SentenceSession::applyPrefix(std::u16string_view text,int rawLength) {
    auto events=learningThrough(text,rawLength);readyLearning_.insert(readyLearning_.end(),events.begin(),events.end());
    auto commit=std::u16string(text.substr(committed_.size()));committed_=text;committedRaw_=rawLength;
    auto filtered=std::make_shared<SentenceDecodeResult>(*result_);
    auto& list=filtered->candidates;list.erase(std::remove_if(list.begin(),list.end(),[&](const SentenceCandidate& c){return !starts(c.text,committed_);}),list.end());
    result_=std::move(filtered);selected_=0;return commit;
}
std::optional<std::u16string> SentenceSession::tryAutoCommit(bool enabled,int minimumRetained) {
    if(result_->learningAffected && result_->earlyCommitEvidence.confidenceTruncated){resetAutomaticState();return {};}
    SentenceAutoCommitInput input;
    input.enabled=enabled;input.suspended=suspended_;input.matchingLexicon=hasResult_ && resultResources_==resources_;
    input.raw=raw_;input.committedText=committed_;input.committedRaw=committedRaw_;
    input.lastAutoCommitRaw=lastAutoCommitRaw_;input.configuredRetained=minimumRetained;
    auto decision=autoCommit_.evaluate(input,*result_);if(!decision)return {};
    const bool wasCurrent=current();
    auto commit=applyPrefix(decision->text,decision->rawLength);
    lastAutoCommitRaw_=decision->rawLength;continuation_=false;
    ++generation_;if(wasCurrent)appliedGeneration_=generation_;
    return commit;
}
void SentenceSession::resetEmptyCodePending(){emptyCodePending_.reset();}
void SentenceSession::resetAutomaticState(){autoCommit_.reset();resetEmptyCodePending();}
std::optional<std::u16string> SentenceSession::appendAutomatic(char16_t code,bool enabled,int minimumRetained,
    const SentencePathQueries& queries) {
    if(raw_.size()-committedRaw_>=128)return {};
    const auto normalized=static_cast<char16_t>(code==0x0130?code:unicode::toLower(code));
    const bool letter=normalized>=u'a' && normalized<=u'z';
    const bool confirm=tabPending_ && letter;
    if(confirm){captureFusionLearning(selected_);captureLearning(selected_);}tabPending_=false;
    if(confirm && current() && selected_<static_cast<int>(result_->candidates.size())) {
        const auto selected=result_->candidates[selected_];
        if(selected.boundary && selected.boundary->rawLength>committedRaw_ &&
            selected.boundary->rawLength<=static_cast<int>(raw_.size())) {
            locks_.push_back(std::make_shared<SentenceLockedPrefix>(SentenceLockedPrefix{
                raw_.substr(0,selected.boundary->rawLength),selected.text,selected.boundary}));
            std::optional<std::u16string> commit;
            if(enabled) {
                auto events=learningThrough(selected.text,selected.boundary->rawLength);
                readyLearning_.insert(readyLearning_.end(),events.begin(),events.end());
                commit=selected.text.substr(committed_.size());committed_=selected.text;
                committedRaw_=lastAutoCommitRaw_=selected.boundary->rawLength;
            }
            continuation_=suspended_=false;resetAutomaticState();
            auto fixed=std::make_shared<SentenceDecodeResult>();fixed->rawCode=result_->rawCode;fixed->candidates={selected};result_=std::move(fixed);
            append(normalized);return commit;
        }
    }
    if(!enabled){resetAutomaticState();append(normalized);return {};}
    std::optional<EmptyCodePending> captured;
    // Empty-code is still model-only: a learned generation may influence menu
    // ordering, but it cannot authorize this one-generation shortcut.
    if(letter && !result_->learningAffected && !emptyCodePending_ && !suspended_ && current()) {
        const bool explicitSelection=std::any_of(raw_.begin(),raw_.end(),[](char16_t c){
            return unicode::isDecimalDigit(c) || c==u';' || c==u'\'';
        });
        std::vector<const SentenceCandidate*> eligible;
        // Legal duplicate singles must compete in both uniqueness and confidence,
        // even while a whole-input candidate list is ordered by lexicon rank.
        for(const auto& c:result_->confidencePool())
            if(starts(c.text,committed_) && (explicitSelection || c.maxLexiconRank<=1 || c.eligibleDuplicateSinglePath))eligible.push_back(&c);
        // Hidden candidates contribute probability, but cannot be selected by
        // the empty-code shortcut when the current menu exposes no such text.
        if(!eligible.empty() && std::any_of(result_->candidates.begin(),result_->candidates.end(),
            [&](const auto& c){return c.text==eligible.front()->text;})) {
            const auto& top=*eligible.front();bool strong=eligible.size()==1 && !result_->earlyCommitEvidence.confidenceTruncated;
            const auto& evidence=result_->earlyCommitEvidence;
            if(!strong && !evidence.confidenceTruncated && !evidence.prefixes.empty() &&
               top.text==result_->candidates.front().text) {
                double maximum=eligible.front()->confidenceScore;
                for(auto c:eligible)maximum=std::max(maximum,c->confidenceScore);
                double total=0;for(auto c:eligible)total+=std::exp(c->confidenceScore-maximum);
                strong=total>0 && std::exp(top.confidenceScore-maximum)/total>=.99999;
            }
            if(strong && top.text.size()>committed_.size() && starts(top.text,committed_)) {
                int lastStart=top.boundary && top.boundary->previous?top.boundary->previous->rawLength:0;
                captured=EmptyCodePending{top.text,committed_,static_cast<int>(raw_.size()),lastStart,eligible.size()==1};
            }
        }
    }
    if(!letter)resetEmptyCodePending();
    append(normalized);
    if(letter && (emptyCodePending_ || captured)) {
        if(!queries.complete || !queries.properPrefix)resetEmptyCodePending();
        else if(queries.complete(raw_,committed_,{},false,activeLock().get()))resetEmptyCodePending();
        else {
            if(!emptyCodePending_)emptyCodePending_=std::move(captured);
            const auto& pending=*emptyCodePending_;
            if(pending.committed!=committed_ || pending.baseRawLength<0 ||
               pending.baseRawLength>=static_cast<int>(raw_.size()) || pending.lastSegmentStart<0 ||
               pending.lastSegmentStart>=static_cast<int>(raw_.size()))resetEmptyCodePending();
            else if(!queries.properPrefix(std::u16string_view(raw_).substr(pending.lastSegmentStart)) &&
                    (minimumRetained<=0 || static_cast<int>(raw_.size())-pending.baseRawLength>=minimumRetained)) {
                if(pending.uniqueness && queries.complete(std::u16string_view(raw_).substr(0,pending.baseRawLength),
                    pending.committed,pending.text,true,activeLock().get()))resetEmptyCodePending();
                else {
                    auto commit=applyPrefix(pending.text,pending.baseRawLength);
                    lastAutoCommitRaw_=pending.baseRawLength;continuation_=true;suspended_=false;
                    resetAutomaticState();++generation_;
                    return commit;
                }
            }
        }
    }
    return tryAutoCommit(enabled,minimumRetained);
}
std::vector<SentenceLearningEvent> SentenceSession::takeLearning() {
    auto result=std::move(readyLearning_);readyLearning_.clear();return result;
}
void SentenceSession::captureLearning(int index) {
    if(!tabPending_ || !learningBaseline_ || !current() || result_->learningMode.empty() ||
       index<0 || index>=static_cast<int>(result_->candidates.size()))return;
    const auto chosen=result_->candidates[index];
    if(learningBaseline_->source!=SentenceSourceComposed || chosen.source!=SentenceSourceComposed) {
        learningBaseline_.reset();return;
    }
    auto boundaries=[](const SentenceCandidate& candidate) {
        std::vector<SentenceLearningBoundary> result;
        for(auto b=candidate.boundary;b;b=b->previous)result.push_back({b->rawLength,b->textLength});
        std::reverse(result.begin(),result.end());return result;
    };
    int floor=std::max(committedRaw_,activeLock()?static_cast<int>(activeLock()->rawCode.size()):0);
    auto events=sentenceLearningDiff(raw_,learningBaseline_->text,chosen.text,boundaries(*learningBaseline_),boundaries(chosen),floor);
    for(auto& e:events){e.mode=result_->learningMode;pendingLearning_.push_back(std::move(e));}
    learningBaseline_.reset();
}
void SentenceSession::captureFusionLearning(int index) {
    if(!current() || result_->learningMode.empty() || index<=0 ||
       index>=static_cast<int>(result_->candidates.size()))return;
    const auto& selected=result_->candidates[index];
    const bool selectedDirect=(selected.source&SentenceSourceDirect)!=0;
    const bool selectedComposed=selected.source==SentenceSourceComposed;
    if(!selectedDirect && !selectedComposed)return;
    const int rawEnd=selected.boundary?selected.boundary->rawLength:static_cast<int>(raw_.size());
    for(int i=0;i<index;++i) {
        const auto& ahead=result_->candidates[i];
        if(selectedDirect && ahead.source==SentenceSourceComposed)
            pendingLearning_.push_back(SentenceFusionPreference::event(
                result_->learningMode,raw_,selected.text,ahead.text,true,rawEnd));
        else if(selectedComposed && (ahead.source&SentenceSourceDirect)!=0)
            pendingLearning_.push_back(SentenceFusionPreference::event(
                result_->learningMode,raw_,ahead.text,selected.text,false,rawEnd));
    }
}

std::vector<SentenceLearningEvent> SentenceSession::learningThrough(std::u16string_view text,int rawEnd) {
    std::vector<SentenceLearningEvent> result;
    auto i=pendingLearning_.begin();
    while(i!=pendingLearning_.end()) {
        if(i->rawEnd>rawEnd){++i;continue;}
        const bool fusion=i->mode==SentenceFusionPreference::mode(result_->learningMode);
        if(fusion || (i->textEnd<=static_cast<int>(text.size()) &&
           text.substr(i->textStart,i->textEnd-i->textStart)==i->text))
            result.push_back(*i);
        i=pendingLearning_.erase(i);
    }
    return result;
}
std::vector<SentenceLearningEvent> SentenceSession::reinforceLearning(const SentenceCandidate& selected,int rawEnd) const {
    std::vector<SentenceLearningEvent> result;
    if(result_->learningMode.empty() || selected.text.empty() || !selected.boundary ||
       selected.source!=SentenceSourceComposed)return result;
    const int floor=std::max(committedRaw_,activeLock()?static_cast<int>(activeLock()->rawCode.size()):0);
    std::vector<std::shared_ptr<const SentencePathBoundary>> boundaries;
    for(auto b=selected.boundary;b;b=b->previous)boundaries.push_back(b);
    std::reverse(boundaries.begin(),boundaries.end());
    for(const auto& b:boundaries) {
        // Scores around 9 / 10.39 / 11.20 correspond to the first, second and
        // third stable observations in Tigirl's existing learning model.
        // Reinforce the first two only; the third is already effectively mature.
        if(b->learningReward<=0 || b->learningReward>=11.0 || b->rawLength>rawEnd ||
           b->learningRawStart<floor || b->learningTextStart<0 || b->learningTextStart>=b->textLength ||
           b->rawLength>b->learningRawStart+128 || b->textLength>static_cast<int>(selected.text.size()))continue;
        SentenceLearningEvent event;event.id=learningId();event.time=learningNow();event.mode=result_->learningMode;
        event.code=raw_.substr(b->learningRawStart,b->rawLength-b->learningRawStart);
        for(auto& c:event.code)if(c>=u'A' && c<=u'Z')c+=u'a'-u'A';
        event.text=selected.text.substr(b->learningTextStart,b->textLength-b->learningTextStart);
        if(!learningStaticText(event.text))continue;
        event.context=learningContext(std::u16string_view(selected.text).substr(0,b->learningTextStart));
        event.rawStart=b->learningRawStart;event.rawEnd=b->rawLength;
        event.textStart=b->learningTextStart;event.textEnd=b->textLength;
        result.push_back(std::move(event));
    }
    return result;
}

std::u16string SentenceSession::displayCode() const {
    auto live=liveRaw();if(!hasResult_ || resultResources_!=resources_ || result_->candidates.empty())return live;
    int index=selected_<static_cast<int>(result_->candidates.size())?selected_:0;
    auto segmented=result_->candidates[index].segmentedCode;if(segmented.empty())return live;
    std::size_t rawIndex=0;for(auto& c:segmented)if(c!=u' ' && rawIndex<raw_.size())c=raw_[rawIndex++];
    if(raw_==result_->rawCode){}
    else if(starts(raw_,result_->rawCode))segmented+=raw_.substr(result_->rawCode.size());
    else if(starts(result_->rawCode,raw_)) {
        std::size_t i=0,n=0;
        for(;i<segmented.size() && n<raw_.size();++i)if(segmented[i]!=u' ') {
            if(segmented[i]!=raw_[n++])return live;
        }
        if(n!=raw_.size())return live;segmented.resize(i);
    }else return live;
    if(committedRaw_) {auto tail=trimAfter(segmented,committedRaw_);return tail.empty() && !live.empty()?live:tail;}
    return segmented;
}
}
