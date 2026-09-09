#include "SentenceSession.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#ifdef _WIN32
#include <icu.h>
#else
#include <unicode/uchar.h>
#endif
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
    resetAutomaticState();lastAutoCommitRaw_=0;
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
void SentenceSession::edited(){selected_=0;++generation_;}
bool SentenceSession::append(char16_t code) {
    if(raw_.size()-committedRaw_>=128)return false;
    raw_+=static_cast<char16_t>(code==0x0130?code:u_tolower(code));edited();return true;
}
void SentenceSession::backspace() {
    resetAutomaticState();
    if(raw_.size()<=static_cast<std::size_t>(committedRaw_+1)){clear();return;}
    raw_.pop_back();edited();
}
void SentenceSession::changeResources(std::uint64_t resources) {
    if(resources_==resources)return;
    resetAutomaticState();
    resources_=resources;hasResult_=false;result_=std::make_shared<SentenceDecodeResult>();edited();
}
std::optional<SentenceDecodeTicket> SentenceSession::request() const {
    if(!active())return {};
    return SentenceDecodeTicket{identity_,generation_,resources_,raw_,committed_};
}
bool SentenceSession::matches(const SentenceDecodeTicket& ticket) const {
    return active() && ticket.session==identity_ && ticket.generation==generation_ && ticket.resources==resources_ &&
        ticket.raw==raw_ && ticket.requiredPrefix==committed_;
}
bool SentenceSession::current() const {
    return active() && hasResult_ && appliedGeneration_==generation_ && resultResources_==resources_ && result_->rawCode==raw_;
}
bool SentenceSession::apply(const SentenceDecodeTicket& ticket,SentenceDecodeResult result) {
    if(!matches(ticket) || current())return false;
    // Failure/empty results must still consume the request generation. Raw
    // identity retains original casing; the decoder normalizes only lookup.
    result.rawCode=ticket.raw;
    bool implicit=continuation_ && !std::any_of(raw_.begin(),raw_.end(),[](char16_t c){return u_charType(c)==U_DECIMAL_DIGIT_NUMBER || c==u';' || c==u'\'';});
    auto& list=result.candidates;
    list.erase(std::remove_if(list.begin(),list.end(),[&](const SentenceCandidate& c){
        return !starts(c.text,committed_) || (implicit && c.maxLexiconRank>1);
    }),list.end());
    result_=std::make_shared<SentenceDecodeResult>(std::move(result));appliedGeneration_=generation_;resultResources_=resources_;hasResult_=true;selected_=0;return true;
}
void SentenceSession::moveSelection(int delta,int pageSize) {
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
    auto text=candidateText(index);clear();return text;
}
std::u16string SentenceSession::commitWithSuffix(std::u16string_view suffix) {
    auto text=current() && selected_<static_cast<int>(result_->candidates.size())?candidateText(selected_):liveRaw();
    text+=suffix;clear();return text;
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
    auto commit=std::u16string(text.substr(committed_.size()));committed_=text;committedRaw_=rawLength;
    auto filtered=std::make_shared<SentenceDecodeResult>(*result_);
    auto& list=filtered->candidates;list.erase(std::remove_if(list.begin(),list.end(),[&](const SentenceCandidate& c){return !starts(c.text,committed_);}),list.end());
    result_=std::move(filtered);selected_=0;return commit;
}
std::optional<std::u16string> SentenceSession::tryAutoCommit(bool enabled,int minimumRetained) {
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
    if(!enabled){resetAutomaticState();append(code);return {};}
    const auto normalized=static_cast<char16_t>(code==0x0130?code:u_tolower(code));
    const bool letter=normalized>=u'a' && normalized<=u'z';
    std::optional<EmptyCodePending> captured;
    if(letter && !emptyCodePending_ && !suspended_ && current()) {
        const bool explicitSelection=std::any_of(raw_.begin(),raw_.end(),[](char16_t c){
            return u_charType(c)==U_DECIMAL_DIGIT_NUMBER || c==u';' || c==u'\'';
        });
        std::vector<const SentenceCandidate*> eligible;
        for(const auto& c:result_->candidates)if(explicitSelection || c.maxLexiconRank<=1)eligible.push_back(&c);
        if(!eligible.empty()) {
            const auto& top=*eligible.front();bool strong=eligible.size()==1;
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
        else if(queries.complete(raw_,committed_,{},false))resetEmptyCodePending();
        else {
            if(!emptyCodePending_)emptyCodePending_=std::move(captured);
            const auto& pending=*emptyCodePending_;
            if(pending.committed!=committed_ || pending.baseRawLength<0 ||
               pending.baseRawLength>=static_cast<int>(raw_.size()) || pending.lastSegmentStart<0 ||
               pending.lastSegmentStart>=static_cast<int>(raw_.size()))resetEmptyCodePending();
            else if(!queries.properPrefix(std::u16string_view(raw_).substr(pending.lastSegmentStart)) &&
                    (minimumRetained<=0 || static_cast<int>(raw_.size())-pending.baseRawLength>=minimumRetained)) {
                if(pending.uniqueness && queries.complete(std::u16string_view(raw_).substr(0,pending.baseRawLength),
                    pending.committed,pending.text,true))resetEmptyCodePending();
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
