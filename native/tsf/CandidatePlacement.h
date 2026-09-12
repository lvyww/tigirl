#pragma once
#include <windows.h>
#include <algorithm>

namespace tiger::tsf {
// Physical pixels: prefer below the caret, then slide into the current monitor's
// work area. Keep the existing 5 px caret gap and 2 px right/bottom inset. There
// is no above/below latch: the same geometry gives the same result across words,
// temporary layout loss, failed publications and monitor changes.
inline POINT candidatePosition(const RECT& caret,const RECT& work,int width,int height) {
    constexpr LONG gap=5,edge=2;
    return {std::clamp(caret.left,work.left,std::max(work.left,work.right-width-edge)),
        std::clamp(caret.bottom+gap,work.top,std::max(work.top,work.bottom-height-edge))};
}
}
