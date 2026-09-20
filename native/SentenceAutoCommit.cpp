#include "SentenceAutoCommit.h"
#include "Grapheme.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
namespace tiger {
namespace {
bool starts(std::u16string_view text,std::u16string_view prefix){return text.size()>=prefix.size() && text.substr(0,prefix.size())==prefix;}

}
std::optional<SentencePrefixCommit> SentenceAutoCommit::evaluate(const SentenceAutoCommitInput& in,const SentenceDecodeResult& result) {
    const auto& raw=result.rawCode;const auto& evidence=result.earlyCommitEvidence;
    if(!in.enabled || in.suspended || !in.matchingLexicon ||
       !(raw==in.raw || (raw.size()+1==in.raw.size() && starts(in.raw,raw))) || raw.size()<=4) {
        reset();return {};
    }
    const bool currentGeneration=raw==in.raw;
    if(evidence.confidenceTruncated && !currentGeneration){reset();return {};}
    // Confidence intentionally excludes final-stage ranking priors. Require
    // every automatic commit to remain a prefix of the candidate displayed
    // first after those priors have reranked the menu. A null top preserves
    // merged incomplete-tail evidence, where no candidate is displayable.
    const auto* visibleTop=result.candidates.empty()?nullptr:&result.candidates.front().text;
    if(!lastSeen_.empty() && lastSeen_==raw)return mature(in,raw,visibleTop);
    if(!lastSeen_.empty() && !(raw.size()==lastSeen_.size()+1 && starts(raw,lastSeen_)))trackers_.clear();
    lastSeen_=raw;
    using Key=std::pair<std::u16string_view,int>;
    std::map<Key,const SentencePrefixEvidence*> evidenceIndex;
    for(const auto& p:evidence.prefixes)evidenceIndex.emplace(Key{p.text,p.rawLength},&p);
    std::map<Key,const Tracker*> trackerIndex;
    for(const auto& t:trackers_)trackerIndex.emplace(Key{t.text,t.rawLength},&t);
    std::set<Key> visibleBoundaries;
    for(const auto& c:result.candidates)if(!c.text.empty())for(auto b=c.boundary;b;b=b->previous)
        if(b->textLength>=0 && static_cast<std::size_t>(b->textLength)<=c.text.size())
            visibleBoundaries.emplace(std::u16string_view(c.text).substr(0,b->textLength),b->rawLength);
    std::vector<SentencePrefixEvidence> qualifying;
    std::map<Key,std::size_t> qualifyingIndex;
    for(const auto& p:evidence.prefixes) {
        const double baseShare=std::isnan(p.baseShare)?p.share:p.baseShare;
        if(p.text.empty() || !p.boundaryClosed || p.share<.99 ||
           (evidence.confidenceTruncated && baseShare<.999) || p.rawLength<=in.committedRaw ||
           p.text.size()<=in.committedText.size() || !starts(p.text,in.committedText) ||
           (visibleTop && !starts(*visibleTop,p.text)) || (!evidence.mergedIncompleteTail && !visibleBoundaries.count(Key{p.text,p.rawLength})))continue;
        auto [it,inserted]=qualifyingIndex.emplace(Key{p.text,p.rawLength},qualifying.size());
        if(inserted)qualifying.push_back(p);else qualifying[it->second]=p;
    }
    const bool retain=(evidence.mergedIncompleteTail && !options_.countMergedTail) ||
        (qualifying.empty() && (evidence.neutralLowConfidence || evidence.mergedIncompleteTail));
    std::vector<Tracker> next;
    if(retain) {
        for(auto t:trackers_) {
            auto found=evidenceIndex.find(Key{t.text,t.rawLength});if(found==evidenceIndex.end())continue;
            const auto* self=found->second;
            bool contradicted=false;
            for(const auto& p:evidence.prefixes) {
                if(p.text.empty() || p.text==t.text || starts(p.text,t.text) || starts(t.text,p.text))continue;
                std::size_t stem=0;while(stem<p.text.size() && stem<t.text.size() && p.text[stem]==t.text[stem])++stem;
                if(stem && stem<t.text.size() && p.share>self->share){contradicted=true;break;}
            }
            if(contradicted || ++t.gap>3)continue;
            t.share=self->share;
            if(evidence.neutralLowConfidence){t.evidence=0;t.strong=0;}
            next.push_back(std::move(t));
        }
    } else {
        for(const auto& p:qualifying) {
            auto old=trackerIndex.find(Key{p.text,p.rawLength});
            Tracker t=old==trackerIndex.end()?Tracker{p.text,p.rawLength}:*old->second;
            t.evidence=std::min(std::max(1,options_.requiredEvidence),t.evidence+1);
            const double baseShare=std::isnan(p.baseShare)?p.share:p.baseShare;
            t.strong=baseShare>=.999?std::min(std::max(1,options_.requiredStrong),t.strong+1):0;
            t.gap=0;t.share=p.share;next.push_back(std::move(t));
        }
    }
    trackers_=std::move(next);return mature(in,raw,visibleTop);
}
std::optional<SentencePrefixCommit> SentenceAutoCommit::mature(const SentenceAutoCommitInput& in,std::u16string_view raw,const std::u16string* visibleTop) {
    const Tracker* best=nullptr;std::size_t bestLength=0;
    const int retained=in.configuredRetained>0?std::max(options_.minimumRetained,in.configuredRetained):options_.minimumRetained;
    for(const auto& t:trackers_) {
        const auto committedElements=wordTextElements(in.committedText).size();
        const auto totalElements=wordTextElements(t.text).size();
        const int targetElements=totalElements>committedElements?
            static_cast<int>(totalElements-committedElements):0;
        int protectedBoundary=t.rawLength;
        if(in.competingBoundaryEnd)protectedBoundary=std::max(
            protectedBoundary,in.competingBoundaryEnd(raw,in.committedRaw,t.rawLength,targetElements));
        if((t.evidence<options_.requiredEvidence && t.strong<options_.requiredStrong) || t.rawLength<=in.committedRaw ||
           t.rawLength>static_cast<int>(raw.size()) || static_cast<int>(raw.size())-protectedBoundary<retained ||
           t.text.size()<=in.committedText.size() || !starts(t.text,in.committedText) ||
           (visibleTop && !starts(*visibleTop,t.text)))continue;
        const auto length=wordTextElements(t.text).size();
        if(!best || length>bestLength || (length==bestLength && (t.share>best->share || (t.share==best->share && t.rawLength<best->rawLength)))) {
            best=&t;bestLength=length;
        }
    }
    if(!best || static_cast<int>(raw.size())-in.lastAutoCommitRaw<3 || wordTextElements(std::u16string_view(best->text).substr(in.committedText.size())).empty())return {};
    SentencePrefixCommit commit{best->text,best->rawLength};reset();return commit;
}
}
