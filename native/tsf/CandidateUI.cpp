#define NOMINMAX
#include "../../SampleIME/Private.h"
#include "../../SampleIME/Globals.h"
#include "CandidateUI.h"
#include "CandidateDpi.h"
#include <algorithm>
#include <cmath>
#include "../CandidateTheme.h"
#include <cstring>
#include <stdexcept>

namespace tiger::tsf {
namespace {
constexpr wchar_t windowClass[]=L"NativeTiger.Candidate.v1";

}
CandidateUI::CandidateUI(Service* owner,std::shared_ptr<Context> state,CandidateStyle style,std::shared_ptr<PrivateFonts> fonts)
    :owner_(owner),state_(std::move(state)),engine_(state_->engine),snapshot_(engine_.snapshot()),style_(std::move(style)),fonts_(std::move(fonts)) {
    selected_=snapshot_.selectedCandidate>=0?static_cast<UINT>(snapshot_.selectedCandidate):static_cast<UINT>(snapshot_.page*engine_.pageSize());
    for(UINT i=0;i<snapshot_.total;i+=static_cast<UINT>(engine_.pageSize())) pages_.push_back(i);
    DllAddRef();
}
CandidateUI::~CandidateUI() { detach(); DllRelease(); }
void CandidateUI::setStyle(const CandidateStyle& style,std::shared_ptr<PrivateFonts> fonts) {
    if(style_==style && fonts_==fonts)return;
    // Construct before replacing the working renderer; font errors keep the old UI.
    auto renderer=std::make_unique<CandidateRenderer>(style,fonts?fonts->paths():std::vector<std::filesystem::path>{});
    style_=style;fonts_=std::move(fonts);renderer_=std::move(renderer);
    refreshReveal();
}
void CandidateUI::detach() {
    owner_=nullptr; shown_=false; reveal_.reset(); placement_.reset();
    if(window_) {
        KillTimer(window_,1);
        const auto window=window_; window_=nullptr;
        SetWindowLongPtrW(window,GWLP_USERDATA,0);
        DestroyWindow(window);
    }
    renderer_.reset();
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
    if(!shown_) { reveal_.reset(); placement_.reset(); if(window_) { KillTimer(window_,1); ShowWindow(window_,SW_HIDE); } }
    else if(window_)refreshReveal();
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
    selected_=snapshot_.selectedCandidate>=0?static_cast<UINT>(snapshot_.selectedCandidate):static_cast<UINT>(snapshot_.page*engine_.pageSize());
    pages_.clear();
    for(UINT i=0;i<snapshot_.total;i+=static_cast<UINT>(engine_.pageSize())) pages_.push_back(i);
    if(shown_)reveal_.update(snapshot_,style_,GetTickCount64());
    // The candidate model must advance even while the application has no layout.
    // Hide stale geometry until a subsequent layout notification supplies it.
    hasCaret_=caret!=nullptr;
    if(!caret) {
        if(window_) ShowWindow(window_,SW_HIDE);
        return;
    }
    caret_=candidatePhysicalCaret(*caret,ownerWindow);
    CandidateDpiScope dpiScope;
    if(!shown_) return;
    if(!window_) {
        WNDCLASSEXW klass{}; klass.cbSize=sizeof(klass); klass.lpfnWndProc=windowProc;
        klass.hInstance=Global::dllInstanceHandle; klass.lpszClassName=windowClass;
        klass.hCursor=LoadCursor(nullptr,IDC_ARROW);
        if(!RegisterClassExW(&klass) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return;
        window_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_LAYERED,windowClass,L"虎码字词",
            WS_POPUP,caret_.left,caret_.bottom,1,1,ownerWindow,nullptr,Global::dllInstanceHandle,this);
        if(!window_) return;
    }
    try {
    if(!renderer_)renderer_=std::make_unique<CandidateRenderer>(style_,fonts_?fonts_->paths():std::vector<std::filesystem::path>{});
    // Move to the caret monitor first, so GetDpiForWindow returns its DPI.
    if(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST)!=MonitorFromRect(&caret_,MONITOR_DEFAULTTONEAREST)) {
        layingOut_=true;
        SetWindowPos(window_,nullptr,caret_.left,caret_.bottom,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
        layingOut_=false;
    }
    refreshReveal();
    } catch(...) {
        ShowWindow(window_,SW_HIDE);
        OutputDebugStringW(L"NativeTiger: DirectWrite candidate layout failed\n");
    }
}
void CandidateUI::refreshReveal() {
    if(!window_)return;
    KillTimer(window_,1);
    if(!owner_ || !state_ || !shown_)return;
    const auto now=GetTickCount64();
    reveal_.update(snapshot_,style_,now);
    presentation_=reveal_.presentation(snapshot_,style_);
    const auto remaining=reveal_.remaining(style_,now);
    if(remaining)SetTimer(window_,1,remaining,nullptr);
    itemRects_.clear();
    if(!hasCaret_ || (presentation_.code.empty() && presentation_.items.empty())) {
        ShowWindow(window_,SW_HIDE);return;
    }
    layoutAndPaint();
}
void CandidateUI::layoutAndPaint() {
    if(!window_ || !renderer_ || layingOut_ || !shown_ || !hasCaret_ ||
       (presentation_.code.empty() && presentation_.items.empty()))return;
    CandidateDpiScope scope;
    struct Guard {bool& flag;Guard(bool& f):flag(f){flag=true;}~Guard(){flag=false;}} guard(layingOut_);
    for(int attempt=0;attempt<2;++attempt) {
        dpi_=GetDpiForWindow(window_);if(!dpi_)dpi_=96;
        MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
        if(!GetMonitorInfoW(MonitorFromRect(&caret_,MONITOR_DEFAULTTONEAREST),&monitor))return;
        renderer_->layout(presentation_,std::max(1.f,(monitor.rcWork.right-monitor.rcWork.left-2)*96.f/dpi_));
        width_=static_cast<int>(renderer_->pixelWidth(dpi_));height_=static_cast<int>(renderer_->pixelHeight(dpi_));
        itemRects_.clear();const float scale=dpi_/96.f;
        for(const auto& r:renderer_->items())itemRects_.push_back(RECT{
            static_cast<LONG>(std::floor(r.left*scale)),static_cast<LONG>(std::floor(r.top*scale)),
            static_cast<LONG>(std::ceil(r.right*scale)),static_cast<LONG>(std::ceil(r.bottom*scale))});
        const auto laidOutDpi=dpi_;place();
        if(GetDpiForWindow(window_)==laidOutDpi)break;
    }
    paint(nullptr);
}
void CandidateUI::place() {
    MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
    GetMonitorInfoW(MonitorFromRect(&caret_,MONITOR_DEFAULTTONEAREST),&monitor);
    width_=std::min(width_,static_cast<int>(monitor.rcWork.right-monitor.rcWork.left));
    const auto monitorId=MonitorFromRect(&caret_,MONITOR_DEFAULTTONEAREST);
    if(placementMonitor_!=monitorId){placement_.reset();placementMonitor_=monitorId;}
    const auto point=placement_.place(caret_,monitor.rcWork,width_,height_);
    SetWindowPos(window_,HWND_TOPMOST,point.x,point.y,width_,height_,SWP_NOACTIVATE|SWP_SHOWWINDOW);
}
void CandidateUI::paint(HDC target) {
    if(!window_ || !renderer_ || width_<=0 || height_<=0) return;
    CandidateDpiScope dpiScope;
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
        std::vector<std::uint32_t> rendered;
        const UINT first=static_cast<UINT>(snapshot_.page*engine_.pageSize());
        renderer_->render(dpi_,selected_>=first?selected_-first:UINT_MAX,rendered);
        if(rendered.size()!=static_cast<std::size_t>(width_)*height_)throw std::runtime_error("Candidate surface/layout mismatch");
        std::memcpy(bits,rendered.data(),rendered.size()*sizeof(std::uint32_t));
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
        if(message==WM_TIMER && w==1) {self->refreshReveal();return 0;}
        if(message==WM_DPICHANGED) {
            self->dpi_=HIWORD(w);self->layoutAndPaint();return 0;
        }
        if(message==WM_DISPLAYCHANGE || message==WM_SETTINGCHANGE) {self->layoutAndPaint();return 0;}
        if(message==WM_MBUTTONUP) {if(self->owner_)self->owner_->candidateCycle();return 0;}
        if(message==WM_RBUTTONDOWN) {
            POINT point{};GetCursorPos(&point);
            if(self->owner_)self->owner_->candidateMenu(point,window);
            return 0;
        }
        if(message==WM_MOUSEWHEEL) {
            if(self->owner_)self->owner_->candidateWheel(static_cast<short>(HIWORD(w)));
            return 0;
        }
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
