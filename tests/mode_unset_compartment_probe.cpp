// Reproduce the live VT_EMPTY + OnChange reported by Windows during foreground
// activation. Public ClearCompartment disconnects the interface instead, so a
// thin wrapper injects only the unset read and optional callback write lock;
// values, normal notifications, and retry dispatch use real Windows TSF.
#define NOMINMAX
#include "tsf/ModeCompartments.h"
#include <ctffunc.h>
#include <iostream>
#include <stdexcept>
#include <string>
using Microsoft::WRL::ComPtr;
void checkAt(HRESULT hr,int line){if(FAILED(hr))throw std::runtime_error("TSF operation failed at line "+std::to_string(line)+" HRESULT "+std::to_string(hr));}
#define check(expr) checkAt((expr),__LINE__)
void pump(){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
struct Value { VARTYPE vt; LONG number; };
Value get(ITfCompartment* compartment){VARIANT v;VariantInit(&v);check(compartment->GetValue(&v));Value out{v.vt,v.vt==VT_I4?v.lVal:0};VariantClear(&v);return out;}
HRESULT set(ITfCompartment* compartment,TfClientId client,LONG value){VARIANT v;VariantInit(&v);v.vt=VT_I4;v.lVal=value;return compartment->SetValue(client,&v);}
class Compartment final:public ITfCompartment,public ITfSource {
    LONG refs_=1;
    ComPtr<ITfCompartment> real_;
    ComPtr<ITfSource> source_;
    ComPtr<ITfCompartmentEventSink> sink_;
    DWORD cookie_=TF_INVALID_COOKIE;
public:
    bool blockWrites=false,unset=false;
    int writes=0,blockedWrites=0;
    explicit Compartment(ITfCompartment* real):real_(real){check(real_.As(&source_));}
    STDMETHODIMP QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(iid==IID_IUnknown || iid==IID_ITfCompartment)*out=static_cast<ITfCompartment*>(this);
        else if(iid==IID_ITfSource)*out=static_cast<ITfSource*>(this);
        if(!*out)return E_NOINTERFACE;AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override{return InterlockedIncrement(&refs_);}
    STDMETHODIMP_(ULONG) Release() override{const auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
    STDMETHODIMP SetValue(TfClientId client,const VARIANT* value) override {
        ++writes;if(blockWrites){++blockedWrites;return TF_E_LOCKED;}const bool wasUnset=unset;unset=false;const auto hr=real_->SetValue(client,value);if(FAILED(hr))unset=wasUnset;return hr;
    }
    STDMETHODIMP GetValue(VARIANT* value) override{if(unset){if(!value)return E_POINTER;VariantInit(value);return S_OK;}return real_->GetValue(value);}
    STDMETHODIMP AdviseSink(REFIID iid,IUnknown* sink,DWORD* cookie) override {
        auto hr=source_->AdviseSink(iid,sink,cookie);
        if(SUCCEEDED(hr) && iid==IID_ITfCompartmentEventSink){sink->QueryInterface(IID_PPV_ARGS(&sink_));cookie_=*cookie;}return hr;
    }
    STDMETHODIMP UnadviseSink(DWORD cookie) override {
        auto hr=source_->UnadviseSink(cookie);if(cookie==cookie_){sink_.Reset();cookie_=TF_INVALID_COOKIE;}return hr;
    }
    HRESULT notify(REFGUID guid){auto sink=sink_;return sink?sink->OnChange(guid):E_NOINTERFACE;}
};
class Manager final:public ITfThreadMgr,public ITfCompartmentMgr {
    LONG refs_=1;
    ComPtr<ITfThreadMgrEx> real_;
    ComPtr<ITfCompartmentMgr> compartments_;
public:
    TfClientId client=TF_CLIENTID_NULL;
    ComPtr<Compartment> open,conversion;
    Manager(){
        check(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&real_)));
        check(real_->ActivateEx(&client,TF_TMAE_NOACTIVATETIP|TF_TMAE_NOACTIVATEKEYBOARDLAYOUT));check(real_.As(&compartments_));
        ComPtr<ITfCompartment> a,b;check(compartments_->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,&a));check(compartments_->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,&b));
        open.Attach(new Compartment(a.Get()));conversion.Attach(new Compartment(b.Get()));
        check(set(conversion.Get(),client,TF_CONVERSIONMODE_FULLSHAPE|TF_CONVERSIONMODE_SYMBOL));check(set(open.Get(),client,0));
    }
    ~Manager(){real_->Deactivate();}
    STDMETHODIMP QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(iid==IID_IUnknown || iid==IID_ITfThreadMgr)*out=static_cast<ITfThreadMgr*>(this);
        else if(iid==IID_ITfCompartmentMgr)*out=static_cast<ITfCompartmentMgr*>(this);
        if(!*out)return E_NOINTERFACE;AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override{return InterlockedIncrement(&refs_);}
    STDMETHODIMP_(ULONG) Release() override{const auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
    STDMETHODIMP Activate(TfClientId*) override{return E_NOTIMPL;}
    STDMETHODIMP Deactivate() override{return E_NOTIMPL;}
    STDMETHODIMP CreateDocumentMgr(ITfDocumentMgr**) override{return E_NOTIMPL;}
    STDMETHODIMP EnumDocumentMgrs(IEnumTfDocumentMgrs**) override{return E_NOTIMPL;}
    STDMETHODIMP GetFocus(ITfDocumentMgr**) override{return E_NOTIMPL;}
    STDMETHODIMP SetFocus(ITfDocumentMgr*) override{return E_NOTIMPL;}
    STDMETHODIMP AssociateFocus(HWND,ITfDocumentMgr*,ITfDocumentMgr**) override{return E_NOTIMPL;}
    STDMETHODIMP IsThreadFocus(BOOL*) override{return E_NOTIMPL;}
    STDMETHODIMP GetFunctionProvider(REFCLSID,ITfFunctionProvider**) override{return E_NOTIMPL;}
    STDMETHODIMP EnumFunctionProviders(IEnumTfFunctionProviders**) override{return E_NOTIMPL;}
    STDMETHODIMP GetGlobalCompartment(ITfCompartmentMgr**) override{return E_NOTIMPL;}
    STDMETHODIMP GetCompartment(REFGUID guid,ITfCompartment** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(guid==GUID_COMPARTMENT_KEYBOARD_OPENCLOSE)*out=open.Get();else if(guid==GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION)*out=conversion.Get();
        if(!*out)return E_INVALIDARG;(*out)->AddRef();return S_OK;
    }
    STDMETHODIMP ClearCompartment(TfClientId id,REFGUID guid) override{return compartments_->ClearCompartment(id,guid);}
    STDMETHODIMP EnumCompartments(IEnumGUID** values) override{return compartments_->EnumCompartments(values);}
    void empty(bool clearOpen,bool blocked){
        const GUID& guid=clearOpen?GUID_COMPARTMENT_KEYBOARD_OPENCLOSE:GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION;
        // A public ClearCompartment disconnects its interface without notification.
        // The OS foreground path instead reports a live VT_EMPTY and OnChange;
        // reproduce that narrow event while retaining the real TSF store/sinks.
        auto part=clearOpen?open:conversion;part->unset=true;
        part->blockWrites=blocked;const auto hr=part->notify(guid);part->blockWrites=false;check(hr);
    }
    bool chinese(){auto a=get(open.Get()),b=get(conversion.Get());return a.vt==VT_I4 && a.number==1 && b.vt==VT_I4 && (b.number&TF_CONVERSIONMODE_NATIVE)!=0;}
    bool english(){auto a=get(open.Get()),b=get(conversion.Get());return a.vt==VT_I4 && a.number==0 && b.vt==VT_I4 && (b.number&TF_CONVERSIONMODE_NATIVE)==0;}
};
struct Record {std::string name;bool passed;int calls,blocked;bool chinese,english;};
Record emptyCase(bool initial,bool clearOpen,bool blocked){
    ComPtr<Manager> m;m.Attach(new Manager);tiger::tsf::ModeCompartments sync;int calls=0;
    check(sync.open(m.Get(),m->client,initial,[&](bool){++calls;}));m->empty(clearOpen,blocked);pump();
    const bool pass=(initial?m->chinese():m->english()) && calls==0;
    return {std::string(initial?"chinese-":"english-")+(clearOpen?"empty-open-":"empty-conversion-")+(blocked?"deferred":"immediate"),pass,calls,m->open->blockedWrites+m->conversion->blockedWrites,m->chinese(),m->english()};
}
Record supersedeCase(bool clearOpen){
    ComPtr<Manager> m;m.Attach(new Manager);tiger::tsf::ModeCompartments sync;int calls=0;bool changed=true;
    check(sync.open(m.Get(),m->client,true,[&](bool chinese){++calls;changed=chinese;}));m->empty(clearOpen,true);
    // The user's explicit request arrives before any queued WM_APP retry.
    check(set(m->open.Get(),m->client,0));pump();
    return {std::string("explicit-english-supersedes-empty-")+(clearOpen?"open":"conversion"),m->english() && calls==1 && !changed,calls,m->open->blockedWrites+m->conversion->blockedWrites,m->chinese(),m->english()};
}
Record pendingCase(){
    ComPtr<Manager> m;m.Attach(new Manager);tiger::tsf::ModeCompartments sync;int calls=0;bool changed=true;
    check(sync.open(m.Get(),m->client,true,[&](bool chinese){++calls;changed=chinese;}));
    m->conversion->blockWrites=true;check(set(m->open.Get(),m->client,0));m->conversion->blockWrites=false;
    m->empty(false,true);pump();
    return {"empty-preserves-pending-english",m->english() && calls==1 && !changed,calls,m->open->blockedWrites+m->conversion->blockedWrites,m->chinese(),m->english()};
}
Record closeCase(){
    ComPtr<Manager> m;m.Attach(new Manager);tiger::tsf::ModeCompartments sync;int calls=0;
    check(sync.open(m.Get(),m->client,true,[&](bool){++calls;}));m->empty(false,true);sync.close();
    const auto writes=m->open->writes+m->conversion->writes;pump();const bool untouched=writes==m->open->writes+m->conversion->writes;
    check(sync.open(m.Get(),m->client,false,[&](bool){++calls;}));pump();
    return {"close-cancels-retry-reopen-english",untouched && m->english() && calls==0,calls,m->open->blockedWrites+m->conversion->blockedWrites,m->chinese(),m->english()};
}
int main(){try{
    check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));bool all=true;
    auto emit=[&](const Record& r){all=all && r.passed;std::cout<<std::boolalpha<<"{\"scenario\":\""<<r.name<<"\",\"passed\":"<<r.passed<<",\"callbacks\":"<<r.calls<<",\"blocked_writes\":"<<r.blocked<<",\"chinese\":"<<r.chinese<<",\"english\":"<<r.english<<"}\n";};
    for(bool initial:{false,true})for(bool clearOpen:{false,true})for(bool blocked:{false,true})emit(emptyCase(initial,clearOpen,blocked));
    emit(supersedeCase(false));emit(supersedeCase(true));emit(pendingCase());emit(closeCase());
    CoUninitialize();return all?0:1;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
