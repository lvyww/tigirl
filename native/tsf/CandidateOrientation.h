#pragma once
#include "CandidatePlacement.h"
#include <optional>

namespace tiger::tsf {
// Values are sampled in the candidate's physical-pixel DPI scope. Context owns
// the memory; these identifiers additionally fence focus/monitor/host changes.
struct CandidatePlacementEnvironment {
    std::uint64_t epoch=0;
    std::uintptr_t monitor=0,owner=0,root=0;
    unsigned dpi=96;
    RECT work{},ownerBounds{},rootBounds{};
    static bool sameRect(const RECT& a,const RECT& b) {
        return a.left==b.left && a.top==b.top && a.right==b.right && a.bottom==b.bottom;
    }
    bool operator==(const CandidatePlacementEnvironment& b) const {
        return epoch==b.epoch && monitor==b.monitor && owner==b.owner && root==b.root && dpi==b.dpi &&
            sameRect(work,b.work) && sameRect(ownerBounds,b.ownerBounds) && sameRect(rootBounds,b.rootBounds);
    }
};

class CandidateOrientation {
public:
    static bool usableCaret(const RECT& caret) {
        return caret.bottom>caret.top && caret.right>=caret.left;
    }
    void reset() { valid_=false;above_=false; }
    bool above() const { return valid_ && above_; }
    // Copy this value before calculation; publish the copy only after a current
    // frame was successfully displayed. A failed/hidden frame must not latch it.
    std::optional<POINT> place(const RECT& caret,const CandidatePlacementEnvironment& env,int width,int height) {
        using Wide=std::int64_t;
        if(!usableCaret(caret) || width<=0 || height<=0 ||
           env.work.right<=env.work.left || env.work.bottom<=env.work.top)return std::nullopt;
        const Wide caretHeight=Wide(caret.bottom)-caret.top;
        if(!valid_ || !(env==environment_)) {
            above_=false;referenceY_=caret.bottom;referenceHeight_=caretHeight;
        } else {
            // 3 DIP, capped to a quarter of the smaller trusted caret height.
            // Degenerate 1..3 px carets get zero tolerance, not a whole-line band.
            const Wide dip=(Wide(env.dpi?env.dpi:96)*3+48)/96;
            const Wide tolerance=(std::min)((std::max)(Wide(1),dip),
                                           (std::min)(caretHeight,referenceHeight_)/4);
            const Wide delta=Wide(caret.bottom)-referenceY_;
            if(delta < -tolerance) {
                above_=false;referenceY_=caret.bottom;referenceHeight_=caretHeight;
            } else if(delta > tolerance) {
                referenceY_=caret.bottom;referenceHeight_=caretHeight;
            }
            // Do not chase samples inside the band: slow real movement must
            // eventually cross the stable reference, rather than reset every tick.
        }
        const auto& work=env.work;
        const Wide below=Wide(caret.bottom)+5,above=Wide(caret.top)-5-height;
        const bool fitsBelow=below>=work.top && below+height<=work.bottom;
        const bool fitsAbove=above>=work.top && above+height<=work.bottom;
        if(!(above_ && fitsAbove)) {
            if(fitsBelow)above_=false;
            else if(fitsAbove)above_=true;
            else above_=Wide(caret.top)-work.top > Wide(work.bottom)-caret.bottom;
        }
        environment_=env;valid_=true;
        return clampCandidateOrigin(caret.left,above_?above:below,work,width,height);
    }
private:
    bool valid_=false,above_=false;
    LONG referenceY_=0;
    std::int64_t referenceHeight_=0;
    CandidatePlacementEnvironment environment_{};
};
}
