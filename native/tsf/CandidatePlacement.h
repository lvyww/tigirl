#pragma once
#include <algorithm>
#include <cstdint>
#include <windows.h>

namespace tiger::tsf {
// Context owns the memory; these identify the current focus/coordinate space.
// epoch is Service's focus/mode revision, not a composition or key revision.
struct CandidatePlacementEnvironment {
    std::uintptr_t monitor=0,owner=0;
    std::uint64_t epoch=0;
    unsigned dpi=96;
    RECT ownerRect{};
    bool hasOwnerRect=false;
};

class CandidatePlacement {
public:
    void reset() { valid_=above_=false; ++revision_; }
    std::uint64_t revision() const { return revision_; }
    bool above() const { return valid_ && above_; }
    LONG referenceY() const { return referenceY_; }
    static bool validCaret(const RECT& caret) {
        return caret.bottom>caret.top && caret.right>=caret.left;
    }
    // Small physical-pixel tolerance derived from 3 DIP, capped by one quarter
    // of the input caret height (not the candidate font). Tiny carets use zero.
    static LONG tolerance(const RECT& caret,unsigned dpi) {
        const auto scaled=(3LL*(dpi?dpi:96)+95)/96;
        return static_cast<LONG>((std::min)(scaled,(std::max)(0LL,
            (static_cast<long long>(caret.bottom)-caret.top)/4)));
    }
    // Call on a copy. Publish the copy only after a current frame succeeds.
    // Missing/invalid geometry must neither erase nor advance direction memory.
    POINT place(const RECT& caret,const RECT& work,int width,int height,
                const CandidatePlacementEnvironment& environment={}) {
        if(!validCaret(caret) || width<=0 || height<=0 ||
           work.right<=work.left || work.bottom<=work.top)return {work.left,work.top};
        const bool same=valid_ && equal(work_,work) && equal(environment_,environment);
        if(!same) {
            above_=false;referenceY_=caret.bottom;
            referenceTolerance_=tolerance(caret,environment.dpi);
        }
        const auto noise=(std::min)(referenceTolerance_,tolerance(caret,environment.dpi));
        const auto delta=static_cast<long long>(caret.bottom)-referenceY_;
        const bool inheritAbove=above_ && delta>=-noise;
        // Do not chase every sample: slow upward movement must accumulate and
        // eventually leave the deadband. Downward movement keeps the preference.
        if(delta>noise || delta<-noise) {
            referenceY_=caret.bottom;referenceTolerance_=tolerance(caret,environment.dpi);
        }
        constexpr long long gap=5,rightReserve=2;
        const auto below=static_cast<long long>(caret.bottom)+gap;
        const auto above=static_cast<long long>(caret.top)-gap-height;
        const bool fitsAbove=above>=work.top && above+height<=work.bottom;
        const bool fitsBelow=below>=work.top && below+height<=work.bottom;
        bool chooseAbove=inheritAbove && fitsAbove;
        if(!chooseAbove && !fitsBelow) {
            const auto spaceAbove=(std::max)(0LL,(std::min)(
                static_cast<long long>(work.bottom),static_cast<long long>(caret.top)-gap)-work.top);
            const auto spaceBelow=(std::max)(0LL,static_cast<long long>(work.bottom)-
                (std::max)(static_cast<long long>(work.top),below));
            chooseAbove=fitsAbove || spaceAbove>spaceBelow;
        }
        // Remember a genuine, fully visible above-caret placement, not a
        // clamped oversized fallback that may overlap the input line.
        const bool nextAbove=chooseAbove && fitsAbove;
        if(nextAbove && !above_) {
            referenceY_=caret.bottom;referenceTolerance_=tolerance(caret,environment.dpi);
        }
        above_=nextAbove;valid_=true;work_=work;environment_=environment;++revision_;
        const auto x=std::clamp(static_cast<long long>(caret.left),static_cast<long long>(work.left),
            (std::max)(static_cast<long long>(work.left),static_cast<long long>(work.right)-width-rightReserve));
        const auto y=std::clamp(chooseAbove?above:below,static_cast<long long>(work.top),
            (std::max)(static_cast<long long>(work.top),static_cast<long long>(work.bottom)-height));
        return {static_cast<LONG>(x),static_cast<LONG>(y)};
    }
private:
    static bool equal(const RECT& a,const RECT& b) {
        return a.left==b.left && a.top==b.top && a.right==b.right && a.bottom==b.bottom;
    }
    static bool equal(const CandidatePlacementEnvironment& a,const CandidatePlacementEnvironment& b) {
        return a.monitor==b.monitor && a.owner==b.owner && a.epoch==b.epoch && a.dpi==b.dpi &&
            a.hasOwnerRect==b.hasOwnerRect && (!a.hasOwnerRect || equal(a.ownerRect,b.ownerRect));
    }
    CandidatePlacementEnvironment environment_{};
    RECT work_{};
    LONG referenceY_=0,referenceTolerance_=0;
    std::uint64_t revision_=0;
    bool valid_=false,above_=false;
};

// Fresh-placement convenience for callers without an existing input context.
inline POINT placeCandidateWindow(const RECT& caret,const RECT& work,int width,int height) {
    CandidatePlacement placement;
    return placement.place(caret,work,width,height);
}
}
