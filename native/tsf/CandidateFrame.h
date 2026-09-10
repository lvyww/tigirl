#pragma once
#include <windows.h>
namespace tiger::tsf {
// Publish a fully drawn backing bitmap without a separate geometry mutation.
// Visibility stays with the caller, so hidden windows cannot flash during preparation.
inline bool publishCandidateFrame(HWND window,HDC pixels,POINT position,SIZE size,BLENDFUNCTION blend) {
    if(!pixels || size.cx<=0 || size.cy<=0)return false;
    POINT source{};
    return UpdateLayeredWindow(window,nullptr,&position,&size,pixels,&source,0,&blend,ULW_ALPHA)!=FALSE;
}
}
