#pragma once
#include <windows.h>
#include <cstring>
#include <cstdint>
#include <vector>
namespace tiger::tsf {
// Publish a fully drawn backing bitmap without a separate geometry mutation.
// Visibility stays with the caller, so hidden windows cannot flash during preparation.
inline bool publishCandidateFrame(HWND window,HDC pixels,POINT position,SIZE size,BLENDFUNCTION blend) {
    if(!pixels || size.cx<=0 || size.cy<=0)return false;
    POINT source{};
    return UpdateLayeredWindow(window,nullptr,&position,&size,pixels,&source,0,&blend,ULW_ALPHA)!=FALSE;
}
// Reuse the GDI backing surface for same-size content and position updates.
class CandidateSurface {
    HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ original_=nullptr;void* bits_=nullptr;
    SIZE size_{};
public:
    CandidateSurface()=default;
    CandidateSurface(const CandidateSurface&)=delete;
    CandidateSurface& operator=(const CandidateSurface&)=delete;
    ~CandidateSurface(){clear();}
    void clear(){if(dc_){if(original_)SelectObject(dc_,original_);if(bitmap_)DeleteObject(bitmap_);DeleteDC(dc_);}dc_=nullptr;bitmap_=nullptr;original_=nullptr;bits_=nullptr;size_={};}
    bool prepare(SIZE size,const std::vector<std::uint32_t>& pixels){
        if(size.cx<=0 || size.cy<=0 || pixels.size()!=static_cast<std::size_t>(size.cx)*size.cy)return false;
        if(!dc_ || size.cx!=size_.cx || size.cy!=size_.cy){
            clear();dc_=CreateCompatibleDC(nullptr);if(!dc_)return false;
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=size.cx;info.bmiHeader.biHeight=-size.cy;
            info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
            bitmap_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,&bits_,nullptr,0);
            if(!bitmap_){clear();return false;}
            original_=SelectObject(dc_,bitmap_);if(!original_ || original_==HGDI_ERROR){original_=nullptr;clear();return false;}size_=size;
        }
        GdiFlush();std::memcpy(bits_,pixels.data(),pixels.size()*sizeof(std::uint32_t));return true;
    }
    bool publish(HWND window,POINT position){BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};return publishCandidateFrame(window,dc_,position,size_,blend);}
    bool print(HDC target){return dc_ && BitBlt(target,0,0,size_.cx,size_.cy,dc_,0,0,SRCCOPY)!=FALSE;}
};

}
