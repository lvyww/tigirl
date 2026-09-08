#define NOMINMAX
#include <windows.h>
#include <msctf.h>
#include <iostream>
int wmain(int argc,wchar_t** argv) {
    if(argc!=2)return 2;
    if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 3;
    USHORT processMachine=0,nativeMachine=0;
    if(!IsWow64Process2(GetCurrentProcess(),&processMachine,&nativeMachine))return 4;
    const auto module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    const auto error=module?0:GetLastError();
    HRESULT instanceResult=E_FAIL;
    if(module) {
        const auto getClass=reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID,REFIID,void**)>(GetProcAddress(module,"DllGetClassObject"));
        CLSID clsid{};CLSIDFromString(L"{D2291A80-84D8-4641-9AB2-BDD1472C846B}",&clsid);
        IClassFactory* factory=nullptr;
        if(getClass && SUCCEEDED(instanceResult=getClass(clsid,IID_IClassFactory,reinterpret_cast<void**>(&factory)))) {
            ITfTextInputProcessorEx* service=nullptr;
            instanceResult=factory->CreateInstance(nullptr,IID_ITfTextInputProcessorEx,reinterpret_cast<void**>(&service));
            if(service)service->Release();factory->Release();
        }
        FreeLibrary(module);
    }
    CoUninitialize();
    std::cout<<"{\"wow64_process_machine\":"<<processMachine
        <<",\"native_machine\":"<<nativeMachine<<",\"loaded\":"<<(module?"true":"false")
        <<",\"load_error\":"<<error<<",\"class_instance\":"<<(SUCCEEDED(instanceResult)?"true":"false")
        <<",\"instance_hresult\":"<<static_cast<unsigned long>(instanceResult)<<"}\n";
    return 0;
}
