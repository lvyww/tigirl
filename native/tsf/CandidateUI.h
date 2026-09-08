#pragma once
#include "Service.h"

namespace tiger::tsf {
class CandidateUI final : public ITfCandidateListUIElementBehavior {
public:
    CandidateUI(Service* owner,std::shared_ptr<Context> state,CandidateStyle style,std::shared_ptr<PrivateFonts> fonts);
    ~CandidateUI();
    void detach();
    void update(const RECT* caret,HWND ownerWindow);
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
    void paint(HDC dc);
    void textMask(HDC dc);
    void place();
    LONG refs_=1;
    Service* owner_=nullptr; // detached before service destruction; no ownership cycle
    std::shared_ptr<Context> state_;
    Engine engine_;
    Snapshot snapshot_;
    CandidateStyle style_;
    std::shared_ptr<PrivateFonts> fonts_;
    CandidatePresentation presentation_;
    std::vector<RECT> itemRects_;
    RECT codeRect_{};
    UINT dpi_=0;
    UINT selected_=0;
    std::vector<UINT> pages_;
    bool shown_=false;
    HWND window_=nullptr;
    HFONT font_=nullptr;
    RECT caret_{};
    int rowHeight_=28,width_=300,height_=32;
};
}
