#define NOMINMAX
#include "../../SampleIME/Private.h"
#include "../../SampleIME/Globals.h"
#include "CandidateUI.h"
#include "CandidateDpi.h"
#include "CandidateFrame.h"
#include <algorithm>
#include <cmath>
#include "../CandidateTheme.h"
#include <cstring>
#include <stdexcept>

namespace tiger::tsf {
namespace {
constexpr UINT refreshMessage=WM_APP+0x351;
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
    style_=style;fonts_=std::move(fonts);rendererDirty_=true;
    schedulePaint();
}
void CandidateUI::detach() {
    ++visualRevision_;stopAnimation();refreshPending_=false;
    owner_=nullptr; shown_=false; updatedFlags_=0; hasPresentedCandidates_=false; reveal_.reset(); placement_.reset();
    if(window_) {
        KillTimer(window_,1);KillTimer(window_,3);KillTimer(window_,4);
        const auto window=window_; window_=nullptr;
        SetWindowLongPtrW(window,GWLP_USERDATA,0);
        DestroyWindow(window);
    }
    surface_.clear();renderer_.reset();
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
    if(!shown_) { ++visualRevision_;hideWindow();reveal_.reset(); placement_.reset(); layoutDeadline_=0; if(window_) { KillTimer(window_,4);KillTimer(window_,1); } }
    else if(window_)schedulePaint();
    return S_OK;
}
HRESULT CandidateUI::IsShown(BOOL* value) { if(!value) return E_POINTER; *value=shown_?TRUE:FALSE; return S_OK; }
HRESULT CandidateUI::GetUpdatedFlags(DWORD* value) {
    if(!value) return E_POINTER;
    *value=updatedFlags_;
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
    if(window_)schedulePaint();
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
void CandidateUI::updateContent() {
    // Prepare the entire model before replacing any host-visible state. Layout
    // updates never enter here, even when Context::revision advanced on key-up.
    auto next=state_->engine;
    auto snapshot=next.snapshot();
    const UINT selected=snapshot.selectedCandidate>=0?static_cast<UINT>(snapshot.selectedCandidate):
        static_cast<UINT>(snapshot.page*next.pageSize());
    std::vector<UINT> pages;
    for(UINT i=0;i<snapshot.total;i+=static_cast<UINT>(next.pageSize()))pages.push_back(i);
    UINT previousPage=0;GetCurrentPage(&previousPage);
    DWORD flags=TF_CLUIE_STRING; // Includes candidates beyond the native first page.
    if(snapshot.total!=snapshot_.total)flags|=TF_CLUIE_COUNT;
    if(selected!=selected_)flags|=TF_CLUIE_SELECTION;
    if(pages!=pages_)flags|=TF_CLUIE_PAGEINDEX;
    engine_=std::move(next);snapshot_=std::move(snapshot);
    selected_=selected;pages_=std::move(pages);
    UINT page=0;GetCurrentPage(&page);
    if(page!=previousPage)flags|=TF_CLUIE_CURRENTPAGE;
    updatedFlags_|=flags;++modelRevision_;
}
HRESULT CandidateUI::notifyUpdated(ITfUIElementMgr* manager,DWORD elementId) {
    if(!owner_ || !manager || elementId==TF_INVALID_UIELEMENTID || !updatedFlags_ || notifying_)return S_FALSE;
    ComPtr<CandidateUI> alive=this;
    ComPtr<ITfUIElementMgr> managerAlive=manager;
    struct Guard { bool& flag; Guard(bool& f):flag(f){flag=true;} ~Guard(){flag=false;} } guard(notifying_);
    const auto revision=modelRevision_;
    const auto hr=managerAlive->UpdateUIElement(elementId);
    // Failure is retried on the next update (including a layout recovery). A
    // reentrant content change must not be acknowledged by this older callback.
    if(SUCCEEDED(hr) && owner_ && revision==modelRevision_)updatedFlags_=0;
    return hr;
}
void CandidateUI::update(const RECT* caret,HWND ownerWindow,bool layoutPending,CandidateUpdate update) {
    if(!owner_ || !state_) return;
    if(update==CandidateUpdate::Content)updateContent();
    if(shown_)reveal_.update(snapshot_,style_,GetTickCount64());
    // The candidate model must advance even while the application has no layout.
    // Brief TS_E_NOLAYOUT during an edit must not turn a visible resize into
    // a fresh appearance. Freeze the published frame, with a bounded deadline.
    hasCaret_=caret!=nullptr;
    if(!caret) {
        ++visualRevision_;stopAnimation();itemRects_.clear();
        if(layoutPending && shown_ && window_ && IsWindowVisible(window_)) {
            const auto now=GetTickCount64();
            if(!layoutDeadline_)layoutDeadline_=now+100;
            if(now<layoutDeadline_ && SetTimer(window_,4,static_cast<UINT>(layoutDeadline_-now),nullptr))return;
        }
        layoutDeadline_=0;
        if(window_)KillTimer(window_,4);
        hideWindow();
        return;
    }
    layoutDeadline_=0;if(window_)KillTimer(window_,4);
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
    schedulePaint();
}
void CandidateUI::stopAnimation() {
    transition_.Cancel();if(window_){KillTimer(window_,2);KillTimer(window_,3);}
}
void CandidateUI::hideWindow(bool endPresentation) {
    stopAnimation();itemRects_.clear();
    if(endPresentation)hasPresentedCandidates_=false;
    if(window_)ShowWindow(window_,SW_HIDE);
}
void CandidateUI::schedulePaint() {
    if(!window_ || !owner_ || !shown_)return;
    ++visualRevision_;itemRects_.clear();retryCount_=0;
    if(!refreshPending_)refreshPending_=PostMessageW(window_,refreshMessage,0,0)!=FALSE;
}
void CandidateUI::animate() {
    if(!window_ || !owner_ || !shown_ || !hasCaret_){stopAnimation();return;}
    if(refreshPending_)return; // New input wins over an old animation target.
    if(!transition_.Active()){KillTimer(window_,2);return;}
    const auto rect=transition_.Sample(GetTickCount64());
    const POINT position{rect.x,rect.y};const SIZE size{std::max(1,rect.width),std::max(1,rect.height)};
    if(!paint(nullptr,&position,transition_.Active()?&size:nullptr)) {
        stopAnimation();itemRects_.clear();if(retryCount_++<3)SetTimer(window_,3,100,nullptr);
    } else if(!transition_.Active())KillTimer(window_,2);
}

void CandidateUI::refreshReveal() {
    if(!window_)return;
    KillTimer(window_,1);
    if(!owner_ || !state_ || !shown_)return;
    if(!hasCaret_ && layoutDeadline_)return;
    if(rendererDirty_ || !renderer_) {
        const auto revision=visualRevision_;
        auto next=std::make_shared<CandidateRenderer>(style_,fonts_?fonts_->paths():std::vector<std::filesystem::path>{});
        if(!window_ || !owner_ || revision!=visualRevision_)return;
        renderer_=std::move(next);rendererDirty_=false;finalReady_=false;
    }
    const auto now=GetTickCount64();
    reveal_.update(snapshot_,style_,now);
    presentation_=reveal_.presentation(snapshot_,style_);
    const auto remaining=reveal_.remaining(style_,now);
    if(remaining)SetTimer(window_,1,remaining,nullptr);
    itemRects_.clear();
    if(!hasCaret_ || (presentation_.code.empty() && presentation_.items.empty())) {
        // No items during an active composition need not start a new session.
        // In particular, do not relatch first-presentation state on empty results.
        hideWindow(!hasCaret_ || snapshot_.raw.empty());return;
    }
    layoutAndPaint();
}
void CandidateUI::layoutAndPaint() {
    if(!window_ || !renderer_ || layingOut_ || !shown_ || !hasCaret_ ||
       (presentation_.code.empty() && presentation_.items.empty()))return;
    const bool firstCandidateFrame=!hasPresentedCandidates_ && !presentation_.items.empty();
    if(firstCandidateFrame)stopAnimation();
    CandidateDpiScope scope;
    struct Guard {bool& flag;Guard(bool& f):flag(f){flag=true;}~Guard(){flag=false;}} guard(layingOut_);
    const auto monitorId=MonitorFromRect(&caret_,MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    if(!GetMonitorInfoW(monitorId,&monitor))return;
    dpi_=candidateMonitorDpi(monitorId);
    const auto revision=visualRevision_;
    auto drawing=renderer_;
    const float maxWidth=std::max(1.f,(monitor.rcWork.right-monitor.rcWork.left-2)*96.f/dpi_);
    const bool changed=!finalReady_ || dpi_!=cachedDpi_ || maxWidth!=cachedMaxWidth_ ||
        presentation_.code!=cachedPresentation_.code || presentation_.items!=cachedPresentation_.items || presentation_.codeOnly!=cachedPresentation_.codeOnly;
    if(changed)drawing->layout(presentation_,maxWidth);
    width_=static_cast<int>(drawing->pixelWidth(dpi_));height_=static_cast<int>(drawing->pixelHeight(dpi_));
    auto nextPlacement=placement_;
    if(placementMonitor_!=monitorId)nextPlacement.reset();
    const auto position=nextPlacement.place(caret_,monitor.rcWork,width_,height_);
    const UINT first=static_cast<UINT>(snapshot_.page*engine_.pageSize());
    const auto selection=selected_>=first?selected_-first:UINT_MAX;
    if(changed || cachedSelection_!=selection){
        finalReady_=false;drawing->render(dpi_,selection,finalPixels_);
        if(!window_ || !owner_ || revision!=visualRevision_)return;
        finalReady_=true;cachedPresentation_=presentation_;cachedDpi_=dpi_;cachedMaxWidth_=maxWidth;cachedSelection_=selection;
    }
    RECT current{};GetWindowRect(window_,&current);
    const FrameRect from{current.left,current.top,current.right-current.left,current.bottom-current.top};
    const FrameRect target{position.x,position.y,width_,height_};
    if(!firstCandidateFrame && IsWindowVisible(window_) && style_.animationEnabled && style_.animationDurationMs && from!=target) {
        if(!transition_.Active() || transition_.Target()!=target || transition_.Duration()!=static_cast<unsigned>(style_.animationDurationMs))
            transition_.Start(from,target,GetTickCount64(),60,static_cast<unsigned>(style_.animationDurationMs));
        const auto frame=transition_.Sample(GetTickCount64());
        const POINT at{frame.x,frame.y};const SIZE size{std::max(1,frame.width),std::max(1,frame.height)};
        if(!paint(nullptr,&at,&size))throw std::runtime_error("Candidate animation publication failed");
        if(!SetTimer(window_,2,10,nullptr)){stopAnimation();if(!paint(nullptr,&position))return;}
    } else {
        stopAnimation();if(!paint(nullptr,&position))throw std::runtime_error("Candidate publication failed");
    }
    if(!window_ || !owner_ || revision!=visualRevision_)return;
    // paint() has published pixels and final geometry and, when necessary,
    // successfully shown the window. Only this current frame can latch state.
    if(!presentation_.items.empty() && !drawing->items().empty())hasPresentedCandidates_=true;
    placement_=nextPlacement;placementMonitor_=monitorId;
    itemRects_.clear();const float scale=dpi_/96.f;
    for(const auto& r:renderer_->items())itemRects_.push_back(RECT{
        static_cast<LONG>(std::floor(r.left*scale)),static_cast<LONG>(std::floor(r.top*scale)),
        static_cast<LONG>(std::ceil(r.right*scale)),static_cast<LONG>(std::ceil(r.bottom*scale))});
}
bool CandidateUI::paint(HDC target,const POINT* destination,const SIZE* frameSize) {
    if(!window_ || !renderer_ || width_<=0 || height_<=0) return false;
    CandidateDpiScope dpiScope;
    const int frameWidth=frameSize?frameSize->cx:width_,frameHeight=frameSize?frameSize->cy:height_;
    if(target)return surface_.print(target);
    bool published=false;
    const auto revision=visualRevision_;auto drawing=renderer_;
    try {
        const UINT first=static_cast<UINT>(snapshot_.page*engine_.pageSize());
        if(frameSize)drawing->render(dpi_,selected_>=first?selected_-first:UINT_MAX,scratchPixels_,frameSize);
        else if(!finalReady_)throw std::runtime_error("Candidate final frame unavailable");
        const auto& rendered=frameSize?scratchPixels_:finalPixels_;
        if(rendered.size()!=static_cast<std::size_t>(frameWidth)*frameHeight)throw std::runtime_error("Candidate surface/layout mismatch");
        if(!window_ || !owner_ || !shown_ || !hasCaret_ || revision!=visualRevision_)return false;
        const SIZE size{frameWidth,frameHeight};
        if(!surface_.prepare(size,rendered))throw std::runtime_error("Candidate backing surface unavailable");
        RECT rect{};GetWindowRect(window_,&rect);
        const POINT position=destination?*destination:POINT{rect.left,rect.top};
        const auto window=window_;
        published=surface_.publish(window,position);
        if(!published || window_!=window || !owner_ || !shown_ || !hasCaret_ || revision!=visualRevision_)return false;
        if(!IsWindowVisible(window) &&
           !SetWindowPos(window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW))return false;
        // Showing/publishing can reenter the host; a hidden or superseded frame
        // must not mark the new session as having presented candidates.
        published=window_==window && owner_ && shown_ && hasCaret_ &&
            revision==visualRevision_ && IsWindowVisible(window);
    } catch(...) { OutputDebugStringW(L"NativeTiger: candidate rendering failed\n"); }
    return published;
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
        if(message==refreshMessage) {
            self->refreshPending_=false;
            try{self->refreshReveal();}catch(...){self->stopAnimation();self->itemRects_.clear();if(self->window_ && self->retryCount_++<3)SetTimer(window,3,100,nullptr);}
            return 0;
        }
        if(message==WM_TIMER && w==4) {
            if(self->layoutDeadline_ && !self->hasCaret_) {
                const auto now=GetTickCount64();
                if(now<self->layoutDeadline_){SetTimer(window,4,static_cast<UINT>(self->layoutDeadline_-now),nullptr);return 0;}
                self->layoutDeadline_=0;self->hideWindow();
            }
            KillTimer(window,4);return 0;
        }
        if(message==WM_TIMER && w==1) {KillTimer(window,1);self->schedulePaint();return 0;}
        if(message==WM_TIMER && w==2) {self->animate();return 0;}
        if(message==WM_TIMER && w==3) {KillTimer(window,3);if(!self->refreshPending_)self->refreshPending_=PostMessageW(window,refreshMessage,0,0)!=FALSE;return 0;}
        if(message==WM_DPICHANGED) {
            if(!self->layingOut_)self->schedulePaint();return 0;
        }
        if(message==WM_DISPLAYCHANGE || message==WM_SETTINGCHANGE) {self->schedulePaint();return 0;}
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
        if(message==WM_PRINTCLIENT) { if(self->finalReady_)self->paint(reinterpret_cast<HDC>(w)); return 0; }
        if(message==WM_PAINT) { PAINTSTRUCT paint; BeginPaint(window,&paint); EndPaint(window,&paint); return 0; }
        if(message==WM_LBUTTONDOWN) {
            POINT point{static_cast<short>(LOWORD(l)),static_cast<short>(HIWORD(l))};
            RECT client{};GetClientRect(window,&client);if(!PtInRect(&client,point) || self->refreshPending_)return 0;
            for(std::size_t i=0;i<self->itemRects_.size();++i) if(PtInRect(&self->itemRects_[i],point)) {
                self->SetSelection(static_cast<UINT>(self->snapshot_.page*self->engine_.pageSize()+i));
                self->Finalize(); break;
            }
            return 0;
        }
        if(message==WM_NCDESTROY) {
            if(self->window_==window) {
                ++self->visualRevision_;self->stopAnimation();self->window_=nullptr;
                self->hasPresentedCandidates_=false;self->refreshPending_=false;self->layoutDeadline_=0;
                self->itemRects_.clear();
            }
            SetWindowLongPtrW(window,GWLP_USERDATA,0);return DefWindowProcW(window,message,w,l);
        }
    } catch(...) { return 0; }
    return DefWindowProcW(window,message,w,l);
}
}
