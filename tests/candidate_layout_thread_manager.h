#pragma once
// Test-only manager decorator. Loading a DLL class factory does not activate a
// registered TIP, and native AdviseKeyEventSink rejects that client. This probe
// explicitly calls ITfKeyEventSink, so only the subscription is supplied here.
// All contexts, edit cookies, document operations, UI elements and compartments
// still come from the actual Windows TSF manager. No production hooks/registration.
class LayoutThreadManager final : public ITfThreadMgr,public ITfKeystrokeMgr {
    ULONG refs_=1;
    ComPtr<ITfThreadMgr> real_;
    ComPtr<ITfKeyEventSink> keys_;
    TfClientId client_;
public:
    LayoutThreadManager(ITfThreadMgr* real,TfClientId client):real_(real),client_(client) {}
    STDMETHODIMP QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(iid==IID_IUnknown || iid==IID_ITfThreadMgr)*out=static_cast<ITfThreadMgr*>(this);
        else if(iid==IID_ITfKeystrokeMgr)*out=static_cast<ITfKeystrokeMgr*>(this);
        else return real_->QueryInterface(iid,out);
        AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override {return ++refs_;}
    STDMETHODIMP_(ULONG) Release() override {const auto n=--refs_;if(!n)delete this;return n;}
    STDMETHODIMP Activate(TfClientId* id) override {return real_->Activate(id);}
    STDMETHODIMP Deactivate() override {return real_->Deactivate();}
    STDMETHODIMP CreateDocumentMgr(ITfDocumentMgr** doc) override {return real_->CreateDocumentMgr(doc);}
    STDMETHODIMP EnumDocumentMgrs(IEnumTfDocumentMgrs** docs) override {return real_->EnumDocumentMgrs(docs);}
    STDMETHODIMP GetFocus(ITfDocumentMgr** doc) override {return real_->GetFocus(doc);}
    STDMETHODIMP SetFocus(ITfDocumentMgr* doc) override {return real_->SetFocus(doc);}
    STDMETHODIMP AssociateFocus(HWND w,ITfDocumentMgr* doc,ITfDocumentMgr** old) override {return real_->AssociateFocus(w,doc,old);}
    STDMETHODIMP IsThreadFocus(BOOL* focused) override {return real_->IsThreadFocus(focused);}
    STDMETHODIMP GetFunctionProvider(REFCLSID clsid,ITfFunctionProvider** value) override {return real_->GetFunctionProvider(clsid,value);}
    STDMETHODIMP EnumFunctionProviders(IEnumTfFunctionProviders** value) override {return real_->EnumFunctionProviders(value);}
    STDMETHODIMP GetGlobalCompartment(ITfCompartmentMgr** value) override {return real_->GetGlobalCompartment(value);}
    STDMETHODIMP AdviseKeyEventSink(TfClientId id,ITfKeyEventSink* sink,BOOL) override {
        if(id!=client_ || !sink)return E_INVALIDARG;
        if(keys_)return CONNECT_E_ADVISELIMIT;
        keys_=sink;return S_OK;
    }
    STDMETHODIMP UnadviseKeyEventSink(TfClientId id) override {
        if(id!=client_)return E_INVALIDARG;
        keys_.Reset();return S_OK;
    }
    // The test deliberately bypasses global keyboard routing; unexpected calls
    // fail instead of making untested keystroke-manager behavior look successful.
    STDMETHODIMP GetForeground(CLSID*) override {return E_NOTIMPL;}
    STDMETHODIMP TestKeyDown(WPARAM,LPARAM,BOOL*) override {return E_NOTIMPL;}
    STDMETHODIMP TestKeyUp(WPARAM,LPARAM,BOOL*) override {return E_NOTIMPL;}
    STDMETHODIMP KeyDown(WPARAM,LPARAM,BOOL*) override {return E_NOTIMPL;}
    STDMETHODIMP KeyUp(WPARAM,LPARAM,BOOL*) override {return E_NOTIMPL;}
    STDMETHODIMP GetPreservedKey(ITfContext*,const TF_PRESERVEDKEY*,GUID*) override {return E_NOTIMPL;}
    STDMETHODIMP IsPreservedKey(REFGUID,const TF_PRESERVEDKEY*,BOOL*) override {return E_NOTIMPL;}
    STDMETHODIMP PreserveKey(TfClientId,REFGUID,const TF_PRESERVEDKEY*,const WCHAR*,ULONG) override {return E_NOTIMPL;}
    STDMETHODIMP UnpreserveKey(REFGUID,const TF_PRESERVEDKEY*) override {return E_NOTIMPL;}
    STDMETHODIMP SetPreservedKeyDescription(REFGUID,const WCHAR*,ULONG) override {return E_NOTIMPL;}
    STDMETHODIMP GetPreservedKeyDescription(REFGUID,BSTR*) override {return E_NOTIMPL;}
    STDMETHODIMP SimulatePreservedKey(ITfContext*,REFGUID,BOOL*) override {return E_NOTIMPL;}
};
