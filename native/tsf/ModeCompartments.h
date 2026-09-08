#pragma once
#include <msctf.h>
#include <wrl/client.h>
#include <functional>
namespace tiger::tsf {
// Thread-affine TSF mode synchronization. Own writes do not notify the consumer;
// external changes notify once, after the two standard compartments agree.
class ModeCompartments {
public:
    ModeCompartments()=default;
    ~ModeCompartments();
    ModeCompartments(const ModeCompartments&)=delete;
    ModeCompartments& operator=(const ModeCompartments&)=delete;
    HRESULT open(ITfThreadMgr* manager,TfClientId client,bool chinese,std::function<void(bool)> changed);
    HRESULT publish(bool chinese);
    void close();
private:
    static LRESULT CALLBACK dispatch(HWND,UINT,WPARAM,LPARAM);
    HWND window_=nullptr;
    HINSTANCE module_=nullptr;
    bool pending_=false,desired_=true,retrying_=false;
    HRESULT changed(REFGUID guid);
    Microsoft::WRL::ComPtr<ITfCompartment> open_,conversion_;
    Microsoft::WRL::ComPtr<ITfSource> openSource_,conversionSource_;
    Microsoft::WRL::ComPtr<ITfCompartmentEventSink> sink_;
    DWORD openCookie_=TF_INVALID_COOKIE,conversionCookie_=TF_INVALID_COOKIE;
    TfClientId client_=TF_CLIENTID_NULL;
    bool writing_=false,chinese_=true;
    std::function<void(bool)> callback_;
};
}
