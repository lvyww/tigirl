#pragma once
#include "CandidatePresentation.h"
#include <algorithm>
#include <cstdint>
namespace tiger {
// One clock per visible composition session, matching TigerClaw Overlay.
class CandidateReveal {
public:
    void reset() { active_=candidates_=annotations_=false; start_=0; }
    void update(const Snapshot& snapshot,const CandidateStyle& style,std::uint64_t now) {
        const auto full=presentCandidates(snapshot,style);
        if(full.code.empty() && full.items.empty()) { reset(); return; }
        if(!active_) { active_=true; start_=now; }
        const auto elapsed=now>=start_?now-start_:0;
        candidates_=candidates_ || snapshot.candidates.empty() || elapsed>=static_cast<unsigned>(style.candidateDelayMs);
        const bool hasAnnotations=std::any_of(snapshot.candidates.begin(),snapshot.candidates.end(),[](const Candidate& c){return !c.annotation.empty();});
        annotations_=annotations_ || !hasAnnotations || snapshot.mode==Mode::Pinyin || elapsed>=static_cast<unsigned>(style.annotationDelayMs);
    }
    unsigned remaining(const CandidateStyle& style,std::uint64_t now) const {
        if(!active_)return 0;
        const auto elapsed=now>=start_?now-start_:0;
        unsigned result=0;
        auto add=[&](bool expanded,int delay) {
            if(expanded)return;
            unsigned left=elapsed>=static_cast<unsigned>(delay)?1:static_cast<unsigned>(delay-elapsed);
            result=result?std::min(result,left):left;
        };
        add(candidates_,style.candidateDelayMs);add(annotations_,style.annotationDelayMs);
        return result;
    }
    CandidatePresentation presentation(const Snapshot& snapshot,const CandidateStyle& style) const {
        return presentCandidates(snapshot,style,candidates_,candidates_ && annotations_);
    }
private:
    bool active_=false,candidates_=false,annotations_=false;
    std::uint64_t start_=0;
};
}
