#include "tsf/ModeCompartments.h"
#include <ctffunc.h>
#include <iostream>
#include <memory>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("TSF compartment operation failed");}
void require(bool value){if(!value)throw std::runtime_error("Mode synchronization assertion failed");}
LONG get(ITfCompartment* compartment){VARIANT value;VariantInit(&value);check(compartment->GetValue(&value));require(value.vt==VT_I4);auto result=value.lVal;VariantClear(&value);return result;}
void set(ITfCompartment* compartment,TfClientId client,LONG value){VARIANT v;VariantInit(&v);v.vt=VT_I4;v.lVal=value;check(compartment->SetValue(client,&v));}
int main(){
 try {
    check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
    {
    ComPtr<ITfThreadMgr> manager;check(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager)));
    TfClientId client;check(manager->Activate(&client));
    ComPtr<ITfCompartmentMgr> compartments;check(manager.As(&compartments));
    ComPtr<ITfCompartment> open,conversion;
    check(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,&open));
    check(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,&conversion));
    const LONG extra=TF_CONVERSIONMODE_FULLSHAPE|TF_CONVERSIONMODE_SYMBOL;
    set(conversion.Get(),client,extra);
    int calls=0;bool mode=false;
    auto sync=std::make_unique<tiger::tsf::ModeCompartments>();
    check(sync->open(manager.Get(),client,true,[&](bool chinese){++calls;mode=chinese;}));
    require(get(open.Get())==1 && get(conversion.Get())==(extra|TF_CONVERSIONMODE_NATIVE) && calls==0);
    check(sync->publish(false));require(get(open.Get())==0 && get(conversion.Get())==extra && calls==0);
    set(open.Get(),client,1);require(mode && calls==1 && get(conversion.Get())==(extra|TF_CONVERSIONMODE_NATIVE));
    set(conversion.Get(),client,extra);require(!mode && calls==2 && get(open.Get())==0);
    set(conversion.Get(),client,TF_CONVERSIONMODE_SYMBOL);require(calls==2 && get(open.Get())==0);
    check(sync->publish(true));require(calls==2 && get(conversion.Get())==(TF_CONVERSIONMODE_SYMBOL|TF_CONVERSIONMODE_NATIVE));
    sync->close();set(open.Get(),client,0);require(calls==2);
    check(sync->open(manager.Get(),client,false,[&](bool){++calls;sync.reset();}));
    require(get(open.Get())==0);set(open.Get(),client,1);require(!sync && calls==3);
    set(open.Get(),client,0);require(calls==3);
    check(manager->Deactivate());
    }
    CoUninitialize();std::cout<<"{\"status\":\"passed\",\"external_callbacks\":3,\"own_writes_suppressed\":true,\"other_conversion_bits_preserved\":true,\"callback_destruction\":true}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
