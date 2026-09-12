#pragma once
#include <algorithm>
#include <windows.h>

namespace tiger::tsf {
// All inputs are physical screen pixels. Prefer the 5 px gap below the caret;
// when that would overflow, slide up only far enough to touch the work-area
// bottom. Do not flip above the caret or retain placement state across frames.
// Keep the existing 2 px right reserve. An oversized window starts at the
// work-area top/left; positioning alone cannot make oversized content fit.
inline POINT placeCandidateWindow(const RECT& caret,const RECT& work,int width,int height) {
    constexpr LONG gap=5, rightReserve=2;
    return {std::clamp(caret.left,work.left,(std::max)(work.left,work.right-width-rightReserve)),
        std::clamp(caret.bottom+gap,work.top,(std::max)(work.top,work.bottom-height))};
}
}
