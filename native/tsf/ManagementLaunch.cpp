#define NOMINMAX
#include "ManagementLaunch.h"
#include "PackageLayout.h"
#include "../ManagementUri.h"
#include <filesystem>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#pragma comment(lib,"windowsapp.lib")
extern void DllAddRef();
extern void DllRelease();
namespace tiger::tsf {
HRESULT launchManagement(HINSTANCE module,std::wstring_view action,std::wstring_view value) {
    try {
        auto uri=tiger::managementUri(action,value);
        HANDLE token=nullptr;DWORD container=0,size=0;
        if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) {
            GetTokenInformation(token,TokenIsAppContainer,&container,sizeof(container),&size);CloseHandle(token);
        }
        if(container) {
            // User-triggered URI activation lets Windows start the registered
            // desktop tool. No resident broker and no input-text IPC.
            auto operation=winrt::Windows::System::Launcher::LaunchUriAsync(winrt::Windows::Foundation::Uri(uri));
            DllAddRef();
            try {
                operation.Completed([](auto const& result,auto status) {
                    bool ok=false;
                    try {ok=status==winrt::Windows::Foundation::AsyncStatus::Completed && result.GetResults();}catch(...){}
                    if(!ok)MessageBoxW(nullptr,L"无法启动输入法工具。请检查安装是否完整，或在桌面程序中重试。",L"虎娘",MB_OK|MB_ICONERROR);
                    DllRelease();
                });
            }catch(...){DllRelease();throw;}
            return S_OK;
        }
        const auto executable=packageResourceDirectory(module)/L"Tigirl.exe";
        // URI payload has only an allowlisted action and hex-encoded UTF-16.
        auto command=L"\""+executable.wstring()+L"\" --uri \""+uri+L"\"";
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process))return HRESULT_FROM_WIN32(GetLastError());
        CloseHandle(process.hThread);CloseHandle(process.hProcess);return S_OK;
    }catch(...){return winrt::to_hresult();}
}
}
