#pragma once
#include <algorithm>
#include <cstdint>
#include <windows.h>

namespace tiger::tsf {
// Low-level physical-pixel work-area clamp shared by both placement directions.
// Preserve the 2 px right reserve and zero bottom reserve. Oversized content
// starts at the work-area top/left; positioning alone cannot make it fit.
inline POINT clampCandidateOrigin(LONG x,std::int64_t y,const RECT& work,int width,int height) {
    using Wide=std::int64_t;
    return {static_cast<LONG>(std::clamp<Wide>(x,work.left,
                (std::max<Wide>)(work.left,Wide(work.right)-width-2))),
        static_cast<LONG>(std::clamp<Wide>(y,work.top,
                (std::max<Wide>)(work.top,Wide(work.bottom)-height)))};
}
// Stateless below-caret fallback. CandidateOrientation owns direction selection
// and cross-composition memory; this primitive deliberately retains no state.
inline POINT placeCandidateWindow(const RECT& caret,const RECT& work,int width,int height) {
    return clampCandidateOrigin(caret.left,std::int64_t(caret.bottom)+5,work,width,height);
}
}
