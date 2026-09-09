#include "SentenceAutoCommit.h"
#include "Grapheme.h"
#include <algorithm>
namespace tiger {
namespace {
bool starts(std::u16string_view text,std::u16string_view prefix){return text.size()>=prefix.size() && text.substr(0,prefix.size())==prefix;}
const SentencePrefixEvidence* find(const std::vector<SentencePrefixEvidence>& list,std::u16string_view text,int raw) {
    for(const auto& p:list)if(p.text==text && p.rawLength==raw)return &p;return nullptr;
}
bool visible(const SentencePrefixEvidence& prefix,const std::vector<SentenceCandidate>& candidates) {
    for(const auto& c:candidates)if(!c.text.empty() && starts(c.text,prefix.text))
        for(auto b=c.boundary;b;b=b->previous)if(b->rawLength==prefix.rawLength && b->textLength>=0 && static_cast<std::size_t>(b->textLength)==prefix.text.size())return true;
    return false;
}
}
std::optional<SentencePrefixCommit> SentenceAutoCommit::evaluate(const SentenceAutoCommitInput& in,const SentenceDecodeResult& result) {
    const auto& raw=result.rawCode;const auto& evidence=result.earlyCommitEvidence;
    if(!in.enabled || in.suspended || !in.matchingLexicon ||
       !(raw==in.raw || (raw.size()+1==in.raw.size() && starts(in.raw,raw))) || raw.size()<=4 || evidence.confidenceTruncated) {
        reset();return {};
    }
    if(!lastSeen_.empty() && lastSeen_==raw)return mature(in,raw);
    if(!lastSeen_.empty() && !(raw.size()==lastSeen_.size()+1 && starts(raw,lastSeen_)))trackers_.clear();
    lastSeen_=raw;
    const auto* accepted=!result.candidates.empty() && result.candidates.front().supplementScore>0?&result.candidates.front().text:nullptr;
    std::vector<SentencePrefixEvidence> qualifying;
    for(const auto& p:evidence.prefixes) {
        if(p.text.empty() || !p.boundaryClosed || p.share<.995 || p.rawLength<=in.committedRaw ||
           p.text.size()<=in.committedText.size() || !starts(p.text,in.committedText) ||
           (accepted && !starts(*accepted,p.text)) || (!evidence.mergedIncompleteTail && !visible(p,result.candidates)))continue;
        auto it=std::find_if(qualifying.begin(),qualifying.end(),[&](const auto& q){return p.text==q.text && p.rawLength==q.rawLength;});
        if(it==qualifying.end())qualifying.push_back(p);else *it=p;
    }
    const bool retain=(evidence.mergedIncompleteTail && !options_.countMergedTail) ||
        (qualifying.empty() && (evidence.neutralLowConfidence || evidence.mergedIncompleteTail));
    std::vector<Tracker> next;
    if(retain) {
        for(auto t:trackers_) {
            const auto* self=find(evidence.prefixes,t.text,t.rawLength);
            if(!self)continue;
            bool contradicted=false;
            for(const auto& p:evidence.prefixes) {
                if(p.text.empty() || p.text==t.text || starts(p.text,t.text) || starts(t.text,p.text))continue;
                std::size_t stem=0;while(stem<p.text.size() && stem<t.text.size() && p.text[stem]==t.text[stem])++stem;
                if(stem && stem<t.text.size() && p.share>self->share){contradicted=true;break;}
            }
            if(contradicted || ++t.gap>3)continue;
            t.share=self->share;next.push_back(std::move(t));
        }
    } else {
        for(const auto& p:qualifying) {
            auto old=std::find_if(trackers_.begin(),trackers_.end(),[&](const auto& t){return t.text==p.text && t.rawLength==p.rawLength;});
            Tracker t=old==trackers_.end()?Tracker{p.text,p.rawLength}:*old;
            t.evidence=std::min(std::max(1,options_.requiredEvidence),t.evidence+1);
            t.strong=p.share>=.99999?std::min(std::max(1,options_.requiredStrong),t.strong+1):0;
            t.gap=0;t.share=p.share;next.push_back(std::move(t));
        }
    }
    trackers_=std::move(next);return mature(in,raw);
}
std::optional<SentencePrefixCommit> SentenceAutoCommit::mature(const SentenceAutoCommitInput& in,std::u16string_view raw) {
    const Tracker* best=nullptr;std::size_t bestLength=0;
    const int retained=in.configuredRetained>0?std::max(options_.minimumRetained,in.configuredRetained):options_.minimumRetained;
    for(const auto& t:trackers_) {
        if((t.evidence<options_.requiredEvidence && t.strong<options_.requiredStrong) || t.rawLength<=in.committedRaw ||
           t.rawLength>static_cast<int>(raw.size()) || static_cast<int>(raw.size())-t.rawLength<retained ||
           t.text.size()<=in.committedText.size() || !starts(t.text,in.committedText))continue;
        const auto length=wordTextElements(t.text).size();
        if(!best || length>bestLength || (length==bestLength && (t.share>best->share || (t.share==best->share && t.rawLength<best->rawLength)))) {
            best=&t;bestLength=length;
        }
    }
    if(!best || static_cast<int>(raw.size())-in.lastAutoCommitRaw<3 || wordTextElements(std::u16string_view(best->text).substr(in.committedText.size())).empty())return {};
    SentencePrefixCommit commit{best->text,best->rawLength};reset();return commit;
}
}
