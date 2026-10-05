#pragma once
#include "SettingsSkin.h"
#include <commctrl.h>
#include <map>

// Settings-only decoration around native EDIT/COMBOBOX interaction.
class SettingsControls {
public:
    enum class Kind {Edit,Combo,Scroll,Popup,Tip,Hotkey};
    explicit SettingsControls(SettingsSkin& skin):skin_(skin){}
    ~SettingsControls();
    void attach(HWND hwnd,Kind kind);
    void scale(UINT dpi,bool contrast);
    void contrast(bool value);
    HBRUSH fieldBrush(HWND hwnd)const;
    COLORREF fieldColor(HWND hwnd)const;
    void field(HDC dc,const RECT& r,HWND hwnd);
    void comboItem(const DRAWITEMSTRUCT& draw);
private:
    SettingsSkin& skin_;
    UINT dpi_=96;bool contrast_=false;
    HBRUSH field_=CreateSolidBrush(RGB(255,250,240)),disabled_=CreateSolidBrush(RGB(242,232,214));
    struct State {Kind kind;bool hot=false;};std::map<HWND,State> states_;
    int px(int value)const{return MulDiv(value,dpi_,96);}
    COLORREF border(HWND hwnd)const;
    void nonclient(HWND hwnd,HDC dc=nullptr);
    void scrollbar(HWND hwnd,HDC dc,const RECT& window);
    void roundedRegion(HWND hwnd);
    static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data);
};
