#pragma once
#include <algorithm>
#include <cstdint>
#include <windows.h>

namespace tiger::tsf {
// Coordinates and extents are physical pixels, obtained in the same DPI scope.
struct CandidatePlacementEnvironment {
    RECT work{}, ownerRect{};
    HMONITOR monitor=nullptr;
    HWND owner=nullptr;
    UINT dpi=96;
    bool hasOwnerRect=false;
    static bool sameRect(const RECT& a,const RECT& b) {
        return a.left==b.left && a.top==b.top && a.right==b.right && a.bottom==b.bottom;
    }
    bool operator==(const CandidatePlacementEnvironment& other) const {
        return monitor==other.monitor && owner==other.owner && dpi==other.dpi &&
            sameRect(work,other.work) && hasOwnerRect==other.hasOwnerRect &&
            (!hasOwnerRect || sameRect(ownerRect,other.ownerRect));
    }
};

// Owned by Context, NOT CandidateUI: commit/cancel may destroy a popup without
// ending the focused input environment. Only direction and a stable caret-Y
// reference survive. Actual popup coordinates always follow the current caret.
// Calculate on a copy; publish that copy only after a current visible frame
// succeeds, and only if revision() still matches the source revision.
class CandidatePlacement {
public:
    void reset() { valid_=false; above_=false; ++revision_; }
    bool valid() const { return valid_; }
    bool above() const { return valid_ && above_; }
    LONG referenceY() const { return referenceY_; }
    std::uint64_t revision() const { return revision_; }
    bool sameEnvironment(const CandidatePlacementEnvironment& value) const {
        return valid_ && environment_==value;
    }
    static bool usableCaret(const RECT& caret) {
        return caret.bottom>caret.top && caret.right>=caret.left;
    }
    static LONG tolerance(const RECT& caret,UINT dpi) {
        // About 3 DIP, capped at one quarter of this host caret's height.
        // Very short carets may have zero tolerance; never swallow a full line.
        const std::int64_t pixels=(3LL*(dpi?dpi:96)+48)/96;
        const auto quarter=(static_cast<std::int64_t>(caret.bottom)-caret.top)/4;
        return static_cast<LONG>((std::max)(std::int64_t{0},(std::min)(pixels,quarter)));
    }
    bool place(const RECT& caret,const CandidatePlacementEnvironment& environment,
               int width,int height,POINT& position) {
        const auto& work=environment.work;
        if(!usableCaret(caret) || width<=0 || height<=0 ||
           work.right<=work.left || work.bottom<=work.top)return false;
        using Wide=std::int64_t;
        constexpr Wide gap=5, rightReserve=2;
        const LONG nextTolerance=tolerance(caret,environment.dpi);
        const bool same=sameEnvironment(environment);
        const Wide delta=static_cast<Wide>(caret.bottom)-referenceY_;
        const LONG epsilon=(std::min)(nextTolerance,referenceTolerance_);
        const bool keepAbove=same && above_ && delta>=-static_cast<Wide>(epsilon);
        // Do not replace the reference on every jitter sample: slow accumulated
        // movement must eventually cross the threshold. Downward motion updates
        // the reference but continues to inherit an above preference.
        if(!same || delta>epsilon || delta<-static_cast<Wide>(epsilon)) {
            referenceY_=caret.bottom;referenceTolerance_=nextTolerance;
        } else referenceTolerance_=epsilon;
        const Wide below=static_cast<Wide>(caret.bottom)+gap;
        const Wide above=static_cast<Wide>(caret.top)-gap-height;
        const bool fitsBelow=below>=work.top && below+height<=work.bottom;
        const bool fitsAbove=above>=work.top && above+height<=work.bottom;
        if(keepAbove && fitsAbove)above_=true;
        else if(fitsBelow)above_=false;
        else if(fitsAbove)above_=true;
        else {
            // Oversized/off-work-area geometry: prefer the side with more room,
            // then clamp. Direction memory never overrides visibility constraints.
            const Wide roomAbove=(std::max)(Wide{0},(std::min)(static_cast<Wide>(caret.top)-gap,Wide{work.bottom})-work.top);
            const Wide roomBelow=(std::max)(Wide{0},Wide{work.bottom}-(std::max)(below,Wide{work.top}));
            above_=roomAbove>roomBelow;
        }
        const Wide maxX=(std::max)(Wide{work.left},Wide{work.right}-width-rightReserve);
        const Wide maxY=(std::max)(Wide{work.top},Wide{work.bottom}-height);
        position={static_cast<LONG>(std::clamp(Wide{caret.left},Wide{work.left},maxX)),
                  static_cast<LONG>(std::clamp(above_?above:below,Wide{work.top},maxY))};
        valid_=true;environment_=environment;++revision_;
        return true;
    }
private:
    CandidatePlacementEnvironment environment_{};
    LONG referenceY_=0,referenceTolerance_=0;
    bool valid_=false,above_=false;
    std::uint64_t revision_=0;
};
}
