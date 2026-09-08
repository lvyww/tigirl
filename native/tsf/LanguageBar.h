#pragma once
#include <windows.h>
#include <msctf.h>
#include <ctfutb.h>
#include <ctffunc.h>
#include <wrl/client.h>
namespace tiger::tsf {
class LanguageBar final : public ITfLangBarItemButton, public ITfSource {
public:
    // The Windows taskbar only accepts the standard input-mode item ID.
    static inline const GUID ItemId=GUID_LBI_INPUTMODE;
    LanguageBar(HINSTANCE module,REFCLSID service,bool secure);
    HRESULT open(ITfThreadMgr*,TfClientId);
    void close();
    void update(bool chinese,bool enabled);
    void userWordFailure(bool failed);
    STDMETHODIMP QueryInterface(REFIID,void**) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO*) override;
    STDMETHODIMP GetStatus(DWORD*) override;
    STDMETHODIMP Show(BOOL) override;
    STDMETHODIMP GetTooltipString(BSTR*) override;
    STDMETHODIMP OnClick(TfLBIClick,POINT,const RECT*) override;
    STDMETHODIMP InitMenu(ITfMenu*) override;
    STDMETHODIMP OnMenuSelect(UINT) override;
    STDMETHODIMP GetIcon(HICON*) override;
    STDMETHODIMP GetText(BSTR*) override;
    STDMETHODIMP AdviseSink(REFIID,IUnknown*,DWORD*) override;
    STDMETHODIMP UnadviseSink(DWORD) override;
private:
    ~LanguageBar();
    void notify(DWORD);
    LONG refs_=1;
    HINSTANCE module_;
    TF_LANGBARITEMINFO info_{};
    TfClientId client_=TF_CLIENTID_NULL;
    Microsoft::WRL::ComPtr<ITfLangBarItemMgr> manager_;
    Microsoft::WRL::ComPtr<ITfCompartment> compartment_;
    Microsoft::WRL::ComPtr<ITfLangBarItemSink> sink_;
    bool chinese_=true,enabled_=false,hidden_=false,secure_=false,userWordFailed_=false;
};
}
