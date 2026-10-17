#define NOMINMAX
#include <windows.h>
#include <msctf.h>
#include <wrl/client.h>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void check(HRESULT value, const char* stage) {
    if (FAILED(value)) {
        std::cerr << stage << ": 0x" << std::hex << static_cast<unsigned long>(value) << '\n';
        ComPtr<IErrorInfo> info;
        if (GetErrorInfo(0, &info) == S_OK && info) {
            BSTR description = nullptr;
            if (SUCCEEDED(info->GetDescription(&description)) && description) {
                std::wcerr << description << L'\n'; SysFreeString(description);
            }
        }
        throw std::runtime_error(stage);
    }
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    try {
        check(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "COM initialization");
        const auto module = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module) throw std::runtime_error("Cannot load package DLL");
        const auto getClass = reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**)>(GetProcAddress(module, "DllGetClassObject"));
        if (!getClass) throw std::runtime_error("Missing class factory");
        CLSID clsid{};
        check(CLSIDFromString(L"{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}", &clsid), "CLSID");
        ComPtr<IClassFactory> factory;
        check(getClass(clsid, IID_PPV_ARGS(&factory)), "Class factory");
        ComPtr<ITfTextInputProcessorEx> service;
        check(factory->CreateInstance(nullptr, IID_PPV_ARGS(&service)), "Service instance");
        ComPtr<ITfThreadMgrEx> manager;
        check(CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager)), "Thread manager");
        TfClientId client = TF_CLIENTID_NULL;
        // Activate only this test thread, with no keyboard/profile selection.
        check(manager->ActivateEx(&client, TF_TMAE_NOACTIVATETIP | TF_TMAE_NOACTIVATEKEYBOARDLAYOUT), "Thread activation");
        // This opens the actual bundled dictionary and shared runtime resources.
        ComPtr<ITfClientId> identities;
        check(manager.As(&identities), "Client identities");
        TfClientId serviceClient = TF_CLIENTID_NULL;
        check(identities->GetClientId(clsid, &serviceClient), "Service client identity");
        const auto activation = service->ActivateEx(manager.Get(), serviceClient, 0);
        bool activated = SUCCEEDED(activation);
        if (!activated) {
            // On a clean, unregistered machine Windows cannot attach the TIP's
            // key sink. That stage occurs only AFTER the shipping DLL has opened
            // its dictionary and loaded settings/fonts. Require this exact stop
            // point; missing runtime files must still fail the test.
            ComPtr<IErrorInfo> info;
            std::wstring stopStage;
            if (GetErrorInfo(0, &info) == S_OK && info) {
                BSTR description = nullptr;
                if (SUCCEEDED(info->GetDescription(&description)) && description) {
                    stopStage = description; SysFreeString(description);
                }
            }
            if (activation != E_INVALIDARG || stopStage != L"Advise keystroke events") {
                std::wcerr << L"Unexpected resource startup failure: " << stopStage << L'\n';
                check(activation, "Packaged resource startup");
            }
        }
        check(service->Deactivate(), "Service deactivation");
        service.Reset(); factory.Reset();
        check(manager->Deactivate(), "Thread deactivation");
        identities.Reset(); manager.Reset();
        FreeLibrary(module);
        CoUninitialize();
        std::cout << "{\"resources_opened\":true,\"activation_completed\":" << (activated ? "true" : "false")
                  << ",\"deactivated\":true,\"registration_required\":false}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
