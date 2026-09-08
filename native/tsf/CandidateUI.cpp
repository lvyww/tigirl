#define NOMINMAX
#include "../../SampleIME/Private.h"
#include "../../SampleIME/Globals.h"
#include "CandidateUI.h"
#include <algorithm>
#include <cmath>
#include "../CandidateTheme.h"
#include <cstring>
#include <stdexcept>

namespace tiger::tsf {
namespace {
std::wstring wide(std::u16string_view text) { return {reinterpret_cast<const wchar_t*>(text.data()),text.size()}; }
constexpr wchar_t windowClass[]=L"NativeTiger.Candidate.v1";
}
CandidateUI::CandidateUI(Service* owner,std::shared_ptr<Context> state,CandidateStyle style,std::shared_ptr<PrivateFonts> fonts)
    :owner_(owner),state_(std::move(state)),engine_(state_->engine),snapshot_(engine_.snapshot()),style_(std::move(style)),fonts_(std::move(fonts)) {
    selected_=static_cast<UINT>(snapshot_.page*engine_.pageSize());
    for(UINT i=0;i<snapshot_.total;i+=static_cast<UINT>(engine_.pageSize())) pages_.push_back(i);
    DllAddRef();
}
CandidateUI::~CandidateUI() { detach(); DllRelease(); }
void CandidateUI::detach() {
    owner_=nullptr; shown_=false;
    if(window_) {
        const auto window=window_; window_=nullptr;
        SetWindowLongPtrW(window,GWLP_USERDATA,0);
        DestroyWindow(window);
    }
    if(font_) { DeleteObject(font_); font_=nullptr; }
    fonts_.reset();
    state_.reset();
}
ULONG CandidateUI::AddRef() { return InterlockedIncrement(&refs_); }
ULONG CandidateUI::Release() { auto n=InterlockedDecrement(&refs_); if(!n) delete this; return n; }
HRESULT CandidateUI::QueryInterface(REFIID iid,void** object) {
    if(!object) return E_POINTER;
    *object=nullptr;
    if(iid!=IID_IUnknown && iid!=IID_ITfUIElement && iid!=IID_ITfCandidateListUIElement && iid!=IID_ITfCandidateListUIElementBehavior) return E_NOINTERFACE;
    *object=static_cast<ITfCandidateListUIElementBehavior*>(this); AddRef(); return S_OK;
}
HRESULT CandidateUI::GetDescription(BSTR* value) {
    if(!value) return E_POINTER;
    *value=SysAllocString(L"虎码字词候选"); return *value?S_OK:E_OUTOFMEMORY;
}
HRESULT CandidateUI::GetGUID(GUID* value) { if(!value) return E_POINTER; *value=Global::SampleIMEGuidCandUIElement; return S_OK; }
HRESULT CandidateUI::Show(BOOL value) {
    shown_=value!=FALSE && owner_;
    if(window_) ShowWindow(window_,shown_?SW_SHOWNOACTIVATE:SW_HIDE);
    return S_OK;
}
HRESULT CandidateUI::IsShown(BOOL* value) { if(!value) return E_POINTER; *value=shown_?TRUE:FALSE; return S_OK; }
HRESULT CandidateUI::GetUpdatedFlags(DWORD* value) {
    if(!value) return E_POINTER;
    *value=TF_CLUIE_DOCUMENTMGR|TF_CLUIE_COUNT|TF_CLUIE_SELECTION|TF_CLUIE_STRING|TF_CLUIE_PAGEINDEX|TF_CLUIE_CURRENTPAGE;
    return S_OK;
}
HRESULT CandidateUI::GetDocumentMgr(ITfDocumentMgr** value) {
    if(!value) return E_POINTER;
    *value=nullptr;
    return state_?state_->context->GetDocumentMgr(value):TF_E_DISCONNECTED;
}
HRESULT CandidateUI::GetCount(UINT* value) { if(!value) return E_POINTER; *value=snapshot_.total; return S_OK; }
HRESULT CandidateUI::GetSelection(UINT* value) { if(!value) return E_POINTER; *value=selected_; return S_OK; }
HRESULT CandidateUI::GetString(UINT index,BSTR* value) {
    if(!value) return E_POINTER;
    *value=nullptr;
    if(index>=snapshot_.total) return E_INVALIDARG;
    try {
        auto candidate=engine_.candidateAt(index);
        *value=SysAllocStringLen(reinterpret_cast<const wchar_t*>(candidate.display.data()),static_cast<UINT>(candidate.display.size()));
        return *value?S_OK:E_OUTOFMEMORY;
    } catch(...) { return E_FAIL; }
}
HRESULT CandidateUI::GetPageIndex(UINT* indices,UINT size,UINT* count) {
    if(!count || (size && !indices)) return E_POINTER;
    *count=static_cast<UINT>(pages_.size());
    for(UINT i=0;i<std::min(size,*count);++i) indices[i]=pages_[i];
    return size<*count?S_FALSE:S_OK;
}
HRESULT CandidateUI::SetPageIndex(UINT* indices,UINT count) {
    if(!indices || !count || indices[0]!=0) return E_INVALIDARG;
    for(UINT i=0;i<count;++i) if(indices[i]>=snapshot_.total || (i && indices[i]<=indices[i-1])) return E_INVALIDARG;
    try { pages_.assign(indices,indices+count); return S_OK; } catch(...) { return E_OUTOFMEMORY; }
}
HRESULT CandidateUI::GetCurrentPage(UINT* page) {
    if(!page) return E_POINTER;
    auto found=std::upper_bound(pages_.begin(),pages_.end(),selected_);
    *page=found==pages_.begin()?0:static_cast<UINT>(found-pages_.begin()-1);
    return S_OK;
}
HRESULT CandidateUI::SetSelection(UINT index) {
    if(index>=snapshot_.total) return E_INVALIDARG;
    selected_=index;
    if(window_) paint(nullptr);
    return S_OK;
}
HRESULT CandidateUI::Finalize() {
    if(!owner_ || !state_) return TF_E_DISCONNECTED;
    ComPtr<ITfTextInputProcessorEx> keepAlive=owner_;
    return owner_->choose(state_,selected_,false);
}
HRESULT CandidateUI::Abort() {
    if(!owner_ || !state_) return TF_E_DISCONNECTED;
    ComPtr<ITfTextInputProcessorEx> keepAlive=owner_;
    return owner_->choose(state_,0,true);
}
void CandidateUI::update(const RECT* caret,HWND ownerWindow) {
    if(!owner_ || !state_) return;
    engine_=state_->engine; snapshot_=engine_.snapshot();
    selected_=static_cast<UINT>(snapshot_.page*engine_.pageSize());
    pages_.clear();
    for(UINT i=0;i<snapshot_.total;i+=static_cast<UINT>(engine_.pageSize())) pages_.push_back(i);
    // The candidate model must advance even while the application has no layout.
    // Hide stale geometry until a subsequent layout notification supplies it.
    if(!caret) {
        if(window_) ShowWindow(window_,SW_HIDE);
        return;
    }
    caret_=*caret;
    if(!shown_) return;
    presentation_=presentCandidates(snapshot_,style_);
    if(presentation_.code.empty() && presentation_.items.empty()) {
        if(window_) ShowWindow(window_,SW_HIDE);
        return;
    }
    if(!window_) {
        WNDCLASSEXW klass{}; klass.cbSize=sizeof(klass); klass.lpfnWndProc=windowProc;
        klass.hInstance=Global::dllInstanceHandle; klass.lpszClassName=windowClass;
        klass.hCursor=LoadCursor(nullptr,IDC_ARROW);
        if(!RegisterClassExW(&klass) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return;
        window_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_LAYERED,windowClass,L"虎码字词",
            WS_POPUP,0,0,1,1,ownerWindow,nullptr,Global::dllInstanceHandle,this);
        if(!window_) return;
    }
    const UINT dpi=GetDpiForWindow(window_);
    if(!font_ || dpi_!=dpi) {
        if(font_) DeleteObject(font_);
        dpi_=dpi;
        auto family=wide(style_.font);
        if(!family.empty() && family.front()==L'#') family.erase(0,1);
        const int pixels=std::max(1,static_cast<int>(std::lround(style_.fontSize*dpi/96.0)));
        font_=CreateFontW(-pixels,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,family.c_str());
    }
    HDC dc=GetDC(window_); const auto old=SelectObject(dc,font_);
    TEXTMETRICW metrics{}; GetTextMetricsW(dc,&metrics);
    const int padding=std::max(2,MulDiv(6,static_cast<int>(dpi),96));
    const int gap=std::max(2,MulDiv(10,static_cast<int>(dpi),96));
    rowHeight_=std::max(static_cast<int>(metrics.tmHeight)+2,static_cast<int>(std::lround(style_.fontSize*dpi/96.0*(style_.vertical?1.5:1.0))));
    MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
    GetMonitorInfoW(MonitorFromRect(&caret_,MONITOR_DEFAULTTONEAREST),&monitor);
    const int maxWidth=std::max(1,static_cast<int>(monitor.rcWork.right-monitor.rcWork.left)-2);
    auto measure=[&](std::u16string_view text) {
        SIZE size{}; const auto value=wide(text);
        GetTextExtentPoint32W(dc,value.c_str(),static_cast<int>(value.size()),&size);
        return std::min(std::max(1,static_cast<int>(size.cx)),std::max(1,maxWidth-padding*2));
    };
    int x=padding,y=padding,right=padding;
    codeRect_={}; itemRects_.clear();
    if(!presentation_.code.empty()) {
        const int width=measure(presentation_.code);
        codeRect_={x,y,x+width,y+rowHeight_}; right=codeRect_.right;
        if(style_.vertical) y+=rowHeight_; else x+=width+gap;
    }
    for(const auto& item:presentation_.items) {
        const int width=measure(item);
        if(!style_.vertical && x>padding && x+width+padding>maxWidth) { x=padding; y+=rowHeight_; }
        itemRects_.push_back(RECT{x,y,x+width,y+rowHeight_});
        right=std::max(right,x+width);
        if(style_.vertical) y+=rowHeight_; else x+=width+gap;
    }
    width_=std::min(maxWidth,std::max(right+padding,padding*2+1));
    height_=y+padding+(style_.vertical?0:rowHeight_);
    if(style_.vertical && presentation_.items.empty() && presentation_.code.empty()) height_+=rowHeight_;
    SelectObject(dc,old); ReleaseDC(window_,dc);
    place(); paint(nullptr);
}
void CandidateUI::place() {
    MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
    GetMonitorInfoW(MonitorFromRect(&caret_,MONITOR_DEFAULTTONEAREST),&monitor);
    width_=std::min(width_,static_cast<int>(monitor.rcWork.right-monitor.rcWork.left));
    int x=std::clamp(static_cast<int>(caret_.left),static_cast<int>(monitor.rcWork.left),static_cast<int>(monitor.rcWork.right)-width_);
    int y=static_cast<int>(caret_.bottom)+2;
    if(y+height_>monitor.rcWork.bottom) y=static_cast<int>(caret_.top)-height_-2;
    y=std::max(y,static_cast<int>(monitor.rcWork.top));
    SetWindowPos(window_,HWND_TOPMOST,x,y,width_,height_,SWP_NOACTIVATE|SWP_SHOWWINDOW);
}
void CandidateUI::textMask(HDC dc) {
    RECT bounds; GetClientRect(window_,&bounds);
    FillRect(dc,&bounds,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    const auto old=SelectObject(dc,font_); SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,RGB(255,255,255));
    if(!presentation_.code.empty()) {
        auto code=wide(presentation_.code); auto rect=codeRect_;
        DrawTextW(dc,code.c_str(),static_cast<int>(code.size()),&rect,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    }
    for(std::size_t i=0;i<presentation_.items.size() && i<itemRects_.size();++i) {
        auto row=itemRects_[i];

        auto text=wide(presentation_.items[i]);
        DrawTextW(dc,text.c_str(),static_cast<int>(text.size()),&row,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    }
    SelectObject(dc,old);
}
void CandidateUI::paint(HDC target) {
    if(!window_ || width_<=0 || height_<=0) return;
    HDC memory=CreateCompatibleDC(nullptr);
    if(!memory) return;
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width_; info.bmiHeader.biHeight=-height_;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;
    HBITMAP bitmap=CreateDIBSection(memory,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap) { DeleteDC(memory); return; }
    auto old=SelectObject(memory,bitmap);
    try {
        const auto count=static_cast<std::size_t>(width_)*height_;
        std::memset(bits,0,count*sizeof(std::uint32_t));
        textMask(memory);
        GdiFlush();
        const auto pixels=static_cast<const std::uint32_t*>(bits);
        std::vector<std::uint32_t> mask(pixels,pixels+count);
        std::vector<PixelRect> selection;
        for(std::size_t i=0;i<itemRects_.size();++i) if(selected_==static_cast<UINT>(snapshot_.page*engine_.pageSize())+i) {
            const auto& r=itemRects_[i]; selection.push_back({r.left,r.top,r.right,r.bottom});
        }
        auto rendered=renderCandidateTheme(width_,height_,dpi_/96.0,candidateTheme(style_.theme),selection,mask);
        std::memcpy(bits,rendered.data(),count*sizeof(std::uint32_t));
        if(target) BitBlt(target,0,0,width_,height_,memory,0,0,SRCCOPY);
        else {
            RECT rect; GetWindowRect(window_,&rect);
            POINT position{rect.left,rect.top},source{}; SIZE size{width_,height_};
            BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
            if(!UpdateLayeredWindow(window_,nullptr,&position,&size,memory,&source,0,&blend,ULW_ALPHA))
                OutputDebugStringW(L"NativeTiger: layered candidate update failed\n");
        }
    } catch(...) { OutputDebugStringW(L"NativeTiger: candidate rendering failed\n"); }
    GdiFlush(); SelectObject(memory,old); DeleteObject(bitmap); DeleteDC(memory);
}
LRESULT CALLBACK CandidateUI::windowProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto self=reinterpret_cast<CandidateUI*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE) {
        self=static_cast<CandidateUI*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    if(!self) return DefWindowProcW(window,message,w,l);
    ComPtr<CandidateUI> keepAlive=self;
    try {
        if(message==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
        if(message==WM_ERASEBKGND) return 1;
        if(message==WM_PRINTCLIENT) { self->paint(reinterpret_cast<HDC>(w)); return 0; }
        if(message==WM_PAINT) { PAINTSTRUCT paint; BeginPaint(window,&paint); EndPaint(window,&paint); return 0; }
        if(message==WM_LBUTTONDOWN) {
            POINT point{static_cast<short>(LOWORD(l)),static_cast<short>(HIWORD(l))};
            for(std::size_t i=0;i<self->itemRects_.size();++i) if(PtInRect(&self->itemRects_[i],point)) {
                self->SetSelection(static_cast<UINT>(self->snapshot_.page*self->engine_.pageSize()+i));
                self->Finalize(); break;
            }
            return 0;
        }
        if(message==WM_NCDESTROY) { SetWindowLongPtrW(window,GWLP_USERDATA,0); return DefWindowProcW(window,message,w,l); }
    } catch(...) { return 0; }
    return DefWindowProcW(window,message,w,l);
}
}
