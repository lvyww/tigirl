#pragma once
#include <windows.h>
#include <algorithm>

namespace tiger::tsf {
// Physical pixels. Prefer the existing 5 px gap below the caret, then move
// upwards only as far as the current monitor's work area requires. In
// particular, overflow aligns the bottom with work.bottom, NOT caret.top.
// No session latch: short word compositions and long sentence compositions
// obey the same rule, including after shrinkage or a work-area/monitor change.
inline POINT placeCandidateWindow(const RECT& caret,const RECT& work,int width,int height) {
    constexpr LONG gap=5,rightReserve=2;
    return {std::clamp(caret.left,work.left,std::max(work.left,work.right-width-rightReserve)),
        std::clamp(caret.bottom+gap,work.top,std::max(work.top,work.bottom-height))};
}
}
