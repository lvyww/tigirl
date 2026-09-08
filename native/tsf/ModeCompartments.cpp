#include "ModeCompartments.h"
#include <ctffunc.h>
namespace tiger::tsf {
namespace {
constexpr wchar_t dispatchClass[]=L"NativeTiger.ModeCompartments";
class Sink final:public ITfCompartmentEventSink {
    LONG refs_=1;
    std::function<HRESULT(REFGUID)> callback_;
public:
    explicit Sink(std::function<HRESULT(REFGUID)> callback):callback_(std::move(callback)){}
    STDMETHODIMP QueryInterface(REFIID iid,void** value) override {
        if(!value)return E_POINTER;*value=nullptr;
        if(iid!=IID_IUnknown && iid!=IID_ITfCompartmentEventSink)return E_NOINTERFACE;
        *value=static_cast<ITfCompartmentEventSink*>(this);AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override{return InterlockedIncrement(&refs_);}
    STDMETHODIMP_(ULONG) Release() override{auto refs=InterlockedDecrement(&refs_);if(!refs)delete this;return refs;}
    STDMETHODIMP OnChange(REFGUID guid) override {
        Microsoft::WRL::ComPtr<ITfCompartmentEventSink> alive(this);
        try{return callback_(guid);}catch(...){return E_FAIL;}
    }
};
HRESULT read(ITfCompartment* compartment,LONG& value,bool* initialized=nullptr) {
    VARIANT raw;VariantInit(&raw);auto hr=compartment->GetValue(&raw);
    if(SUCCEEDED(hr)) {
        if(initialized)*initialized=raw.vt==VT_I4;
        if(raw.vt==VT_I4)value=raw.lVal;
        else if(raw.vt==VT_EMPTY)value=0;
        else hr=DISP_E_TYPEMISMATCH;
    }
    VariantClear(&raw);return hr;
}
HRESULT write(ITfCompartment* compartment,TfClientId client,LONG value) {
    LONG previous=0;bool initialized=false;auto hr=read(compartment,previous,&initialized);if(FAILED(hr))return hr;
    if(initialized && previous==value)return S_OK;
    VARIANT raw;VariantInit(&raw);raw.vt=VT_I4;raw.lVal=value;
    return compartment->SetValue(client,&raw);
}
}
ModeCompartments::~ModeCompartments(){close();}
LRESULT CALLBACK ModeCompartments::dispatch(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto self=reinterpret_cast<ModeCompartments*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE) {
        self=static_cast<ModeCompartments*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    if(message==WM_APP && self) {
        if(self->pending_) {
            self->pending_=false; self->retrying_=true;
            self->publish(self->desired_); self->retrying_=false;
        }
        return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
HRESULT ModeCompartments::open(ITfThreadMgr* manager,TfClientId client,bool chinese,std::function<void(bool)> callback) {
    close();if(!manager || client==TF_CLIENTID_NULL)return E_INVALIDARG;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&dispatch),&module_))return HRESULT_FROM_WIN32(GetLastError());
    WNDCLASSW cls{};cls.lpfnWndProc=dispatch;cls.hInstance=module_;cls.lpszClassName=dispatchClass;
    if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return HRESULT_FROM_WIN32(GetLastError());
    window_=CreateWindowExW(0,dispatchClass,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,module_,this);
    if(!window_){auto hr=HRESULT_FROM_WIN32(GetLastError());close();return hr;}
    Microsoft::WRL::ComPtr<ITfCompartmentMgr> compartments;
    auto hr=manager->QueryInterface(IID_PPV_ARGS(&compartments));if(FAILED(hr)){close();return hr;}
    hr=compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,&open_);if(FAILED(hr)){close();return hr;}
    hr=compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,&conversion_);if(FAILED(hr)){close();return hr;}
    client_=client;callback_=std::move(callback);
    hr=publish(chinese);if(FAILED(hr)){close();return hr;}
    sink_.Attach(new Sink([this](REFGUID guid){return changed(guid);}));
    hr=open_.As(&openSource_);
    if(SUCCEEDED(hr))hr=openSource_->AdviseSink(IID_ITfCompartmentEventSink,sink_.Get(),&openCookie_);
    if(SUCCEEDED(hr))hr=conversion_.As(&conversionSource_);
    if(SUCCEEDED(hr))hr=conversionSource_->AdviseSink(IID_ITfCompartmentEventSink,sink_.Get(),&conversionCookie_);
    if(FAILED(hr))close();return hr;
}
void ModeCompartments::close() {
    if(window_){DestroyWindow(window_);window_=nullptr;}
    if(module_){UnregisterClassW(dispatchClass,module_);module_=nullptr;}
    pending_=false;
    if(openSource_ && openCookie_!=TF_INVALID_COOKIE)openSource_->UnadviseSink(openCookie_);
    if(conversionSource_ && conversionCookie_!=TF_INVALID_COOKIE)conversionSource_->UnadviseSink(conversionCookie_);
    openCookie_=conversionCookie_=TF_INVALID_COOKIE;
    sink_.Reset();openSource_.Reset();conversionSource_.Reset();open_.Reset();conversion_.Reset();callback_={};client_=TF_CLIENTID_NULL;
}
HRESULT ModeCompartments::publish(bool chinese) {
    if(!open_ || !conversion_)return E_UNEXPECTED;
    desired_=chinese;
    struct Guard{bool& flag;explicit Guard(bool& value):flag(value){flag=true;}~Guard(){flag=false;}}guard(writing_);
    LONG conversion=0;auto hr=read(conversion_.Get(),conversion);if(FAILED(hr))return hr;
    conversion=chinese?(conversion|TF_CONVERSIONMODE_NATIVE):(conversion&~TF_CONVERSIONMODE_NATIVE);
    hr=write(conversion_.Get(),client_,conversion);
    if(SUCCEEDED(hr))hr=write(open_.Get(),client_,chinese?1:0);
    if(SUCCEEDED(hr)){chinese_=chinese;pending_=false;}
    else if(window_ && !retrying_) {
        // TSF forbids writing a compartment from inside its own OnChange.
        // Coalesce the latest desired state and retry after that callback.
        pending_=true;
        if(PostMessageW(window_,WM_APP,0,0))return S_OK;
        pending_=false;
    }
    return hr;
}
HRESULT ModeCompartments::changed(REFGUID guid) {
    if(writing_)return S_OK;
    LONG value=0;bool next=false;
    auto hr=read(guid==GUID_COMPARTMENT_KEYBOARD_OPENCLOSE?open_.Get():conversion_.Get(),value);if(FAILED(hr))return hr;
    next=guid==GUID_COMPARTMENT_KEYBOARD_OPENCLOSE?value!=0:(value&TF_CONVERSIONMODE_NATIVE)!=0;
    // An explicit open/close request also supersedes a pending edit when its
    // value matches the mode currently displayed while waiting for a lock.
    const bool notify=next!=chinese_ || guid==GUID_COMPARTMENT_KEYBOARD_OPENCLOSE;
    hr=publish(next);
    auto callback=callback_;
    if(SUCCEEDED(hr) && notify && callback)callback(next);
    return hr;
}
}
