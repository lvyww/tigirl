#pragma once
#include <windows.h>

// Compose D2D surfaces and GDI labels off screen; expose only the completed frame.
class SettingsPaintBuffer {
public:
    SettingsPaintBuffer(HDC target,const RECT& bounds):target_(target),bounds_(bounds){
        memory_=CreateCompatibleDC(target);
        if(memory_)bitmap_=CreateCompatibleBitmap(target,bounds.right-bounds.left,bounds.bottom-bounds.top);
        if(bitmap_){old_=SelectObject(memory_,bitmap_);SetViewportOrgEx(memory_,-bounds.left,-bounds.top,nullptr);}
    }
    ~SettingsPaintBuffer(){if(old_)SelectObject(memory_,old_);if(bitmap_)DeleteObject(bitmap_);if(memory_)DeleteDC(memory_);}
    SettingsPaintBuffer(const SettingsPaintBuffer&)=delete;
    SettingsPaintBuffer& operator=(const SettingsPaintBuffer&)=delete;
    HDC dc()const{return bitmap_?memory_:target_;}
    void present(){if(bitmap_)BitBlt(target_,bounds_.left,bounds_.top,bounds_.right-bounds_.left,bounds_.bottom-bounds_.top,memory_,bounds_.left,bounds_.top,SRCCOPY);}
private:
    HDC target_=nullptr,memory_=nullptr;RECT bounds_{};HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;
};
