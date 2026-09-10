#pragma once
#include "Service.h"
#include "CandidateRenderer.h"
#include "CandidatePlacement.h"
#include "FrameTransition.h"
#include "CandidateFrame.h"
#include "../CandidateReveal.h"

namespace tiger::tsf {
class CandidateUI final : public ITfCandidateListUIElementBehavior {
public:
    CandidateUI(Service* owner,std::shared_ptr<Context> state,CandidateStyle style,std::shared_ptr<PrivateFonts> fonts);
    ~CandidateUI();
    void detach();
    void setStyle(const CandidateStyle& style,std::shared_ptr<PrivateFonts> fonts);
    void update(const RECT* caret,HWND ownerWindow,bool layoutPending=false);
    const std::shared_ptr<Context>& context() const { return state_; }
    STDMETHODIMP QueryInterface(REFIID iid,void** object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP GetDescription(BSTR* value) override;
    STDMETHODIMP GetGUID(GUID* value) override;
    STDMETHODIMP Show(BOOL value) override;
    STDMETHODIMP IsShown(BOOL* value) override;
    STDMETHODIMP GetUpdatedFlags(DWORD* value) override;
    STDMETHODIMP GetDocumentMgr(ITfDocumentMgr** value) override;
    STDMETHODIMP GetCount(UINT* value) override;
    STDMETHODIMP GetSelection(UINT* value) override;
    STDMETHODIMP GetString(UINT index,BSTR* value) override;
    STDMETHODIMP GetPageIndex(UINT* indices,UINT size,UINT* count) override;
    STDMETHODIMP SetPageIndex(UINT* indices,UINT count) override;
    STDMETHODIMP GetCurrentPage(UINT* page) override;
    STDMETHODIMP SetSelection(UINT index) override;
    STDMETHODIMP Finalize() override;
    STDMETHODIMP Abort() override;
private:
    static LRESULT CALLBACK windowProc(HWND,UINT,WPARAM,LPARAM);
    bool paint(HDC dc,const POINT* destination=nullptr,const SIZE* size=nullptr);
    void schedulePaint();
    void stopAnimation();
    void animate();
    bool refreshPending_=false,rendererDirty_=false;
    FrameTransition transition_;
    CandidateSurface surface_;
    std::vector<std::uint32_t> finalPixels_,scratchPixels_;
    bool finalReady_=false;
    std::uint64_t visualRevision_=0,layoutDeadline_=0;
    CandidatePresentation cachedPresentation_;
    UINT cachedDpi_=0,cachedSelection_=UINT_MAX;
    float cachedMaxWidth_=0;
    unsigned retryCount_=0;
    void refreshReveal();
    CandidateReveal reveal_;
    void layoutAndPaint();
    LONG refs_=1;
    Service* owner_=nullptr; // detached before service destruction; no ownership cycle
    std::shared_ptr<Context> state_;
    Engine engine_;
    Snapshot snapshot_;
    CandidateStyle style_;
    std::shared_ptr<PrivateFonts> fonts_;
    CandidatePresentation presentation_;
    std::vector<RECT> itemRects_;
    UINT dpi_=0;
    UINT selected_=0;
    std::vector<UINT> pages_;
    bool shown_=false,hasCaret_=false;
    HWND window_=nullptr;
    std::shared_ptr<CandidateRenderer> renderer_;
    bool layingOut_=false;
    RECT caret_{};
    CandidatePlacement placement_;
    HMONITOR placementMonitor_=nullptr;
    int width_=300,height_=32;
};
}
