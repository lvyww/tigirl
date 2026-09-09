#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include "ConfigStore.h"
#include "Settings.h"
#include "../native/tsf/LanguageBar.h"
#include <map>
using Microsoft::WRL::ComPtr;
void DllAddRef(){}
void DllRelease(){}

#include <iostream>
#include <stdexcept>
class MenuCapture final : public ITfMenu {
    LONG refs_=1;
public:
    std::vector<std::pair<UINT,std::wstring>> items;
    std::map<UINT,ComPtr<MenuCapture>> children;
    std::map<UINT,DWORD> flags;
    STDMETHODIMP QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(iid!=IID_IUnknown && iid!=IID_ITfMenu)return E_NOINTERFACE;
        *out=static_cast<ITfMenu*>(this);AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override{return ++refs_;}
    STDMETHODIMP_(ULONG) Release() override{auto n=--refs_;if(!n)delete this;return n;}
    STDMETHODIMP AddMenuItem(UINT id,DWORD style,HBITMAP,HBITMAP,const WCHAR* text,ULONG length,ITfMenu** submenu) override {
        flags[id]=style;
        if(submenu){children[id].Attach(new MenuCapture);*submenu=children[id].Get();(*submenu)->AddRef();}
        items.emplace_back(id,std::wstring(text,length));return S_OK;
    }
};


struct Handle {HANDLE value=nullptr;~Handle(){if(value)CloseHandle(value);}};
int wmain(int argc,wchar_t** argv) {
    if(argc!=3)return 2;
    try {
        const std::filesystem::path root=argv[2];
        if(!root.is_absolute() || !std::filesystem::exists(root/L".appcontainer-config-test"))throw std::runtime_error("Isolated marked root required");
        const auto path=root/L"config.txt";
        tiger::updateConfigurationValues(path,{{u"主题",u"海蓝"},{u"_preserve",u"unchanged"}});
        Handle process,token,duplicate;
        process.value=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,wcstoul(argv[1],nullptr,10));
        if(!process.value || !OpenProcessToken(process.value,TOKEN_QUERY|TOKEN_DUPLICATE,&token.value) ||
            !DuplicateToken(token.value,SecurityImpersonation,&duplicate.value))throw std::runtime_error("Cannot get test token");
        DWORD container=0,bytes=0;
        if(!GetTokenInformation(token.value,TokenIsAppContainer,&container,sizeof(container),&bytes) || !container)throw std::runtime_error("AppContainer required");
        PWSTR desktop=nullptr;
        if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_NO_PACKAGE_REDIRECTION|KF_FLAG_DONT_VERIFY,nullptr,&desktop)))throw std::runtime_error("Desktop root unavailable");
        const std::wstring expectedRoot=desktop;CoTaskMemFree(desktop);
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        ComPtr<ITfThreadMgr> manager;
        if(FAILED(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager))))throw std::runtime_error("Cannot create menu manager");
        ComPtr<tiger::tsf::LanguageBar> button;button.Attach(new tiger::tsf::LanguageBar(GetModuleHandleW(nullptr),CLSID_NULL,false));
        button->management(root,{});
        if(FAILED(button->open(manager.Get(),1)))throw std::runtime_error("Cannot add menu button");
        struct Close {tiger::tsf::LanguageBar* button;~Close(){button->close();}} close{button.Get()};
        TF_LANGBARITEMINFO info{};button->GetInfo(&info);
        if(!(info.dwStyle&TF_LBI_STYLE_BTN_MENU) || !(info.dwStyle&TF_LBI_STYLE_BTN_BUTTON))throw std::runtime_error("Missing button/menu capabilities");
        if(!ImpersonateLoggedOnUser(duplicate.value))throw std::runtime_error("Cannot impersonate");
        {
            struct Revert {~Revert(){RevertToSelf();}} revert;
            // The call must happen while impersonating, with the same null
            // hToken and flags as Service. Passing an explicit token from an
            // unrestricted caller misses the parent-directory access failure.
            PWSTR shared=nullptr;
            const auto hr=SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_NO_PACKAGE_REDIRECTION|KF_FLAG_DONT_VERIFY,nullptr,&shared);
            const bool same=SUCCEEDED(hr) && shared && expectedRoot==shared;
            CoTaskMemFree(shared);
            if(!same)throw std::runtime_error("Restricted shared-root lookup failed");
            auto config=tiger::readConfiguration(path);
            if(tiger::configurationValue(config,u"主题")!=u"海蓝")throw std::runtime_error("Desktop theme not visible");
            ComPtr<MenuCapture> menu;menu.Attach(new MenuCapture);
            if(FAILED(button->InitMenu(menu.Get())) || menu->children.count(11)!=1)throw std::runtime_error("Cannot build restricted menu");
            UINT themeId=0;for(const auto& entry:menu->children[11]->items)if(entry.second==L"赛博朋克")themeId=entry.first;
            if(!themeId || FAILED(button->OnMenuSelect(themeId)))throw std::runtime_error("Restricted menu theme selection failed");
            if(tiger::configurationValue(tiger::readConfiguration(path),u"主题")!=u"赛博朋克")throw std::runtime_error("Menu did not save theme");
            tiger::updateConfigurationValues(path,{{u"主题",u"清晨"}});
        }
        const auto config=tiger::readConfiguration(path);
        if(tiger::configurationValue(config,u"主题")!=u"清晨" || tiger::configurationValue(config,u"_preserve")!=u"unchanged")throw std::runtime_error("AppContainer theme save differs");
        std::cout<<"{\"status\":\"passed\",\"shared_root\":true,\"desktop_to_container\":true,\"container_to_desktop\":true,\"theme_menu\":true,\"atomic_replace\":true,\"unrelated_preserved\":true}\n";
        return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
