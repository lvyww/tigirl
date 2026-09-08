#include "LanguageBar.h"
#include "../../SampleIME/resource.h"
#include <cwchar>
#include <filesystem>
#include <string>
#include <ctffunc.h>
#include <olectl.h>
extern void DllAddRef();
extern void DllRelease();
namespace tiger::tsf {
LanguageBar::LanguageBar(HINSTANCE module,REFCLSID service,bool secure):module_(module),secure_(secure) {
    DllAddRef();
    info_.clsidService=service; info_.guidItem=ItemId;
    info_.dwStyle=TF_LBI_STYLE_BTN_BUTTON;
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
    enabled_=false; compartment_.Reset(); client_=TF_CLIENTID_NULL;
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
HRESULT LanguageBar::InitMenu(ITfMenu* menu) {
    if(!menu)return E_POINTER;
    if(secure_ || !manager_)return S_FALSE;
    auto hr=menu->AddMenuItem(1,0,nullptr,nullptr,L"方案管理",4,nullptr);
    if(SUCCEEDED(hr))hr=menu->AddMenuItem(2,0,nullptr,nullptr,L"输入设置",4,nullptr);
    return hr;
}
HRESULT LanguageBar::OnMenuSelect(UINT id) {
    if(id!=1 && id!=2)return E_INVALIDARG;
    if(secure_ || !manager_)return S_FALSE;
    try {
        wchar_t modulePath[32768];const auto length=GetModuleFileNameW(module_,modulePath,32768);
        if(!length || length>=32768)return E_FAIL;
        const auto directory=std::filesystem::path(modulePath).parent_path();
        const auto executable=directory/L"schema_manager.exe";
        auto command=L"\""+executable.wstring()+L"\""+(id==2?L" --settings":L"");
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,directory.c_str(),&startup,&process))
            return HRESULT_FROM_WIN32(GetLastError());
        CloseHandle(process.hThread);CloseHandle(process.hProcess);return S_OK;
    }catch(...){return E_FAIL;}
}
HRESULT LanguageBar::OnClick(TfLBIClick click,POINT point,const RECT* area) {
    if(click==TF_LBI_CLK_RIGHT && area && !secure_ && manager_) {
        Microsoft::WRL::ComPtr<ITfLangBarItemButton> alive(this);
        HMENU menu=CreatePopupMenu();if(!menu)return E_FAIL;
        AppendMenuW(menu,MF_STRING,1,L"方案管理");AppendMenuW(menu,MF_STRING,2,L"输入设置");
        HWND owner=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"",WS_POPUP,0,0,0,0,nullptr,nullptr,module_,nullptr);
        if(!owner){DestroyMenu(menu);return E_FAIL;}
        SetForegroundWindow(owner);
        const auto selected=TrackPopupMenuEx(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,point.x,point.y,owner,nullptr);
        DestroyWindow(owner);DestroyMenu(menu);
        const auto hr=selected?OnMenuSelect(selected):S_FALSE;
        if(FAILED(hr))MessageBoxW(nullptr,L"无法打开管理程序，请检查原生虎码安装是否完整。",L"原生虎码",MB_OK|MB_ICONERROR);
        return hr;
    }
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
