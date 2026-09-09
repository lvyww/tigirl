#pragma once
#include <windows.h>
#include <algorithm>
namespace tiger::tsf {
// Physical-pixel positioning, matching the reference's 5 px caret gap and
// 2 px right/bottom reserve. Keep an above placement until the session ends.
class CandidatePlacement {
public:
    void reset(){above_=false;}
    POINT place(const RECT& caret,const RECT& work,int width,int height) {
        constexpr int gap=5;
        if(!above_ && caret.bottom+gap+height>work.bottom)
            above_=caret.top-gap-height>=work.top || caret.top-work.top>work.bottom-caret.bottom;
        return {std::clamp(caret.left,work.left,std::max(work.left,work.right-width-2)),
            std::clamp(above_?caret.top-gap-height:caret.bottom+gap,work.top,std::max(work.top,work.bottom-height-2))};
    }
private:bool above_=false;
};
}
