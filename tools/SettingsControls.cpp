#define NOMINMAX
#include "SettingsControls.h"
#include "SettingsPaintBuffer.h"
#include <windowsx.h>
#include <algorithm>
#include <string>
#include <uxtheme.h>
#pragma comment(lib,"uxtheme.lib")
SettingsControls::~SettingsControls(){if(field_)DeleteObject(field_);if(disabled_)DeleteObject(disabled_);}
COLORREF SettingsControls::fieldColor(HWND hwnd)const {
    if(contrast_)return GetSysColor(IsWindowEnabled(hwnd)?COLOR_WINDOW:COLOR_BTNFACE);
    return IsWindowEnabled(hwnd)?RGB(255,250,240):RGB(242,232,214);
}
HBRUSH SettingsControls::fieldBrush(HWND hwnd)const {
    if(contrast_)return GetSysColorBrush(IsWindowEnabled(hwnd)?COLOR_WINDOW:COLOR_BTNFACE);
    return IsWindowEnabled(hwnd)?field_:disabled_;
}
COLORREF SettingsControls::border(HWND hwnd)const {
    if(contrast_)return GetSysColor(IsWindowEnabled(hwnd)?COLOR_WINDOWTEXT:COLOR_GRAYTEXT);
    if(!IsWindowEnabled(hwnd))return RGB(222,208,186);
    auto it=states_.find(hwnd);const bool hot=it!=states_.end() && it->second.hot;
    return GetFocus()==hwnd || hot?RGB(235,148,49):RGB(210,175,129);
}
void SettingsControls::field(HDC dc,const RECT& r,HWND hwnd){
    skin_.surface(dc,r,dpi_,fieldColor(hwnd),border(hwnd),contrast_?0.f:7.f,1.f);
}
void SettingsControls::attach(HWND hwnd,Kind kind){
    if(!hwnd || states_.count(hwnd))return;
    states_[hwnd]={kind,false};SetWindowSubclass(hwnd,procedure,19,reinterpret_cast<DWORD_PTR>(this));
    if(kind==Kind::Edit || kind==Kind::Tip || kind==Kind::Hotkey){
        SetWindowLongPtrW(hwnd,GWL_EXSTYLE,GetWindowLongPtrW(hwnd,GWL_EXSTYLE)&~WS_EX_CLIENTEDGE);
        SetWindowLongPtrW(hwnd,GWL_STYLE,GetWindowLongPtrW(hwnd,GWL_STYLE)&~WS_BORDER);
    }
    if(kind==Kind::Tip){
        SetWindowTheme(hwnd,L"",L"");RECT margin{px(9),px(7),px(9),px(7)};
        SendMessageW(hwnd,TTM_SETMARGIN,0,reinterpret_cast<LPARAM>(&margin));
    }
    if(kind!=Kind::Tip)SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
}
void SettingsControls::contrast(bool value){
    if(contrast_==value)return;contrast_=value;for(const auto& entry:states_)roundedRegion(entry.first);
}
void SettingsControls::scale(UINT dpi,bool contrast){
    dpi_=dpi;contrast_=contrast;
    // Frame changes can create notifications, but do not add/remove attached windows.
    for(const auto& entry:states_){
        HWND hwnd=entry.first;
        if(entry.second.kind!=Kind::Tip)SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        if(entry.second.kind==Kind::Combo){SendMessageW(hwnd,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),px(22));SendMessageW(hwnd,CB_SETITEMHEIGHT,0,px(28));}
        if(entry.second.kind==Kind::Tip){RECT margin{px(9),px(7),px(9),px(7)};SendMessageW(hwnd,TTM_SETMARGIN,0,reinterpret_cast<LPARAM>(&margin));}
        roundedRegion(hwnd);
    }
}
void SettingsControls::roundedRegion(HWND hwnd){
    auto it=states_.find(hwnd);if(it==states_.end())return;
    if(it->second.kind!=Kind::Edit && it->second.kind!=Kind::Tip)return;
    RECT r{};GetWindowRect(hwnd,&r);
    if(r.right<=r.left || r.bottom<=r.top || (it->second.kind==Kind::Tip && !IsWindowVisible(hwnd)))return;
    SetWindowRgn(hwnd,contrast_?nullptr:CreateRoundRectRgn(0,0,r.right-r.left+1,r.bottom-r.top+1,px(14),px(14)),FALSE);
}
void SettingsControls::scrollbar(HWND hwnd,HDC dc,const RECT& window){
    SCROLLBARINFO info{sizeof(info)};
    if(!GetScrollBarInfo(hwnd,OBJID_VSCROLL,&info) || (info.rgstate[0]&(STATE_SYSTEM_INVISIBLE|STATE_SYSTEM_OFFSCREEN)))return;
    RECT track=info.rcScrollBar;OffsetRect(&track,-window.left,-window.top);
    const bool edit=states_.at(hwnd).kind==Kind::Edit;
    HBRUSH background=edit?fieldBrush(hwnd):GetSysColorBrush(COLOR_BTNFACE);
    if(!edit && !contrast_){background=CreateSolidBrush(RGB(255,243,217));}
    FillRect(dc,&track,background);if(!edit && !contrast_)DeleteObject(background);
    if(info.xyThumbBottom<=info.xyThumbTop)return;
    RECT thumb{track.left+(track.right-track.left-px(6))/2,track.top+info.xyThumbTop,
        track.left+(track.right-track.left+px(6))/2,track.top+info.xyThumbBottom};
    auto fill=CreateSolidBrush(contrast_?GetSysColor(COLOR_WINDOWTEXT):RGB(206,159,102));
    auto oldBrush=SelectObject(dc,fill),oldPen=SelectObject(dc,GetStockObject(NULL_PEN));
    RoundRect(dc,thumb.left,thumb.top,thumb.right,thumb.bottom,px(6),px(6));
    SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(fill);
}
void SettingsControls::nonclient(HWND hwnd,HDC supplied){
    RECT wr{},client{};GetWindowRect(hwnd,&wr);GetClientRect(hwnd,&client);
    POINT origin{};ClientToScreen(hwnd,&origin);OffsetRect(&client,origin.x-wr.left,origin.y-wr.top);
    HDC dc=supplied?supplied:GetWindowDC(hwnd);const int saved=SaveDC(dc);
    ExcludeClipRect(dc,client.left,client.top,client.right,client.bottom);
    const auto kind=states_.at(hwnd).kind;
    if(kind==Kind::Edit || kind==Kind::Popup){RECT r{0,0,wr.right-wr.left,wr.bottom-wr.top};field(dc,r,hwnd);}
    scrollbar(hwnd,dc,wr);RestoreDC(dc,saved);if(!supplied)ReleaseDC(hwnd,dc);
}
void SettingsControls::comboItem(const DRAWITEMSTRUCT& draw){
    const bool selected=(draw.itemState&ODS_SELECTED)!=0;
    const COLORREF bg=contrast_?GetSysColor(selected?COLOR_HIGHLIGHT:COLOR_WINDOW):(selected?RGB(255,221,168):RGB(255,250,240));
    const int saved=SaveDC(draw.hDC);auto brush=CreateSolidBrush(bg);FillRect(draw.hDC,&draw.rcItem,brush);DeleteObject(brush);
    SetBkMode(draw.hDC,TRANSPARENT);SetTextColor(draw.hDC,contrast_?GetSysColor(selected?COLOR_HIGHLIGHTTEXT:COLOR_WINDOWTEXT):RGB(58,42,32));
    SelectObject(draw.hDC,reinterpret_cast<HFONT>(SendMessageW(draw.hwndItem,WM_GETFONT,0,0)));
    if(draw.itemID!=UINT_MAX){const auto length=SendMessageW(draw.hwndItem,CB_GETLBTEXTLEN,draw.itemID,0);
        if(length>=0){std::wstring text(static_cast<size_t>(length)+1,L'\0');SendMessageW(draw.hwndItem,CB_GETLBTEXT,draw.itemID,reinterpret_cast<LPARAM>(text.data()));
            RECT r=draw.rcItem;InflateRect(&r,-px(8),0);DrawTextW(draw.hDC,text.c_str(),-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);}}
    RestoreDC(draw.hDC,saved);
}
LRESULT CALLBACK SettingsControls::procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){
    auto self=reinterpret_cast<SettingsControls*>(data);auto found=self->states_.find(hwnd);if(found==self->states_.end())return DefSubclassProc(hwnd,message,w,l);
    const auto kind=found->second.kind;
    if(message==WM_NCDESTROY){RemoveWindowSubclass(hwnd,procedure,19);self->states_.erase(hwnd);return DefSubclassProc(hwnd,message,w,l);}
    // ComboLBox owns its scroll tracking loop. Keep its complete rectangular frame
    // and native scrollbar in every state; partial nonclient overpainting flashes gray.
    if(kind==Kind::Popup)return DefSubclassProc(hwnd,message,w,l);
    if(message==WM_NCCALCSIZE && kind==Kind::Edit){
        DefSubclassProc(hwnd,message,w,l);auto r=w?&reinterpret_cast<NCCALCSIZE_PARAMS*>(l)->rgrc[0]:reinterpret_cast<RECT*>(l);
        InflateRect(r,-self->px(7),-self->px(3));return 0;
    }
    if(message==WM_NCLBUTTONDOWN && kind==Kind::Edit && w==HTBORDER){SetFocus(hwnd);return 0;}
    if(message==WM_MOUSEMOVE || message==WM_NCMOUSEMOVE){
        if(!found->second.hot){found->second.hot=true;RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME);}
        TRACKMOUSEEVENT track{sizeof(track),static_cast<DWORD>(TME_LEAVE|(message==WM_NCMOUSEMOVE?TME_NONCLIENT:0)),hwnd,0};TrackMouseEvent(&track);
    }
    if(message==WM_MOUSELEAVE || message==WM_NCMOUSELEAVE){found->second.hot=false;RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME);}
    if((kind==Kind::Combo || kind==Kind::Tip) && message==WM_ERASEBKGND)return 1;
    if((kind==Kind::Combo || kind==Kind::Tip) && (message==WM_PAINT || message==WM_PRINTCLIENT || message==WM_PRINT)){
        PAINTSTRUCT paint{};HDC dc=message==WM_PAINT?BeginPaint(hwnd,&paint):reinterpret_cast<HDC>(w);RECT r{};GetClientRect(hwnd,&r);SettingsPaintBuffer buffer(dc,r);dc=buffer.dc();int saved=SaveDC(dc);self->field(dc,r,hwnd);
        if(kind==Kind::Combo){
            DRAWITEMSTRUCT draw{};draw.CtlType=ODT_COMBOBOX;draw.CtlID=GetDlgCtrlID(hwnd);draw.hwndItem=hwnd;draw.hDC=dc;
            draw.itemID=static_cast<UINT>(SendMessageW(hwnd,CB_GETCURSEL,0,0));draw.itemAction=ODA_DRAWENTIRE;draw.itemState=ODS_COMBOBOXEDIT;draw.rcItem=r;
            InflateRect(&draw.rcItem,-self->px(5),-self->px(3));draw.rcItem.right-=self->px(22);
            SendMessageW(GetParent(hwnd),WM_DRAWITEM,draw.CtlID,reinterpret_cast<LPARAM>(&draw));
            const int x=r.right-self->px(14),y=(r.top+r.bottom)/2;
            auto pen=CreatePen(PS_SOLID,self->px(2),self->border(hwnd));auto old=SelectObject(dc,pen);
            MoveToEx(dc,x-self->px(3),y-self->px(1),nullptr);LineTo(dc,x,y+self->px(2));LineTo(dc,x+self->px(3),y-self->px(1));SelectObject(dc,old);DeleteObject(pen);
        }else{
            int length=GetWindowTextLengthW(hwnd);std::wstring text(static_cast<size_t>(length)+1,L'\0');GetWindowTextW(hwnd,text.data(),length+1);
            SelectObject(dc,reinterpret_cast<HFONT>(SendMessageW(hwnd,WM_GETFONT,0,0)));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,self->contrast_?GetSysColor(COLOR_WINDOWTEXT):RGB(58,42,32));
            InflateRect(&r,-self->px(9),-self->px(7));DrawTextW(dc,text.c_str(),-1,&r,DT_WORDBREAK|DT_NOPREFIX);
        }
        RestoreDC(dc,saved);buffer.present();if(message==WM_PAINT)EndPaint(hwnd,&paint);return 0;
    }
    if(message==WM_NCPAINT && (kind==Kind::Edit || kind==Kind::Scroll || kind==Kind::Popup)){self->nonclient(hwnd);return 0;}
    const int saved=(message==WM_PRINT && w)?SaveDC(reinterpret_cast<HDC>(w)):0;
    auto result=DefSubclassProc(hwnd,message,w,l);
    if(saved){RestoreDC(reinterpret_cast<HDC>(w),saved);self->nonclient(hwnd,reinterpret_cast<HDC>(w));}
    if(message==WM_SIZE || (kind==Kind::Tip && message==WM_WINDOWPOSCHANGED && (reinterpret_cast<WINDOWPOS*>(l)->flags&SWP_SHOWWINDOW)))self->roundedRegion(hwnd);
    if(message==WM_SETFOCUS || message==WM_KILLFOCUS || message==WM_ENABLE || message==CB_SETCURSEL || message==CB_SHOWDROPDOWN || message==WM_LBUTTONUP || message==WM_KEYUP)
        RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME);
    if(message==WM_PAINT || message==WM_VSCROLL || message==WM_NCMOUSEMOVE || message==WM_NCLBUTTONDOWN || message==WM_MOUSEWHEEL)
        if(kind==Kind::Edit || kind==Kind::Scroll || kind==Kind::Popup)self->nonclient(hwnd);
    return result;
}
