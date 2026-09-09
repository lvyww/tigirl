#define NOMINMAX
#include "LanguageBar.h"
#include "../../SampleIME/resource.h"
#include <cwchar>
#include <filesystem>
#include <string>
#include <ctffunc.h>
#include <olectl.h>
#include "../ConfigStore.h"
#include "../Settings.h"
#include "../SchemaCatalog.h"
#include "../CandidateTheme.h"
#include "../OrdinalCase.h"
#include <algorithm>
#include <cstdio>
extern void DllAddRef();
extern void DllRelease();
namespace tiger::tsf {
LanguageBar::LanguageBar(HINSTANCE module,REFCLSID service,bool secure):module_(module),secure_(secure) {
    DllAddRef();
    info_.clsidService=service; info_.guidItem=ItemId;
    info_.dwStyle=TF_LBI_STYLE_BTN_BUTTON|TF_LBI_STYLE_BTN_MENU;
    wcscpy_s(info_.szDescription,L"原生虎码 · 中英文切换");
}
LanguageBar::~LanguageBar(){DllRelease();}
HRESULT LanguageBar::open(ITfThreadMgr* manager,TfClientId client) {
    if(manager_ || !manager || client==TF_CLIENTID_NULL) return E_INVALIDARG;
    Microsoft::WRL::ComPtr<ITfCompartmentMgr> compartments;
    auto hr=manager->QueryInterface(IID_PPV_ARGS(&compartments));
    if(SUCCEEDED(hr)) hr=compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,&compartment_);
    if(SUCCEEDED(hr)) hr=manager->QueryInterface(IID_PPV_ARGS(&manager_));
    if(SUCCEEDED(hr)) { client_=client; hr=manager_->AddItem(this); }
    if(FAILED(hr)) { manager_.Reset(); compartment_.Reset(); client_=TF_CLIENTID_NULL; }
    return hr;
}
void LanguageBar::close() {
    menuParent_=nullptr;
    enabled_=false; addWord_={}; menu_.clear(); compartment_.Reset(); client_=TF_CLIENTID_NULL;
    auto manager=manager_; manager_.Reset();
    if(manager) manager->RemoveItem(this);
    sink_.Reset();
}
HRESULT LanguageBar::QueryInterface(REFIID iid,void** out) {
    if(!out)return E_POINTER; *out=nullptr;
    if(iid==IID_IUnknown || iid==IID_ITfLangBarItem || iid==IID_ITfLangBarItemButton) *out=static_cast<ITfLangBarItemButton*>(this);
    else if(iid==IID_ITfSource) *out=static_cast<ITfSource*>(this);
    else return E_NOINTERFACE;
    AddRef(); return S_OK;
}
ULONG LanguageBar::AddRef(){return InterlockedIncrement(&refs_);}
ULONG LanguageBar::Release(){auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
void LanguageBar::notify(DWORD flags) {
    Microsoft::WRL::ComPtr<ITfLangBarItemButton> alive(this);
    auto sink=sink_; if(sink)sink->OnUpdate(flags);
}
void LanguageBar::update(bool chinese,bool enabled) {
    if(chinese_==chinese && enabled_==enabled)return;
    chinese_=chinese;enabled_=enabled;
    notify(TF_LBI_STATUS|TF_LBI_ICON|TF_LBI_TEXT|TF_LBI_TOOLTIP);
}
void LanguageBar::userWordFailure(bool failed) {
    failed=failed && !secure_;
    if(userWordFailed_==failed)return;
    userWordFailed_=failed;
    notify(TF_LBI_ICON|TF_LBI_TEXT|TF_LBI_TOOLTIP);
}
HRESULT LanguageBar::GetInfo(TF_LANGBARITEMINFO* info){if(!info)return E_POINTER;*info=info_;return S_OK;}
HRESULT LanguageBar::GetStatus(DWORD* status){if(!status)return E_POINTER;*status=(enabled_?0:TF_LBI_STATUS_DISABLED)|(hidden_?TF_LBI_STATUS_HIDDEN:0);return S_OK;}
HRESULT LanguageBar::Show(BOOL show){hidden_=!show;notify(TF_LBI_STATUS);return S_OK;}
HRESULT LanguageBar::GetText(BSTR* text){if(!text)return E_POINTER;*text=SysAllocString(userWordFailed_?(chinese_?L"中!":L"英!"):(chinese_?L"中":L"英"));return *text?S_OK:E_OUTOFMEMORY;}
HRESULT LanguageBar::GetTooltipString(BSTR* text){
    if(!text)return E_POINTER;*text=nullptr;
    try {
        std::wstring value=chinese_?L"原生虎码：中文，点击切换英文":L"原生虎码：英文，点击切换中文";
        if(userWordFailed_)value+=L"\n上次词条调整保存失败。请检查用户词库后重新执行调整。";
        *text=SysAllocString(value.c_str());return *text?S_OK:E_OUTOFMEMORY;
    }catch(...){return E_OUTOFMEMORY;}
}
HRESULT LanguageBar::GetIcon(HICON* icon) {
    if(!icon)return E_POINTER;
    const int size=secure_?24:GetSystemMetrics(SM_CXSMICON);
    if(userWordFailed_) {
        // System resources require LR_SHARED; TSF receives its own sized copy
        // so the caller may destroy it without releasing the shared resource.
        const auto shared=LoadImageW(nullptr,IDI_WARNING,IMAGE_ICON,0,0,LR_DEFAULTSIZE|LR_SHARED);
        *icon=shared?static_cast<HICON>(CopyImage(shared,IMAGE_ICON,size,size,0)):nullptr;
    } else *icon=static_cast<HICON>(LoadImageW(module_,MAKEINTRESOURCEW(chinese_?IDI_IME_MODE_ON:IDI_IME_MODE_OFF),IMAGE_ICON,size,size,0));
    return *icon?S_OK:E_FAIL;
}
void LanguageBar::management(std::filesystem::path root,std::function<void()> addWord) {
    root_=std::move(root);addWord_=std::move(addWord);
}
void LanguageBar::traceMenu(const char* stage,HRESULT result) const noexcept {
    try {
        if(root_.empty() || GetFileAttributesW((root_/L"menu-trace.enabled").c_str())==INVALID_FILE_ATTRIBUTES)return;
        const auto file=CreateFileW((root_/L"menu-trace.log").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return;
        char line[256];const auto size=sprintf_s(line,"%llu pid=%lu tid=%lu %s hr=%08lx parent=%p\r\n",
            GetTickCount64(),GetCurrentProcessId(),GetCurrentThreadId(),stage,static_cast<unsigned long>(result),static_cast<void*>(menuParent_));
        DWORD written=0;if(size>0)WriteFile(file,line,static_cast<DWORD>(size),&written,nullptr);
        CloseHandle(file);
    }catch(...){}
}
void LanguageBar::refreshMenu() {
    menu_={{1,L"方案管理"},{2,L"输入设置"}};
    if(root_.empty())return;
    const auto text=readConfiguration(root_/L"config.txt");
    const auto current=currentSchemaSetting(text);
    auto source=configurationValue(text,u"码表存储位置");
    auto folder=source.empty()?root_/L"码表":std::filesystem::path(source);
    if(folder.is_relative())folder=root_/folder;
    auto names=schemaNames(root_);
    std::error_code error;
    for(std::filesystem::directory_iterator it(folder,error),end;!error && it!=end;it.increment(error)) {
        const auto name=it->path().filename().u16string();
        if(it->is_directory(error) && validSchemaName(name))names.push_back(name);
    }
    std::sort(names.begin(),names.end(),[](const auto& a,const auto& b){return ordinalCompareIgnoreCase(a,b)<0;});
    names.erase(std::unique(names.begin(),names.end(),[](const auto& a,const auto& b){return ordinalCompareIgnoreCase(a,b)==0;}),names.end());
    auto wide=[](std::u16string_view value){return std::wstring(reinterpret_cast<const wchar_t*>(value.data()),value.size());};
    MenuEntry schemas{10,L"方案"},themes{11,L"主题"};
    UINT id=100;
    for(const auto& name:names)schemas.children.push_back({id++,wide(name),L"use",wide(name),ordinalCompareIgnoreCase(name,current.empty()?u"虎码字词":current)==0});
    const auto theme=parseCandidateStyle(text).theme;
    for(const auto name:candidateThemeNames)themes.children.push_back({id++,wide(name),L"theme",wide(name),name==theme});
    menu_={{7,L"虎爪 Github 页面",L"official"},{},
        {3,L"方案文件夹",L"folder"},{6,L"导出码表",L"export"},{4,L"重载码表",L"reload"},
        {5,L"加词"},std::move(schemas),std::move(themes),{1,L"方案管理"},{2,L"输入设置"},{},
        {8,L"切换到英文"}};
}
HRESULT LanguageBar::InitMenu(ITfMenu* menu) {
    traceMenu("InitMenu");
    if(!menu)return E_POINTER;
    if(secure_ || !manager_)return S_FALSE;
    try {
        refreshMenu();
        std::function<HRESULT(ITfMenu*,const std::vector<MenuEntry>&)> append;
        append=[&](ITfMenu* target,const std::vector<MenuEntry>& entries) {
            for(const auto& entry:entries) {
                Microsoft::WRL::ComPtr<ITfMenu> child;
                ITfMenu* rawChild=nullptr;
                DWORD flags=entry.id==0?TF_LBMENUF_SEPARATOR:entry.children.empty()?0:TF_LBMENUF_SUBMENU;
                if(entry.checked)flags|=TF_LBMENUF_CHECKED;
                const auto hr=target->AddMenuItem(entry.id,flags,nullptr,nullptr,entry.text.c_str(),static_cast<ULONG>(entry.text.size()),entry.children.empty()?nullptr:&rawChild);
                child.Attach(rawChild);
                if(FAILED(hr))return hr;
                if(child){const auto nested=append(child.Get(),entry.children);if(FAILED(nested))return nested;}
            }
            return S_OK;
        };
        return append(menu,menu_);
    }catch(...){return E_FAIL;}
}
HRESULT LanguageBar::OnMenuSelect(UINT id) {
    if(secure_ || !manager_)return S_FALSE;
    try {
        if(id==5){if(!addWord_)return S_FALSE;auto callback=addWord_;callback();return S_OK;}
        if(id==8){if(!compartment_)return S_FALSE;VARIANT value;VariantInit(&value);value.vt=VT_I4;value.lVal=0;return compartment_->SetValue(client_,&value);}
        std::wstring action,value;
        if(id!=1 && id!=2) {
            const MenuEntry* selected=nullptr;
            for(const auto& entry:menu_) {
                if(entry.id==id)selected=&entry;
                for(const auto& child:entry.children)if(child.id==id)selected=&child;
            }
            if(!selected || selected->action.empty())return E_INVALIDARG;
            action=selected->action;value=selected->value;
        }
        if(action==L"theme") {
            const std::u16string theme(reinterpret_cast<const char16_t*>(value.data()),value.size());
            if(std::find(candidateThemeNames.begin(),candidateThemeNames.end(),theme)==candidateThemeNames.end())return E_INVALIDARG;
            // A UWP host cannot reliably launch the desktop manager. Theme
            // changes are data-only and use the same atomic writer here.
            updateConfigurationValues(root_/L"config.txt",{{u"主题",theme}});
            return S_OK;
        }
        wchar_t modulePath[32768];const auto length=GetModuleFileNameW(module_,modulePath,32768);
        if(!length || length>=32768)return E_FAIL;
        const auto directory=std::filesystem::path(modulePath).parent_path();
        const auto executable=directory/L"schema_manager.exe";
        // Windows command-line quoting, including trailing backslashes.
        auto quote=[](std::wstring_view arg) {
            std::wstring out=L"\"";std::size_t slashes=0;
            for(auto ch:arg){if(ch==L'\\'){++slashes;continue;}out.append(slashes*(ch==L'"'?2:1),L'\\');slashes=0;if(ch==L'"')out+=L'\\';out+=ch;}
            out.append(slashes*2,L'\\');return out+L"\"";
        };
        auto command=quote(executable.wstring())+(id==2?L" --settings":L"");
        if(id!=1 && id!=2)command+=L" --menu-action "+quote(action)+L" "+quote(value);
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,directory.c_str(),&startup,&process))
            return HRESULT_FROM_WIN32(GetLastError());
        CloseHandle(process.hThread);CloseHandle(process.hProcess);return S_OK;
    }catch(...){return E_FAIL;}
}
HRESULT LanguageBar::showMenu(POINT point,HWND candidateOwner) {
    traceMenu(candidateOwner?"CandidateMenu":"TaskbarMenu");
    if(!secure_ && manager_) {
        Microsoft::WRL::ComPtr<ITfLangBarItemButton> alive(this);
        HMENU menu=CreatePopupMenu();if(!menu)return E_FAIL;
        try {
            refreshMenu();
            std::function<bool(HMENU,const std::vector<MenuEntry>&)> append;
            append=[&](HMENU target,const std::vector<MenuEntry>& entries) {
                for(const auto& entry:entries) {
                    if(entry.id==0){if(!AppendMenuW(target,MF_SEPARATOR,0,nullptr))return false;continue;}
                    UINT flags=MF_STRING|(entry.checked?MF_CHECKED:0);
                    UINT_PTR item=entry.id;HMENU child=nullptr;
                    if(!entry.children.empty()) {
                        child=CreatePopupMenu();if(!child)return false;
                        if(!append(child,entry.children)){DestroyMenu(child);return false;}
                        flags|=MF_POPUP;item=reinterpret_cast<UINT_PTR>(child);
                    }
                    if(!AppendMenuW(target,flags,item,entry.text.c_str())){if(child)DestroyMenu(child);return false;}
                }
                return true;
            };
            if(!append(menu,menu_)){DestroyMenu(menu);return E_FAIL;}
        }catch(...){traceMenu("BuildMenuFailed",E_FAIL);DestroyMenu(menu);return E_FAIL;}
        // Use the input context's window as the owner, just as the candidate
        // window does. A disconnected hidden desktop popup is unreliable in
        // an immersive host when the click originates on the system taskbar.
        HWND parent=IsWindow(menuParent_)?menuParent_:nullptr;
        HWND owner=candidateOwner?candidateOwner:CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,L"STATIC",L"",WS_POPUP,point.x,point.y,0,0,parent,nullptr,module_,nullptr);
        if(!owner){const auto hr=HRESULT_FROM_WIN32(GetLastError());traceMenu("CreateMenuOwnerFailed",hr);DestroyMenu(menu);return hr;}
        if(!candidateOwner) {
            if(parent)SetForegroundWindow(parent);
            else SetForegroundWindow(owner);
        }
        SetLastError(ERROR_SUCCESS);
        const auto selected=TrackPopupMenuEx(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,point.x,point.y,owner,nullptr);
        traceMenu(selected?"PopupSelected":"PopupEnded",selected?S_OK:HRESULT_FROM_WIN32(GetLastError()));
        if(!candidateOwner)DestroyWindow(owner);
        DestroyMenu(menu);
        const auto hr=selected?OnMenuSelect(selected):S_FALSE;
        if(FAILED(hr))MessageBoxW(nullptr,L"操作失败，请检查用户数据权限；当前应用也可能不允许启动管理程序。",L"原生虎码",MB_OK|MB_ICONERROR);
        return hr;
    }
    return S_FALSE;
}
HRESULT LanguageBar::OnClick(TfLBIClick click,POINT point,const RECT*) {
    traceMenu(click==TF_LBI_CLK_RIGHT?"RightClick":"LeftClick");
    if(click==TF_LBI_CLK_RIGHT)return showMenu(point);
    if(click!=TF_LBI_CLK_LEFT || !enabled_ || !compartment_)return S_FALSE;
    // Keep the compartment alive if its notification deactivates the service.
    auto compartment=compartment_; const auto client=client_;
    VARIANT value;VariantInit(&value);auto hr=compartment->GetValue(&value);
    if(FAILED(hr))return hr;
    if(value.vt!=VT_I4){VariantClear(&value);return DISP_E_TYPEMISMATCH;}
    value.lVal=value.lVal?0:1;return compartment->SetValue(client,&value);
}
HRESULT LanguageBar::AdviseSink(REFIID iid,IUnknown* object,DWORD* cookie) {
    if(!object || !cookie)return E_POINTER;*cookie=TF_INVALID_COOKIE;
    if(iid!=IID_ITfLangBarItemSink)return CONNECT_E_CANNOTCONNECT;
    if(sink_)return CONNECT_E_ADVISELIMIT;
    auto hr=object->QueryInterface(IID_PPV_ARGS(&sink_));if(SUCCEEDED(hr))*cookie=1;return hr;
}
HRESULT LanguageBar::UnadviseSink(DWORD cookie){if(cookie!=1 || !sink_)return CONNECT_E_NOCONNECTION;sink_.Reset();return S_OK;}
}
