// A real Windows TSF manager/context backed by a small application text store.
// The registered native profile is activated for this process only.
#define NOMINMAX
#include <map>
#include <windows.h>
#include <msctf.h>
#include <ctffunc.h>
#include <ctfutb.h>
#include "../native/tsf/LanguageBar.h"
#include <TextStor.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <dwmapi.h>
#include <filesystem>
#include <wrl/client.h>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <array>
#include <functional>
#include <string>
#include <stdexcept>

template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
static void checked(HRESULT hr,const char* expression,int line) {
    if(FAILED(hr)) {
        std::string description;
        ComPtr<IErrorInfo> info;
        if(GetErrorInfo(0,&info)==S_OK && info) {
            BSTR value=nullptr;
            if(SUCCEEDED(info->GetDescription(&value)) && value) {
                for(UINT i=0;i<SysStringLen(value);++i) description+=static_cast<char>(value[i]);
                SysFreeString(value);
            }
        }
        char text[80]; sprintf_s(text,"line %d HRESULT 0x%08lx: ",line,static_cast<unsigned long>(hr));
        throw std::runtime_error(std::string(text)+expression+" / "+description);
    }
}
#define check(expression) checked((expression),#expression,__LINE__)
static void require(bool value,const char* reason) { if(!value) throw std::runtime_error(reason); }
class LocalActivation {
public:
    explicit LocalActivation(const wchar_t* manifest) {
        if(!manifest) return;
        ACTCTXW context{}; context.cbSize=sizeof(context); context.lpSource=manifest;
        handle_=CreateActCtxW(&context);
        if(handle_==INVALID_HANDLE_VALUE) check(HRESULT_FROM_WIN32(GetLastError()));
        if(!ActivateActCtx(handle_,&cookie_)) {
            const auto hr=HRESULT_FROM_WIN32(GetLastError()); ReleaseActCtx(handle_); handle_=INVALID_HANDLE_VALUE; check(hr);
        }
    }
    ~LocalActivation() { if(handle_!=INVALID_HANDLE_VALUE) { DeactivateActCtx(0,cookie_); ReleaseActCtx(handle_); } }
private: HANDLE handle_=INVALID_HANDLE_VALUE; ULONG_PTR cookie_=0;
};
static void verifyModule(HMODULE expected) {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());
    require(snapshot!=INVALID_HANDLE_VALUE,"Cannot inspect loaded modules");
    MODULEENTRY32W item{}; item.dwSize=sizeof(item); bool found=false,unexpected=false;
    for(BOOL next=Module32FirstW(snapshot,&item);next;next=Module32NextW(snapshot,&item)) {
        if(_wcsicmp(item.szModule,L"Tigirl.dll")==0) {
            if(item.hModule==expected) found=true; else unexpected=true;
        }
    }
    CloseHandle(snapshot);
    require(found && !unexpected,"TSF loaded another SampleIME DLL generation");
}
static void verifyMappedDictionary(const std::filesystem::path& dictionary,std::vector<PSAPI_WORKING_SET_EX_INFORMATION>* pages=nullptr) {
    wchar_t path[32768];
    auto file=dictionary.wstring();
    require(file.size()>2 && file[1]==L':',"Dictionary verification requires a local drive path");
    wchar_t device[32768]; require(QueryDosDeviceW(file.substr(0,2).c_str(),device,32768)!=0,"Cannot resolve dictionary volume");
    const auto expected=std::wstring(device)+file.substr(2);
    SYSTEM_INFO system{}; GetSystemInfo(&system);
    auto address=reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress);
    const auto maximum=reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress);
    bool found=false;
    while(address<maximum) {
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region))) break;
        if(region.State==MEM_COMMIT && region.Type==MEM_MAPPED &&
           GetMappedFileNameW(GetCurrentProcess(),region.BaseAddress,path,32768) && _wcsicmp(path,expected.c_str())==0) {
            found=true;
            if(pages) {
                volatile unsigned char touch=0;
                for(std::size_t offset=0;offset<region.RegionSize;offset+=system.dwPageSize) {
                    auto pointer=static_cast<unsigned char*>(region.BaseAddress)+offset;
                    touch=static_cast<unsigned char>(touch^*pointer);
                    PSAPI_WORKING_SET_EX_INFORMATION page{};page.VirtualAddress=pointer;pages->push_back(page);
                }
                (void)touch;
            }
            break;
        }
        const auto next=reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize;
        if(next<=address) break;
        address=next;
    }
    require(found,"The tested TSF service has not mapped its dictionary");
}
static void verifyDictionary(HMODULE module,std::vector<PSAPI_WORKING_SET_EX_INFORMATION>* pages=nullptr) {
    wchar_t root[32768]{};const auto length=GetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",root,32768);
    if(length && length<32768 && std::filesystem::exists(std::filesystem::path(root)/L".builtin-override-tsf-test")) {
        verifyMappedDictionary(std::filesystem::path(root)/L"schemas"/L"虎码字词"/L"generations"/L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"/L"tiger-v2.tcd",pages);
        return;
    }
    wchar_t path[32768]; require(GetModuleFileNameW(module,path,32768)!=0,"Cannot locate tested module");
    verifyMappedDictionary(std::filesystem::path(path).parent_path()/L"tiger-v2.tcd",pages);
}

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

struct TerminationObservation {std::array<void*,32> frames{};USHORT count=0;unsigned starts=0,ends=0;};
static std::array<TerminationObservation,32> terminationObservations;
static std::size_t terminationObservationCount=0;
static bool recordTerminationStacks=false;
static void dumpTerminationObservations() {
    for(std::size_t i=0;i<terminationObservationCount;++i) {
        const auto& record=terminationObservations[i];
        std::cerr<<"end-stack starts="<<record.starts<<" ends="<<record.ends<<" frames=";
        for(USHORT j=0;j<record.count;++j) {
            HMODULE module=nullptr;wchar_t path[MAX_PATH]{};
            if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(record.frames[j]),&module) && GetModuleFileNameW(module,path,MAX_PATH))
                std::cerr<<std::filesystem::path(path).filename().string()<<"+"<<
                    (reinterpret_cast<std::uintptr_t>(record.frames[j])-reinterpret_cast<std::uintptr_t>(module))<<";";
        }
        std::cerr<<"\n";
    }
}
class TextStore final : public ITextStoreACP, public ITfContextOwnerCompositionSink {
public:
    std::wstring text;
    TS_SELECTION_ACP selection{0,0,{TS_AE_END,FALSE}};
    DWORD lock=0;
    bool rejectLocks=false,deferLocks=false;
    unsigned rejectedLocks=0;
    DWORD pendingLock=0;
    HRESULT grantPendingLock() {
        const auto flags=pendingLock; pendingLock=0;
        if(!flags || lock || !sink)return E_UNEXPECTED;
        lock=flags;const auto hr=sink->OnLockGranted(flags);lock=0;return hr;
    }
    unsigned compositionStarts=0,compositionEnds=0;
    bool layoutReady=true;
    HWND window;
    ComPtr<ITextStoreACPSink> sink;
    explicit TextStore(HWND w):window(w) {}
    STDMETHODIMP QueryInterface(REFIID iid,void** out) override {
        if(!out) return E_POINTER;
        *out=nullptr;
        if(iid==IID_IUnknown || iid==IID_ITextStoreACP)*out=static_cast<ITextStoreACP*>(this);
        else if(iid==IID_ITfContextOwnerCompositionSink)
            *out=static_cast<ITfContextOwnerCompositionSink*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    STDMETHODIMP_(ULONG) Release() override { const auto n=--refs_; if(!n) delete this; return n; }
    STDMETHODIMP OnStartComposition(ITfCompositionView*,BOOL* allow) override {
        if(!allow)return E_POINTER;*allow=TRUE;++compositionStarts;
        if(GetEnvironmentVariableW(L"NATIVE_TIGER_TEST_TRACE",nullptr,0)!=0)
            std::cerr<<"composition-start hwnd="<<window<<" lock="<<lock<<"\n";
        return S_OK;
    }
    STDMETHODIMP OnUpdateComposition(ITfCompositionView*,ITfRange*) override {
        if(GetEnvironmentVariableW(L"NATIVE_TIGER_TEST_TRACE",nullptr,0)!=0)
            std::cerr<<"composition-update hwnd="<<window<<" lock="<<lock<<"\n";
        return S_OK;
    }
    STDMETHODIMP OnEndComposition(ITfCompositionView*) override {
        ++compositionEnds;
        if(recordTerminationStacks && terminationObservationCount<terminationObservations.size()) {
            auto& record=terminationObservations[terminationObservationCount++];
            record.starts=compositionStarts;record.ends=compositionEnds;
            record.count=CaptureStackBackTrace(0,static_cast<DWORD>(record.frames.size()),record.frames.data(),nullptr);
        }
        if(GetEnvironmentVariableW(L"NATIVE_TIGER_TEST_TRACE",nullptr,0)==0)return S_OK;
        std::cerr<<"composition-end hwnd="<<window<<" lock="<<lock<<" stack=";
        void* frames[24]{};const auto count=GetEnvironmentVariableW(L"NATIVE_TIGER_TEST_STACK_TRACE",nullptr,0)!=0?CaptureStackBackTrace(0,24,frames,nullptr):0;
        for(USHORT i=0;i<count;++i) {
            HMODULE module=nullptr;wchar_t name[MAX_PATH]{};
            if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(frames[i]),&module) && GetModuleFileNameW(module,name,MAX_PATH)) {
                const auto base=std::filesystem::path(name).filename().string();
                std::cerr<<base<<"+"<<(reinterpret_cast<std::uintptr_t>(frames[i])-reinterpret_cast<std::uintptr_t>(module))<<";";
            }
        }
        std::cerr<<"\n";return S_OK;
    }
    STDMETHODIMP AdviseSink(REFIID iid,IUnknown* object,DWORD) override {
        if(iid!=IID_ITextStoreACPSink) return E_INVALIDARG;
        return object->QueryInterface(IID_PPV_ARGS(sink.ReleaseAndGetAddressOf()));
    }
    STDMETHODIMP UnadviseSink(IUnknown*) override { sink.Reset(); return S_OK; }
    STDMETHODIMP RequestLock(DWORD flags,HRESULT* result) override {
        if(GetEnvironmentVariableW(L"NATIVE_TIGER_TEST_TRACE",nullptr,0)!=0)
            std::cerr<<"lock-request flags="<<flags<<" held="<<lock<<" defer="<<deferLocks<<" reject="<<rejectLocks<<"\n";
        if(!result) return E_POINTER;
        if(rejectLocks) { ++rejectedLocks; *result=E_FAIL;return E_FAIL; }
        if(deferLocks) {
            if(flags&TS_LF_SYNC) { *result=TS_E_SYNCHRONOUS;return S_OK; }
            pendingLock=flags;*result=TS_S_ASYNC;return S_OK;
        }
        if(lock) {
            if(flags&TS_LF_SYNC) { *result=TS_E_SYNCHRONOUS; return S_OK; }
            queuedLock_|=flags; *result=TS_S_ASYNC; return S_OK;
        }
        lock=flags;
        *result=sink?sink->OnLockGranted(flags):E_UNEXPECTED;
        lock=0;
        while(queuedLock_) {
            const auto queued=queuedLock_;queuedLock_=0;lock=queued;
            if(sink)sink->OnLockGranted(queued);
            lock=0;
        }
        return S_OK;
    }
    STDMETHODIMP GetStatus(TS_STATUS* status) override { *status={0,TS_SS_NOHIDDENTEXT}; return S_OK; }
    STDMETHODIMP QueryInsert(LONG start,LONG end,ULONG,LONG* outStart,LONG* outEnd) override {
        if(start<0 || end<start || end>static_cast<LONG>(text.size())) return TS_E_INVALIDPOS;
        *outStart=start; *outEnd=end; return S_OK;
    }
    STDMETHODIMP GetSelection(ULONG index,ULONG count,TS_SELECTION_ACP* result,ULONG* fetched) override {
        if(!lock) return TS_E_NOLOCK;
        *fetched=0;
        if(!count || (index!=0 && index!=TS_DEFAULT_SELECTION)) return E_INVALIDARG;
        *result=selection; *fetched=1; return S_OK;
    }
    STDMETHODIMP SetSelection(ULONG count,const TS_SELECTION_ACP* value) override {
        if((lock&TS_LF_READWRITE)!=TS_LF_READWRITE) return TS_E_NOLOCK;
        if(count!=1) return E_INVALIDARG;
        selection=*value; return S_OK;
    }
    STDMETHODIMP GetText(LONG start,LONG end,WCHAR* plain,ULONG capacity,ULONG* fetched,
        TS_RUNINFO* runs,ULONG runCapacity,ULONG* runCount,LONG* next) override {
        if(!lock) return TS_E_NOLOCK;
        if(end==-1) end=static_cast<LONG>(text.size());
        if(start<0 || end<start || end>static_cast<LONG>(text.size())) return TS_E_INVALIDPOS;
        *fetched=std::min(capacity,static_cast<ULONG>(end-start));
        if(*fetched) std::copy_n(text.data()+start,*fetched,plain);
        *runCount=runCapacity && end>start?1:0;
        if(*runCount) runs[0]={capacity?*fetched:static_cast<ULONG>(end-start),TS_RT_PLAIN};
        *next=start+static_cast<LONG>(capacity?*fetched:static_cast<ULONG>(end-start));
        return S_OK;
    }
    STDMETHODIMP SetText(DWORD,LONG start,LONG end,const WCHAR* value,ULONG count,TS_TEXTCHANGE* change) override {
        if((lock&TS_LF_READWRITE)!=TS_LF_READWRITE) return TS_E_NOLOCK;
        if(start<0 || end<start || end>static_cast<LONG>(text.size())) return TS_E_INVALIDPOS;
        text.replace(static_cast<std::size_t>(start),static_cast<std::size_t>(end-start),value?value:L"",count);
        *change={start,end,start+static_cast<LONG>(count)};
        selection={change->acpNewEnd,change->acpNewEnd,{TS_AE_END,FALSE}};
        return S_OK;
    }
    STDMETHODIMP GetFormattedText(LONG,LONG,IDataObject** out) override { *out=nullptr; return E_NOTIMPL; }
    STDMETHODIMP GetEmbedded(LONG,REFGUID,REFIID,IUnknown** out) override { *out=nullptr; return E_NOTIMPL; }
    STDMETHODIMP QueryInsertEmbedded(const GUID*,const FORMATETC*,BOOL* value) override { *value=FALSE; return S_OK; }
    STDMETHODIMP InsertEmbedded(DWORD,LONG,LONG,IDataObject*,TS_TEXTCHANGE*) override { return E_NOTIMPL; }
    STDMETHODIMP InsertTextAtSelection(DWORD flags,const WCHAR* value,ULONG count,LONG* start,LONG* end,TS_TEXTCHANGE* change) override {
        if(!lock) return TS_E_NOLOCK;
        const auto from=selection.acpStart,to=selection.acpEnd;
        if(start) *start=from;
        if(end) *end=to;
        if(flags&TS_IAS_QUERYONLY) return S_OK;
        auto hr=SetText(0,from,to,value,count,change);
        if(SUCCEEDED(hr) && end) *end=change->acpNewEnd;
        return hr;
    }
    STDMETHODIMP InsertEmbeddedAtSelection(DWORD,IDataObject*,LONG*,LONG*,TS_TEXTCHANGE*) override { return E_NOTIMPL; }
    STDMETHODIMP RequestSupportedAttrs(DWORD,ULONG,const TS_ATTRID*) override { return S_OK; }
    STDMETHODIMP RequestAttrsAtPosition(LONG,ULONG,const TS_ATTRID*,DWORD) override { return S_OK; }
    STDMETHODIMP RequestAttrsTransitioningAtPosition(LONG,ULONG,const TS_ATTRID*,DWORD) override { return S_OK; }
    STDMETHODIMP FindNextAttrTransition(LONG,LONG halt,ULONG,const TS_ATTRID*,DWORD,LONG* next,BOOL* found,LONG* offset) override {
        *next=halt; *found=FALSE; *offset=0; return S_OK;
    }
    STDMETHODIMP RetrieveRequestedAttrs(ULONG,TS_ATTRVAL*,ULONG* fetched) override { *fetched=0; return S_OK; }
    STDMETHODIMP GetEndACP(LONG* end) override { if(!lock) return TS_E_NOLOCK; *end=static_cast<LONG>(text.size()); return S_OK; }
    STDMETHODIMP GetActiveView(TsViewCookie* view) override { *view=1; return S_OK; }
    STDMETHODIMP GetACPFromPoint(TsViewCookie,const POINT*,DWORD,LONG* value) override { *value=selection.acpEnd; return S_OK; }
    STDMETHODIMP GetTextExt(TsViewCookie,LONG,LONG,RECT* rect,BOOL* clipped) override {
        if(GetEnvironmentVariableW(L"NATIVE_TIGER_TEST_TRACE",nullptr,0)!=0)
            std::cerr<<"text-ext ready="<<layoutReady<<" lock="<<lock<<"\n";
        if(!layoutReady) return TS_E_NOLAYOUT;
        *rect={100,100,102,125}; *clipped=FALSE; return S_OK;
    }
    STDMETHODIMP GetScreenExt(TsViewCookie,RECT* rect) override { *rect={100,100,700,600}; return S_OK; }
    STDMETHODIMP GetWnd(TsViewCookie,HWND* value) override { *value=window; return S_OK; }
private:
    DWORD queuedLock_=0;
    ULONG refs_=1;
};
class UISink final : public ITfUIElementSink {
public:
    bool nativeUI=false;
    DWORD id=TF_INVALID_UIELEMENTID;
    UINT begins=0,updates=0,ends=0;
    STDMETHODIMP QueryInterface(REFIID iid,void** out) override {
        if(!out) return E_POINTER;
        *out=nullptr;
        if(iid!=IID_IUnknown && iid!=IID_ITfUIElementSink) return E_NOINTERFACE;
        *out=static_cast<ITfUIElementSink*>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    STDMETHODIMP_(ULONG) Release() override { auto n=--refs_; if(!n) delete this; return n; }
    STDMETHODIMP BeginUIElement(DWORD value,BOOL* show) override { id=value; *show=nativeUI?TRUE:FALSE; ++begins; return S_OK; }
    STDMETHODIMP UpdateUIElement(DWORD value) override { id=value; ++updates; return S_OK; }
    STDMETHODIMP EndUIElement(DWORD) override { id=TF_INVALID_UIELEMENTID; ++ends; return S_OK; }
private: ULONG refs_=1;
};
struct Document {
    ComPtr<TextStore> store;
    ComPtr<ITfDocumentMgr> manager;
    ComPtr<ITfContext> context;
};
static Document document(ITfThreadMgr* manager,TfClientId client,HWND window) {
    Document doc; doc.store.Attach(new TextStore(window));
    check(manager->CreateDocumentMgr(&doc.manager));
    TfEditCookie cookie;
    check(doc.manager->CreateContext(client,0,static_cast<ITextStoreACP*>(doc.store.Get()),&doc.context,&cookie));
    check(doc.manager->Push(doc.context.Get())); return doc;
}
struct KeyObservation {
    unsigned event=0,vk=0,starts=0,ends=0;bool down=false,after=false,handled=false;
    std::array<wchar_t,16> text{};std::size_t length=0;
};
static std::array<KeyObservation,128> keyObservations;
static std::size_t keyObservationCount=0;
static void observeKey(unsigned event,WPARAM vk,bool down,bool after,bool handled,const Document& doc) {
    auto& value=keyObservations[keyObservationCount++%keyObservations.size()];
    value={};value.event=event;value.vk=static_cast<unsigned>(vk);value.down=down;value.after=after;value.handled=handled;
    value.starts=doc.store->compositionStarts;value.ends=doc.store->compositionEnds;
    value.length=std::min(doc.store->text.size(),value.text.size());
    std::copy_n(doc.store->text.data(),value.length,value.text.data());
}
static void dumpKeyObservations() {
    const auto start=keyObservationCount>keyObservations.size()?keyObservationCount-keyObservations.size():0;
    for(auto i=start;i<keyObservationCount;++i) {
        const auto& value=keyObservations[i%keyObservations.size()];
        std::cerr<<"key-ring event="<<value.event<<" vk="<<value.vk<<" down="<<value.down<<" after="<<value.after<<" handled="<<value.handled
            <<" comp_events="<<value.starts<<","<<value.ends<<" text=";
        for(std::size_t j=0;j<value.length;++j)std::cerr<<static_cast<unsigned>(value.text[j])<<",";
        std::cerr<<"\n";
    }
}
static HWND ownCandidateWindow() {
    HWND window=nullptr;
    while((window=FindWindowExW(nullptr,window,L"NativeTiger.Candidate.v1",nullptr))) {
        DWORD owner=0;GetWindowThreadProcessId(window,&owner);
        if(owner==GetCurrentProcessId())return window;
    }
    return nullptr;
}
static std::function<bool(const MSG&)> queuedKeyDispatch;
static void pump() {
    MSG msg;
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
        if(msg.message>=WM_KEYFIRST && msg.message<=WM_KEYLAST &&
            GetEnvironmentVariableW(L"NATIVE_TIGER_TEST_TRACE",nullptr,0)!=0)
            std::cerr<<"queued-key message="<<msg.message<<" vk="<<msg.wParam<<" time="<<msg.time<<" hwnd="<<msg.hwnd<<"\n";
        if(queuedKeyDispatch && queuedKeyDispatch(msg))continue;
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
}
static unsigned timerWindows(const wchar_t* prefix=L"NativeTiger.ManualTimer.") {
    unsigned count=0;
    for(HWND window=nullptr;(window=FindWindowExW(HWND_MESSAGE,window,nullptr,nullptr))!=nullptr;) {
        DWORD process=0;GetWindowThreadProcessId(window,&process);
        wchar_t name[100]{};GetClassNameW(window,name,100);
        if(process==GetCurrentProcessId() && std::wstring(name).find(prefix)==0) ++count;
    }
    return count;
}
static DWORD reminderProcessId=0;
static std::vector<DWORD> reminderChildren() {
    const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    require(snapshot!=INVALID_HANDLE_VALUE,"Cannot inspect reminder child processes");
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);std::vector<DWORD> children;
    if(Process32FirstW(snapshot,&entry))do {
        if(entry.th32ParentProcessID==GetCurrentProcessId() && _wcsicmp(entry.szExeFile,L"Tigirl.Reminder.exe")==0)
            children.push_back(entry.th32ProcessID);
    } while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);return children;
}
struct OwnedReminder {
    HANDLE process=nullptr;
    ~OwnedReminder(){if(process){if(WaitForSingleObject(process,0)==WAIT_TIMEOUT)TerminateProcess(process,1);CloseHandle(process);}}
};
struct ReminderCheck { HWND window=nullptr,button=nullptr; bool text=false; };
static BOOL CALLBACK reminderText(HWND window,LPARAM data) {
    wchar_t text[64]{};GetWindowTextW(window,text,64);
    wchar_t kind[64]{};GetClassNameW(window,kind,64);
    if(std::wstring(kind)==L"Button" && IsWindowEnabled(window)) reinterpret_cast<ReminderCheck*>(data)->button=window;
    if(std::wstring(text)==L"时间差不多咯！") reinterpret_cast<ReminderCheck*>(data)->text=true;
    return TRUE;
}
static BOOL CALLBACK findReminder(HWND window,LPARAM data) {
    DWORD process=0;GetWindowThreadProcessId(window,&process);
    if(process!=GetCurrentProcessId() && (!reminderProcessId || process!=reminderProcessId)) return TRUE;
    wchar_t title[64]{};GetWindowTextW(window,title,64);
    if(std::wstring(title)==L"计时器") {
        reinterpret_cast<ReminderCheck*>(data)->window=window;
        EnumChildWindows(window,reminderText,data);
    }
    return TRUE;
}
struct AddWordCheck { DWORD* lock=nullptr; bool seen=false; int phase=0,ticks=0; std::string error; };
static AddWordCheck* addWordCheck=nullptr;
static BOOL CALLBACK findAddWord(HWND window,LPARAM output) {
    wchar_t title[64]{}; GetWindowTextW(window,title,64);
    if(std::wstring(title)==L"虎娘加词") *reinterpret_cast<HWND*>(output)=window;
    return TRUE;
}
static void CALLBACK saveAddedWord(HWND,UINT,UINT_PTR,DWORD) {
    if(!addWordCheck || addWordCheck->seen) return;
    HWND dialog=nullptr; EnumThreadWindows(GetCurrentThreadId(),findAddWord,reinterpret_cast<LPARAM>(&dialog));
    if(!dialog) return;
    try {
        require(*addWordCheck->lock==0,"Add-word dialog opened inside TSF edit lock");
        auto typeAb=[] {
            BYTE empty[256]{}; SetKeyboardState(empty);
            INPUT keys[6]{};
            const WORD codes[]={'A','B',VK_SPACE};
            for(int i=0;i<3;++i) {
                keys[i*2].type=keys[i*2+1].type=INPUT_KEYBOARD;
                keys[i*2].ki.wVk=keys[i*2+1].ki.wVk=codes[i];
                keys[i*2+1].ki.dwFlags=KEYEVENTF_KEYUP;
            }
            require(SendInput(6,keys,sizeof(INPUT))==6,"Cannot send keys to add-word field");
        };
        if(addWordCheck->phase==0) {
            SetDlgItemTextW(dialog,101,L"");
            SetFocus(GetDlgItem(dialog,101));
            require(GetForegroundWindow()==dialog && GetFocus()==GetDlgItem(dialog,101),"Word field lacks focus for typing");
            typeAb();
            addWordCheck->phase=1; return;
        }
        wchar_t typed[64]{};
        if(addWordCheck->phase==1) {
            GetDlgItemTextW(dialog,101,typed,64);
            if(std::wstring(typed)!=L"交") {
                require(++addWordCheck->ticks<50,"Word field did not commit Chinese from ab+Space"); return;
            }
            SetDlgItemTextW(dialog,102,L""); SetFocus(GetDlgItem(dialog,102));
            require(GetForegroundWindow()==dialog && GetFocus()==GetDlgItem(dialog,102),"Code field lacks focus for typing");
            typeAb(); addWordCheck->phase=2; addWordCheck->ticks=0; return;
        }
        GetDlgItemTextW(dialog,102,typed,64);
        if(std::wstring(typed)!=L"ab ") {
            require(++addWordCheck->ticks<50,"Code field did not bypass Chinese IME"); return;
        }
        addWordCheck->seen=true;
        SetDlgItemTextW(dialog,101,L"原生加词验证");
        SetDlgItemTextW(dialog,102,L"aaaaaa");
        SendMessageW(dialog,WM_COMMAND,IDOK,0);
        require(GetWindowTextLengthW(GetDlgItem(dialog,103))==0,"Add-word save reported an error");
    } catch(const std::exception& e) {
        addWordCheck->seen=true; addWordCheck->error=e.what(); SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);
    }
}
static POINT highlightedCandidate(HWND window) {
    require(window && IsWindowVisible(window),"Candidate window is not visible");
    UpdateWindow(window);
    RECT rect; GetClientRect(window,&rect);
    HDC screen=GetDC(window),dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,rect.right,rect.bottom);
    auto old=SelectObject(dc,bitmap);
    SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
    // Default theme: 0x48000000 selection over 0xfffff8f3 background.
    const auto color=RGB(183,178,174);
    POINT result{-1,-1};
    for(int y=0;y<rect.bottom && result.x<0;y+=2)
        for(int x=0;x<rect.right;x+=2) if(GetPixel(dc,x,y)==color) { result={x,y}; break; }
    SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(window,screen);
    require(result.x>=0,"Cannot locate rendered candidate highlight");
    return result;
}
static void capture(HWND window,const wchar_t* path) {
    struct PhysicalCoordinates {
        DPI_AWARENESS_CONTEXT previous=SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        ~PhysicalCoordinates(){if(previous)SetThreadDpiAwarenessContext(previous);}
    } coordinates;
    require(coordinates.previous!=nullptr,"Cannot select physical screenshot coordinates");
    RECT desktop;require(GetWindowRect(window,&desktop)!=FALSE,"Cannot locate candidate screenshot");
    // The candidate is a borderless popup. GetWindowRect in a DPI-aware scope
    // gives its physical desktop bounds, matching the screen DC pixel grid.
    RECT rect{0,0,desktop.right-desktop.left,desktop.bottom-desktop.top};
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=rect.right; info.bmiHeader.biHeight=-rect.bottom;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    POINT origin{desktop.left,desktop.top};
    HDC screen=GetDC(nullptr),memory=CreateCompatibleDC(screen);
    void* pixels=nullptr;
    HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    require(bitmap && pixels,"Cannot capture candidate UI");
    HGDIOBJ old=SelectObject(memory,bitmap);
    UpdateWindow(window);
    check(DwmFlush());
    require(BitBlt(memory,0,0,rect.right,rect.bottom,screen,origin.x,origin.y,SRCCOPY|CAPTUREBLT)!=FALSE,"Cannot copy composited candidate pixels");
    GdiFlush();
    BITMAPFILEHEADER header{}; header.bfType=0x4d42;
    header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
    header.bfSize=header.bfOffBits+static_cast<DWORD>(rect.right*rect.bottom*4);
    std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char*>(&header),sizeof(header));
    file.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(info.bmiHeader));
    file.write(static_cast<const char*>(pixels),static_cast<std::streamsize>(rect.right)*rect.bottom*4);
    require(file.good(),"Cannot save candidate capture");
    SelectObject(memory,old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(nullptr,screen);
}
static ULONG compositionCount(const Document& doc) {
    ComPtr<ITfContextComposition> context;
    check(doc.context.As(&context));
    ComPtr<IEnumITfCompositionView> values; check(context->EnumCompositions(&values));
    ULONG count=0,fetched=0;
    for(;;) {
        ComPtr<ITfCompositionView> value;
        check(values->Next(1,&value,&fetched));
        if(!fetched) break;
        ++count;
    }
    return count;
}
#include "text_store_lock_fixture.h"
#include "management_menu_fixture.h"
#include "word_save_failure_fixture.h"
#include "selection_race_fixture.h"
#include "sentence_tsf_fixture.h"
int wmain(int argc,wchar_t** argv) {
    // Avoid synchronous stderr pipe writes between keys: they can mask timing
    // failures. Keep diagnostics in memory and emit them only when exiting.
    struct BufferedTrace {
        std::ostringstream buffer;
        std::streambuf* previous=nullptr;
        BufferedTrace() {if(GetEnvironmentVariableW(L"NATIVE_TIGER_TEST_TRACE",nullptr,0)!=0)previous=std::cerr.rdbuf(buffer.rdbuf());}
        ~BufferedTrace() {if(previous){std::cerr.rdbuf(previous);std::cerr<<buffer.str();}}
    } bufferedTrace;
    try {
        if(argc==2 && wcscmp(argv[1],L"--text-store-lock-test")==0) {text_store_lock_fixture();return 0;}
        const bool liveReader=argc==6 && wcscmp(argv[4],L"--live-reader")==0;
        const bool maskDetached=argc==6 && wcscmp(argv[4],L"--mask-detached")==0;
        const bool timerDetached=argc==6 && wcscmp(argv[4],L"--timer-detach")==0;
        const bool wordSaveFailure=argc==6 && wcscmp(argv[4],L"--word-save-failure")==0;
        const bool candidateMouse=argc==6 && wcscmp(argv[4],L"--candidate-mouse")==0;
        const bool managementLaunch=argc==6 && wcscmp(argv[4],L"--management-launch")==0;
        const bool managementMenu=managementLaunch || (argc==6 && wcscmp(argv[4],L"--management-menu")==0);
        const bool activeMemory=argc==6 && wcscmp(argv[4],L"--memory-active")==0;
        const bool memoryProbe=activeMemory || (argc==6 && wcscmp(argv[4],L"--memory")==0);
        if(memoryProbe) {
            const std::filesystem::path root=argv[5];
            require(root.is_absolute() && std::filesystem::exists(root/L".tsf-memory-test"),"Memory probe requires an isolated marked root");
            require(SetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",root.c_str())!=FALSE,"Cannot isolate memory probe");
        }
        if(liveReader || maskDetached || timerDetached || managementMenu || candidateMouse || wordSaveFailure) {
            const std::filesystem::path root=argv[5];
            require(root.is_absolute() && std::filesystem::exists(root/(maskDetached?L".tsf-mask-test":candidateMouse?L".tsf-candidate-mouse-test":wordSaveFailure?L".tsf-word-save-test":managementMenu?L".tsf-management-test":timerDetached?L".tsf-timer-test":L".tsf-live-test")),"Reader requires an isolated marked root");
            require(SetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",root.c_str())!=FALSE,"Cannot isolate live reader");
        }
        require(argc>=2 && (argc<=5 || memoryProbe || liveReader || maskDetached || timerDetached || managementMenu || candidateMouse || wordSaveFailure),"tsf_host <DLL> [capture|-] [manifest] [test mode]");
        const bool schemaActivation=argc==5 && wcscmp(argv[4],L"--schema-activation-only")==0;
        const bool sentenceMeasure=argc==5 && wcscmp(argv[4],L"--sentence-measure")==0;
        const bool sentenceTest=sentenceMeasure || (argc==5 && wcscmp(argv[4],L"--sentence")==0);
        const bool selectionRace=argc==5 && wcscmp(argv[4],L"--selection-race")==0;
        const bool registeredActivation=argc==3 && wcscmp(argv[2],L"--registered-activation-only")==0;
        const bool activationOnly=maskDetached || sentenceTest || selectionRace || registeredActivation || wordSaveFailure || managementLaunch || timerDetached || liveReader || memoryProbe || schemaActivation || (argc==5 && wcscmp(argv[4],L"--activation-only")==0);
        const bool mixed=argc==5 && wcscmp(argv[4],L"--mixed")==0;
        const bool addWord=argc==5 && wcscmp(argv[4],L"--add-word")==0;
        const bool timerTest=argc==5 && wcscmp(argv[4],L"--timer")==0;
        const bool schemaTest=argc==5 && wcscmp(argv[4],L"--schema")==0;
        require(argc!=5 || activationOnly || mixed || addWord || timerTest || schemaTest,"Unknown test option");
        LocalActivation activation(argc>=4?argv[3]:nullptr);
        check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
        const bool nativeUI=!registeredActivation && argc>=3 && wcscmp(argv[2],L"-")!=0;
        auto module=LoadLibraryW(argv[1]); require(module!=nullptr,"Cannot load native TSF DLL");
        auto getClass=reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID,REFIID,void**)>(GetProcAddress(module,"DllGetClassObject"));
        require(getClass!=nullptr,"No class factory export");
        CLSID clsid; check(CLSIDFromString(L"{D2291A80-84D8-4641-9AB2-BDD1472C846B}",&clsid));
        ComPtr<IClassFactory> factory; check(getClass(clsid,IID_PPV_ARGS(&factory)));
        ComPtr<ITfTextInputProcessorEx> service; check(factory->CreateInstance(nullptr,IID_PPV_ARGS(&service)));
        ComPtr<ITfThreadMgrEx> manager;
        check(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager)));
        service.Reset(); // System activation below creates the actual TIP instance.
        GUID profile; check(CLSIDFromString(L"{83955C0E-2C09-47A5-BCF3-F2B98E11EE8B}",&profile));
        ComPtr<ITfInputProcessorProfileMgr> profiles;
        check(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&profiles)));
        TfClientId client; check(manager->ActivateEx(&client,TF_TMAE_NOACTIVATEKEYBOARDLAYOUT));
        ComPtr<ITfSource> source; check(manager.As(&source));
        ComPtr<UISink> ui; ui.Attach(new UISink); DWORD uiCookie;
        ui->nativeUI=nativeUI;
        check(source->AdviseSink(IID_ITfUIElementSink,ui.Get(),&uiCookie));
        ComPtr<ITfKeystrokeMgr> keys; check(manager.As(&keys));
        HWND window=CreateWindowExW(0,L"STATIC",L"NativeTiger integration host",WS_OVERLAPPEDWINDOW,100,100,600,500,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        require(window!=nullptr,"Cannot create host window");
        HWND secondWindow=CreateWindowExW(0,L"STATIC",L"NativeTiger second context",WS_OVERLAPPEDWINDOW,150,150,600,500,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        require(secondWindow!=nullptr,"Cannot create second host window");
        auto first=document(manager.Get(),client,window),second=document(manager.Get(),client,secondWindow);
        ComPtr<ITfDocumentMgr> previousDocument;
        check(manager->AssociateFocus(window,first.manager.Get(),&previousDocument));
        previousDocument.Reset();
        check(manager->AssociateFocus(secondWindow,second.manager.Get(),&previousDocument));
        if(!activationOnly) { ShowWindow(window,SW_SHOWNORMAL); SetForegroundWindow(window); SetFocus(window); }
        check(manager->SetFocus(first.manager.Get())); pump();
        PROCESS_MEMORY_COUNTERS_EX activationBaseline{};activationBaseline.cb=sizeof(activationBaseline);
        if(memoryProbe)require(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&activationBaseline),sizeof(activationBaseline))!=FALSE,"Cannot sample baseline memory");
        check(profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,clsid,profile,nullptr,
            TF_IPPMF_FORPROCESS|TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE));
        pump();
        TF_INPUTPROCESSORPROFILE activeProfile{};
        check(profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&activeProfile));
        require(activeProfile.clsid==clsid,"System did not activate the native profile");
        verifyModule(module);
        if(schemaActivation) {
            wchar_t root[32768]; const auto length=GetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",root,32768);
            require(length>0 && length<32768,"Schema activation requires isolated root");
            require(std::filesystem::exists(std::filesystem::path(root)/L".schema-tsf-test"),"Schema fixture marker missing");
            auto expected=std::filesystem::path(root)/L"schemas"/L"SchemaTest";
            if(std::filesystem::exists(std::filesystem::path(root)/L".generation-tsf-test")) {
                expected/=L"generations";expected/=L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
            }
            verifyMappedDictionary(expected/L"tiger-v2.tcd");
        } else if(!sentenceTest) verifyDictionary(module);
        if(managementMenu) {
            management_fixture::run(manager.Get(),window,std::filesystem::path(argv[5]),!managementLaunch);
            check(first.manager->Pop(TF_POPF_ALL));check(second.manager->Pop(TF_POPF_ALL));
            check(source->UnadviseSink(uiCookie));check(manager->Deactivate());DestroyWindow(secondWindow);DestroyWindow(window);
            const char* popup=managementLaunch?"false":"true";
            std::cout<<"{\"status\":\"passed\",\"management_actions\":2,\"popup_validated\":"<<popup<<",\"os_synthesized_menu_keys\":"<<popup<<",\"child_foreground\":"<<popup<<",\"host_foreground_restored\":"<<popup<<",\"physical_hardware_input\":false}\n";
            return 0;
        }
        if(maskDetached) {
            try {
                check(profiles->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,clsid,profile,nullptr,TF_IPPMF_FORPROCESS));pump();
                // TSF can retain the deactivated profile's taskbar item until a later pump.
                ComPtr<ITfLangBarItemMgr> bars;check(manager.As(&bars));ComPtr<ITfLangBarItem> oldItem;
                if(SUCCEEDED(bars->GetItem(tiger::tsf::LanguageBar::ItemId,&oldItem)))check(bars->RemoveItem(oldItem.Get()));
                check(factory->CreateInstance(nullptr,IID_PPV_ARGS(&service)));
                ComPtr<ITfClientId> ids;check(manager.As(&ids));TfClientId nativeClient;check(ids->GetClientId(clsid,&nativeClient));
                check(service->ActivateEx(manager.Get(),nativeClient,0));
                ComPtr<ITfThreadMgrEventSink> focus;check(service.As(&focus));check(focus->OnSetFocus(first.manager.Get(),nullptr));
                ComPtr<ITfKeyEventSink> input;check(service.As(&input));check(input->OnSetFocus(TRUE));
                BYTE saved[256],empty[256]{};GetKeyboardState(saved);SetKeyboardState(empty);
                auto tapMask=[&](WPARAM key){BOOL eaten=FALSE;check(input->OnTestKeyDown(first.context.Get(),key,1,&eaten));require(eaten,"Mask key not consumed");check(input->OnKeyDown(first.context.Get(),key,1,&eaten));check(input->OnTestKeyUp(first.context.Get(),key,1,&eaten));if(eaten)check(input->OnKeyUp(first.context.Get(),key,1,&eaten));};
                tapMask('A');tapMask('B');require(first.store->text==L"甲😀","Masked preedit differs");
                tapMask(VK_BACK);require(first.store->text==L"甲","Masked backspace differs");tapMask('B');
                ComPtr<ITfUIElementMgr> elements;check(manager.As(&elements));ComPtr<ITfUIElement> element;check(elements->GetUIElement(ui->id,&element));
                ComPtr<ITfCandidateListUIElement> list;check(element.As(&list));BSTR value=nullptr;check(list->GetString(0,&value));
                const bool original=std::wstring(value,SysStringLen(value))==L"交";SysFreeString(value);require(original,"Candidate text was masked");
                tapMask(VK_SPACE);require(first.store->text==L"交" && compositionCount(first)==0,"Masked commit differs");
                tapMask('A');tapMask('B');tapMask('D');tapMask('K');
                require(first.store->text==L"交交😀甲","Mixed resolved prefix was masked");
                tapMask(VK_SPACE);require(first.store->text==L"交交口","Mixed masked commit differs");
                tapMask('A');tapMask(VK_ESCAPE);require(first.store->text==L"交交口","Cancel committed mask");
                SetKeyboardState(saved);check(service->Deactivate());
                std::cout<<"{\"status\":\"passed\",\"masked_preedit\":true,\"backspace\":true,\"original_candidates\":true,\"commit\":true,\"mixed_prefix\":true,\"cancel\":true,\"physical_input\":false}\n";return 0;
            }catch(const std::exception& error){std::cerr<<error.what()<<'\n';throw;}
        }
        if(timerDetached) {
            check(profiles->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,clsid,profile,nullptr,TF_IPPMF_FORPROCESS));pump();
            check(factory->CreateInstance(nullptr,IID_PPV_ARGS(&service)));
            ComPtr<ITfClientId> ids;check(manager.As(&ids));TfClientId nativeClient;check(ids->GetClientId(clsid,&nativeClient));
            check(service->ActivateEx(manager.Get(),nativeClient,0));
            ComPtr<ITfThreadMgrEventSink> focus;check(service.As(&focus));check(focus->OnSetFocus(first.manager.Get(),nullptr));
            ComPtr<ITfKeyEventSink> input;check(service.As(&input));
            BYTE saved[256],empty[256]{},shifted[256]{};
            require(GetKeyboardState(saved)!=FALSE,"Cannot save timer fixture keyboard state");
            shifted[VK_SHIFT]=shifted[VK_LSHIFT]=0x80;
            unsigned callbacks=0;
            auto tapTimer=[&](WPARAM key) {
                BOOL eaten=FALSE;++callbacks;check(input->OnTestKeyDown(first.context.Get(),key,1,&eaten));
                require(eaten!=FALSE,"Detached timer preview did not consume key");
                ++callbacks;check(input->OnKeyDown(first.context.Get(),key,1,&eaten));require(eaten!=FALSE,"Detached timer dispatch did not consume key");
                ++callbacks;check(input->OnTestKeyUp(first.context.Get(),key,1,&eaten));
                if(eaten){++callbacks;check(input->OnKeyUp(first.context.Get(),key,1,&eaten));}
            };
            require(SetKeyboardState(shifted)!=FALSE,"Cannot set shifted timer code");tapTimer('D');
            require(SetKeyboardState(empty)!=FALSE,"Cannot reset timer modifiers");tapTimer('S');tapTimer('1');
            require(first.store->text==L"Ds1" && reminderChildren().empty(),"Detached timer preedit or preview differs");
            const auto started=GetTickCount64();tapTimer(VK_SPACE);
            const auto children=reminderChildren();
            require(first.store->text.empty() && compositionCount(first)==0 && children.size()==1,"Detached timer did not launch one helper without literal commit");
            check(service->Deactivate());input.Reset();focus.Reset();service.Reset();pump();
            require(timerWindows(L"NativeTiger.DataRefresh.")==0,"Deactivated timer host retained data scheduler");
            const auto child=OpenProcess(SYNCHRONIZE,FALSE,children.front());
            require(child!=nullptr,"Reminder vanished during TSF deactivation");
            const auto alive=WaitForSingleObject(child,0)==WAIT_TIMEOUT;CloseHandle(child);
            require(alive,"TSF deactivation cancelled detached reminder");
            require(SetKeyboardState(saved)!=FALSE,"Cannot restore timer keyboard state");
            check(first.manager->Pop(TF_POPF_ALL));check(second.manager->Pop(TF_POPF_ALL));
            check(source->UnadviseSink(uiCookie));check(manager->Deactivate());DestroyWindow(secondWindow);DestroyWindow(window);
            std::cout<<"{\"host_pid\":"<<GetCurrentProcessId()<<",\"helper_pid\":"<<children.front()<<",\"started_ms\":"<<started
                <<",\"explicit_key_callbacks\":"<<callbacks<<",\"tsf_deactivated\":true,\"status\":\"detached\"}"<<std::endl;
            return 0;
        }
        if(liveReader) {
            check(profiles->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,clsid,profile,nullptr,TF_IPPMF_FORPROCESS));pump();
            check(factory->CreateInstance(nullptr,IID_PPV_ARGS(&service)));
            ComPtr<ITfClientId> ids;check(manager.As(&ids));TfClientId nativeClient;check(ids->GetClientId(clsid,&nativeClient));
            check(service->ActivateEx(manager.Get(),nativeClient,0));
            ComPtr<ITfThreadMgrEventSink> focus;check(service.As(&focus));check(focus->OnSetFocus(first.manager.Get(),nullptr));
            ComPtr<ITfKeyEventSink> input;check(service.As(&input));
            BYTE saved[256],empty[256]{};require(GetKeyboardState(saved)!=FALSE && SetKeyboardState(empty)!=FALSE,"Cannot initialize reader keyboard state");
            auto typeCode=[&] {for(WPARAM key:{WPARAM('A'),WPARAM('B')}) {
                BOOL eaten=FALSE;check(input->OnTestKeyDown(first.context.Get(),key,1,&eaten));require(eaten!=FALSE,"Reader preview did not eat code");
                check(input->OnKeyDown(first.context.Get(),key,1,&eaten));require(eaten!=FALSE,"Reader dispatch did not eat code");
                check(input->OnTestKeyUp(first.context.Get(),key,1,&eaten));
                if(eaten)check(input->OnKeyUp(first.context.Get(),key,1,&eaten));
            }};
            typeCode();
            ComPtr<ITfCompartmentMgr> readerCompartments;check(manager.As(&readerCompartments));
            ComPtr<ITfCompartment> readerMode;check(readerCompartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,&readerMode));
            auto isChinese=[&] {VARIANT value;VariantInit(&value);check(readerMode->GetValue(&value));
                const bool chinese=value.vt==VT_I4 && value.lVal!=0;VariantClear(&value);return chinese;};
            const std::filesystem::path root=argv[5],control=root.parent_path()/L"control";
            require(std::filesystem::is_directory(control),"Reader control directory missing");
            ComPtr<ITfUIElementMgr> uiManager;check(manager.As(&uiManager));
            unsigned observed=0;
            ULONGLONG duplicateSince=0;
            std::cout<<"ready"<<std::endl;
            const auto deadline=GetTickCount64()+90000;
            while(!std::filesystem::exists(control/L"stop")) {
                require(GetTickCount64()<deadline,"Live reader timed out waiting for controller");
                pump();
                if(observed<6 || observed>=7)
                    require(first.store->text==L"ab" && compositionCount(first)==1,"Live reader lost active code or composition");
                if(observed>=7)require(isChinese(),"Duplicate save reset manually selected Chinese mode");
                unsigned stage=0,expectedCount=0,expectedPages=0;std::string generation;
                std::ifstream command(control/(L"expected-"+std::to_wstring(observed+1)+L".txt"));
                command>>stage>>expectedCount>>expectedPages>>generation;
                if(command && stage==7 && observed==6 && compositionCount(first)==0 && !isChinese()) {
                    require(first.store->text.empty() && ui->id==TF_INVALID_UIELEMENTID,"Shared save committed raw code or retained candidate UI");
                    VARIANT value;VariantInit(&value);value.vt=VT_I4;value.lVal=1;
                    check(readerMode->SetValue(client,&value));pump();require(isChinese(),"Cannot rearm Chinese mode after shared save");
                    typeCode();observed=7;
                    std::cout<<"{\"stage\":7,\"pid\":"<<GetCurrentProcessId()<<",\"count\":0,\"pages\":0,\"raw_preserved\":false,\"reload_cancelled\":true,\"default_english_applied\":true}"<<std::endl;
                } else if(command && stage>observed && ui->id!=TF_INVALID_UIELEMENTID) {
                    if(stage==8) {
                        if(!duplicateSince)duplicateSince=GetTickCount64();
                        if(GetTickCount64()-duplicateSince<5500){Sleep(10);continue;}
                    }
                    ComPtr<ITfUIElement> element;check(uiManager->GetUIElement(ui->id,&element));
                    ComPtr<ITfCandidateListUIElement> candidates;check(element.As(&candidates));
                    UINT count=0,pages=0;check(candidates->GetCount(&count));check(candidates->GetPageIndex(nullptr,0,&pages));
                    bool hasLiveWord=false;
                    for(UINT index=0;index<count;++index) {
                        BSTR label=nullptr;check(candidates->GetString(index,&label));
                        if(label && std::wstring(label,SysStringLen(label))==L"live0")hasLiveWord=true;
                        SysFreeString(label);
                    }
                    bool mapped=generation=="-";
                    if(!mapped) {
                        try {verifyMappedDictionary(root/L"schemas"/L"SharedTest"/L"generations"/std::filesystem::path(generation)/L"tiger-v2.tcd");mapped=true;}
                        catch(const std::exception&) {}
                    }
                    if(count==expectedCount && pages==expectedPages && mapped && hasLiveWord==(expectedCount==5)) {
                        observed=stage;
                        std::cout<<"{\"stage\":"<<stage<<",\"pid\":"<<GetCurrentProcessId()<<",\"count\":"<<count<<",\"pages\":"<<pages<<",\"raw_preserved\":true}"<<std::endl;
                    }
                }
                Sleep(10);
            }
            require(observed==8,"Reader stopped before all updates arrived");
            require(SetKeyboardState(saved)!=FALSE,"Cannot restore reader keyboard state");
            check(service->Deactivate());service.Reset();
            require(timerWindows(L"NativeTiger.DataRefresh.")==0,"Reader retained refresh scheduler");
            check(first.manager->Pop(TF_POPF_ALL));check(second.manager->Pop(TF_POPF_ALL));
            check(source->UnadviseSink(uiCookie));check(manager->Deactivate());DestroyWindow(secondWindow);DestroyWindow(window);return 0;
        }
        if(memoryProbe) {
            double medianUs=0,p95Us=0,p99Us=0,maxUs=0;
            SIZE_T warmPrivate=0,afterPrivate=0,afterTenThousand=0,afterTwentyThousand=0;
            if(activeMemory) {
                check(profiles->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,clsid,profile,nullptr,TF_IPPMF_FORPROCESS));pump();
                check(factory->CreateInstance(nullptr,IID_PPV_ARGS(&service)));
                ComPtr<ITfClientId> ids;check(manager.As(&ids));TfClientId nativeClient;check(ids->GetClientId(clsid,&nativeClient));
                check(service->ActivateEx(manager.Get(),nativeClient,0));
                ComPtr<ITfThreadMgrEventSink> focus;check(service.As(&focus));check(focus->OnSetFocus(first.manager.Get(),nullptr));
                ComPtr<ITfKeyEventSink> input;check(service.As(&input));
                BYTE saved[256],empty[256]{};require(GetKeyboardState(saved)!=FALSE,"Cannot save benchmark keyboard state");
                require(SetKeyboardState(empty)!=FALSE,"Cannot clear benchmark keyboard state");
                LARGE_INTEGER frequency{};require(QueryPerformanceFrequency(&frequency)!=FALSE,"Cannot read timer frequency");
                std::vector<double> times;times.reserve(15000);
                auto tap=[&](WPARAM vk,bool measured) {
                    const LPARAM flags=1|(static_cast<LPARAM>(MapVirtualKeyW(static_cast<UINT>(vk),MAPVK_VK_TO_VSC))<<16);
                    LARGE_INTEGER start{},finish{};QueryPerformanceCounter(&start);
                    BOOL tested=FALSE,eaten=FALSE;
                    check(input->OnTestKeyDown(first.context.Get(),vk,flags,&tested));
                    require(tested!=FALSE,"Benchmark key was not handled");
                    check(input->OnKeyDown(first.context.Get(),vk,flags,&eaten));require(eaten!=FALSE,"Benchmark key dispatch failed");
                    const LPARAM released=flags|static_cast<LPARAM>(0xc0000000u);
                    check(input->OnTestKeyUp(first.context.Get(),vk,released,&tested));
                    if(tested)check(input->OnKeyUp(first.context.Get(),vk,released,&eaten));
                    QueryPerformanceCounter(&finish);
                    if(measured)times.push_back((finish.QuadPart-start.QuadPart)*1000000.0/frequency.QuadPart);
                };
                auto privateUsage=[] {
                    PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);
                    require(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters))!=FALSE,"Cannot measure active TSF heap");
                    return counters.PrivateUsage;
                };
                for(int word=0;word<100;++word){tap('A',false);tap('B',false);tap(VK_SPACE,false);}
                warmPrivate=privateUsage();
                for(int word=0;word<5000;++word){tap('A',true);tap('B',true);tap(VK_SPACE,true);}
                afterPrivate=privateUsage();
                for(int word=0;word<5000;++word){tap('A',false);tap('B',false);tap(VK_SPACE,false);}
                afterTenThousand=privateUsage();
                for(int word=0;word<10000;++word){tap('A',false);tap('B',false);tap(VK_SPACE,false);}
                afterTwentyThousand=privateUsage();
                require(ui->begins==ui->ends,"Active benchmark retained UI elements");
                require(first.store->text==std::wstring(20100,L'交'),"Active TSF benchmark lost or duplicated committed text");
                require(compositionCount(first)==0,"Active TSF benchmark retained a composition");
                require(SetKeyboardState(saved)!=FALSE,"Cannot restore benchmark keyboard state");
                std::sort(times.begin(),times.end());medianUs=times[times.size()/2];p95Us=times[times.size()*95/100];p99Us=times[times.size()*99/100];maxUs=times.back();
            }
            std::vector<PSAPI_WORKING_SET_EX_INFORMATION> pages;verifyDictionary(module,&pages);
            std::cout<<"ready"<<std::endl;std::string command;
            require(static_cast<bool>(std::getline(std::cin,command)),"Memory measurement cancelled");
            require(QueryWorkingSetEx(GetCurrentProcess(),pages.data(),static_cast<DWORD>(pages.size()*sizeof(pages[0])))!=FALSE,"Cannot inspect TSF dictionary pages");
            std::size_t resident=0,shared=0,multiple=0;
            for(const auto& page:pages)if(page.VirtualAttributes.Valid){++resident;if(page.VirtualAttributes.Shared)++shared;if(page.VirtualAttributes.ShareCount>1)++multiple;}
            PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
            require(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))!=FALSE,"Cannot sample TSF memory");
            std::cout<<"{\"pid\":"<<GetCurrentProcessId()<<",\"baseline_private_bytes\":"<<activationBaseline.PrivateUsage
                <<",\"private_bytes\":"<<memory.PrivateUsage<<",\"working_set_bytes\":"<<memory.WorkingSetSize
                <<",\"active_input\":"<<(activeMemory?"true":"false")<<",\"warm_private_bytes\":"<<warmPrivate<<",\"after_5000_words_private_bytes\":"<<afterPrivate
                <<",\"after_10000_words_private_bytes\":"<<afterTenThousand<<",\"after_20000_words_private_bytes\":"<<afterTwentyThousand
                <<",\"measured_taps\":"<<(activeMemory?15000:0)<<",\"tap_median_us\":"<<medianUs<<",\"tap_p95_us\":"<<p95Us<<",\"tap_p99_us\":"<<p99Us<<",\"tap_max_us\":"<<maxUs
                <<",\"probe_page_buffer_bytes\":"<<pages.capacity()*sizeof(pages[0])<<",\"dictionary_pages\":"<<pages.size()
                <<",\"resident_pages\":"<<resident<<",\"shareable_pages\":"<<shared<<",\"multiply_shared_pages\":"<<multiple<<"}"<<std::endl;
            require(static_cast<bool>(std::getline(std::cin,command)),"Memory hold cancelled");
            if(activeMemory){check(service->Deactivate());service.Reset();}
            else {check(profiles->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,clsid,profile,nullptr,TF_IPPMF_FORPROCESS));pump();}
            check(first.manager->Pop(TF_POPF_ALL));check(second.manager->Pop(TF_POPF_ALL));
            check(source->UnadviseSink(uiCookie));check(manager->Deactivate());DestroyWindow(secondWindow);DestroyWindow(window);return 0;
        }
        if(activationOnly) {
            ComPtr<ITfCompartmentMgr> compartments; check(manager.As(&compartments));
            ComPtr<ITfCompartment> openMode,conversionMode;
            check(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,&openMode));
            check(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,&conversionMode));
            auto readMode=[](ITfCompartment* item) {
                VARIANT value; VariantInit(&value); check(item->GetValue(&value));
                require(value.vt==VT_I4,"Mode compartment not initialized");
                const LONG result=value.lVal; VariantClear(&value); return result;
            };
            auto writeMode=[&](ITfCompartment* item,LONG value) {
                VARIANT raw; VariantInit(&raw); raw.vt=VT_I4; raw.lVal=value;
                check(item->SetValue(client,&raw)); pump();
            };
            require(readMode(openMode.Get())==1,"Initial native mode is not Chinese");
            writeMode(openMode.Get(),0);
            require((readMode(conversionMode.Get())&TF_CONVERSIONMODE_NATIVE)==0,"External close did not clear native mode");
            writeMode(conversionMode.Get(),readMode(conversionMode.Get())|TF_CONVERSIONMODE_NATIVE);
            require(readMode(openMode.Get())==1,"External conversion did not open Chinese mode");
            check(profiles->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,clsid,profile,nullptr,TF_IPPMF_FORPROCESS)); pump();
            // Deactivating a TIP can activate the user's fallback TIP. Its
            // compartment sinks must not compete with the manually driven
            // instance below. Select an already-loaded plain keyboard layout
            // for this test process only (no user/session profile changes).
            HKL layouts[64]{};const int layoutCount=GetKeyboardLayoutList(64,layouts);HKL neutral=nullptr;
            for(int i=0;i<layoutCount;++i)if(LOWORD(reinterpret_cast<ULONG_PTR>(layouts[i]))==0x0409){neutral=layouts[i];break;}
            require(neutral!=nullptr,"Hidden activation fixture requires a loaded English keyboard layout");
            check(profiles->ActivateProfile(TF_PROFILETYPE_KEYBOARDLAYOUT,0x0409,CLSID_NULL,GUID_NULL,neutral,
                TF_IPPMF_FORPROCESS|TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE));pump();
            TF_INPUTPROCESSORPROFILE neutralProfile{};check(profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&neutralProfile));
            require(neutralProfile.dwProfileType==TF_PROFILETYPE_KEYBOARDLAYOUT && neutralProfile.hkl==neutral,
                "Plain keyboard did not replace the fallback TIP in the test process");
            // The hidden host has no OS foreground focus. Exercise the actual
            // service with explicit TSF focus/key callbacks, separately from
            // the system profile activation verified above.
            ComPtr<ITfLangBarItemMgr> barManager; check(manager.As(&barManager));
            ComPtr<ITfLangBarItem> systemBar;
            barManager->GetItem(tiger::tsf::LanguageBar::ItemId,&systemBar);
            if(systemBar) {
                TF_LANGBARITEMINFO other{};check(systemBar->GetInfo(&other));
                require(other.clsidService!=clsid,"System deactivation retained native language-bar item");
                // Manual activation below bypasses the profile manager's handoff.
                // Remove only this test thread's fallback service mode item.
                check(barManager->RemoveItem(systemBar.Get()));
                systemBar.Reset();
            }
            check(factory->CreateInstance(nullptr,IID_PPV_ARGS(&service)));
            ComPtr<ITfClientId> clientIds; check(manager.As(&clientIds));
            TfClientId serviceClient; check(clientIds->GetClientId(clsid,&serviceClient));
            check(service->ActivateEx(manager.Get(),serviceClient,0));
            ComPtr<ITfThreadMgrEventSink> focusSink; check(service.As(&focusSink));
            ComPtr<ITfKeyEventSink> keySink; check(service.As(&keySink));
            if(sentenceTest) {
                sentence_tsf_fixture::run(service.Get(),manager.Get(),first,second,ui.Get(),sentenceMeasure);
                check(service->Deactivate());service.Reset();
                check(first.manager->Pop(TF_POPF_ALL));check(second.manager->Pop(TF_POPF_ALL));
                check(source->UnadviseSink(uiCookie));check(manager->Deactivate());DestroyWindow(secondWindow);DestroyWindow(window);
                if(sentenceMeasure)std::cout<<"{\"status\":\"sentence-tsf-measured\",\"physical_input\":false}\n";
                else std::cout<<"{\"status\":\"sentence-tsf-passed\",\"physical_input\":false,\"computed_result_deferred_positive\":true,\"computed_result_stale_context\":true,\"computed_result_stale_thread_focus\":true}\n";return 0;
            }
            if(selectionRace) {
                selection_race_fixture::run(service.Get(),serviceClient,first,second,ui.Get());
                check(service->Deactivate());service.Reset();
                check(first.manager->Pop(TF_POPF_ALL));check(second.manager->Pop(TF_POPF_ALL));
                check(source->UnadviseSink(uiCookie));check(manager->Deactivate());DestroyWindow(secondWindow);DestroyWindow(window);
                std::cout<<"{\"status\":\"passed\",\"private_activation\":true,\"module_verified\":true,\"dictionary_verified\":true,\"stale_selection\":true}\n";
                return 0;
            }
            if(wordSaveFailure) {
                word_save_fixture::run(service.Get(),manager.Get(),serviceClient,first,ui.Get(),std::filesystem::path(argv[5]));
                check(service->Deactivate());service.Reset();
                check(first.manager->Pop(TF_POPF_ALL));check(second.manager->Pop(TF_POPF_ALL));
                check(source->UnadviseSink(uiCookie));check(manager->Deactivate());DestroyWindow(secondWindow);DestroyWindow(window);
                std::cout<<"{\"status\":\"passed\",\"preview_no_warning\":true,\"failure_warning\":true,\"persisted_order_restored\":true,\"ordinary_commit_keeps_warning\":true,\"explicit_retry_clears_warning\":true,\"secure_reactivation_default_style\":true,\"secure_reactivation_no_user_overlay\":true,\"secure_warning_suppressed\":true,\"physical_input\":false}\n";
                return 0;
            }
            check(focusSink->OnSetFocus(first.manager.Get(),nullptr));
            unsigned liveKeyCallbacks=0;
            if(schemaActivation) {
                require(timerWindows(L"NativeTiger.DataRefresh.")==1,"Live refresh scheduler missing or duplicated");
                wchar_t rootText[32768]{};
                require(GetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",rootText,32768)>0,"Live refresh requires isolated root");
                const std::filesystem::path root(rootText);
                require(std::filesystem::exists(root/L".schema-tsf-test"),"Live refresh marker missing");
                std::ifstream original(root/L"config.txt",std::ios::binary);
                const std::string initialConfig((std::istreambuf_iterator<char>(original)),{});original.close();
                auto publish=[&](const wchar_t* file,const std::string& bytes) {
                    const auto target=root/file,temp=root/L"live-refresh.tmp";
                    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));require(out.good(),"Cannot write refresh fixture");}
                    require(MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE,"Cannot publish refresh fixture");
                };
                auto waitFor=[&](auto predicate,const char* error) {
                    const auto deadline=GetTickCount64()+3000;
                    while(!predicate() && GetTickCount64()<deadline) {pump();Sleep(10);}
                    require(predicate(),error);
                };
                auto settle=[](ULONGLONG duration=750) {const auto until=GetTickCount64()+duration;while(GetTickCount64()<until){pump();Sleep(10);}};
                auto live=document(manager.Get(),client,window);
                check(focusSink->OnSetFocus(live.manager.Get(),first.manager.Get()));
                BYTE saved[256],empty[256]{};require(GetKeyboardState(saved)!=FALSE && SetKeyboardState(empty)!=FALSE,"Cannot set live refresh keyboard state");
                auto tapLive=[&](WPARAM vk) {
                    BOOL eaten=FALSE;++liveKeyCallbacks;check(keySink->OnTestKeyDown(live.context.Get(),vk,1,&eaten));
                    if(eaten){++liveKeyCallbacks;check(keySink->OnKeyDown(live.context.Get(),vk,1,&eaten));}
                    BOOL up=FALSE;++liveKeyCallbacks;check(keySink->OnTestKeyUp(live.context.Get(),vk,1,&up));
                    if(up){++liveKeyCallbacks;check(keySink->OnKeyUp(live.context.Get(),vk,1,&up));}
                    return eaten!=FALSE;
                };
                ComPtr<ITfUIElementMgr> liveUI;check(manager.As(&liveUI));
                auto candidateList=[&] {
                    ComPtr<ITfUIElement> element;check(liveUI->GetUIElement(ui->id,&element));
                    ComPtr<ITfCandidateListUIElementBehavior> list;check(element.As(&list));return list;
                };
                auto pageCount=[&] {UINT pages=0;check(candidateList()->GetPageIndex(nullptr,0,&pages));return pages;};
                settle(); // Establish the watch before the measured changes.
                require(tapLive('A') && tapLive('B') && live.store->text==L"ab","Live refresh composition missing");
                publish(L"config.txt",initialConfig+"\n每页候选个数 1\n");
                waitFor([&]{return pageCount()==4;},"Settings did not refresh without focus change");
                require(live.store->text==L"ab","Settings refresh changed raw code");
                require(tapLive(VK_OEM_PLUS),"Live paging key was not handled");
                UINT page=0;check(candidateList()->GetCurrentPage(&page));require(page==1,"Live paging fixture failed");
                BOOL held=FALSE;++liveKeyCallbacks;check(keySink->OnKeyDown(live.context.Get(),VK_LSHIFT,1,&held));
                check(focusSink->OnSetFocus(live.manager.Get(),live.manager.Get()));
                check(candidateList()->GetCurrentPage(&page));
                require(page==1,"Duplicate focus notification reset candidate page");
                const auto updates=ui->updates,begins=ui->begins;
                publish(L"unrelated.txt","duplicate invalidation");settle(5500);
                require(ui->updates==updates && ui->begins==begins,"Unchanged reconciliation recreated candidate UI");
                check(candidateList()->GetCurrentPage(&page));require(page==1,"Duplicate invalidation reset candidate page");
                ++liveKeyCallbacks;check(keySink->OnKeyUp(live.context.Get(),VK_LSHIFT,1,&held));
                require(readMode(openMode.Get())==0 && live.store->text==L"ab" && compositionCount(live)==0,"Live refresh lost held Shift state");
                writeMode(openMode.Get(),1);
                require(tapLive('A') && tapLive('B'),"Deferred refresh composition missing");
                live.store->deferLocks=true;
                publish(L"config.txt",initialConfig+"\n每页候选个数 2\n");
                waitFor([&]{return live.store->pendingLock!=0;},"Live refresh did not request deferred edit");
                publish(L"config.txt",initialConfig+"\n每页候选个数 3\n");settle();
                live.store->deferLocks=false;check(live.store->grantPendingLock());pump();
                UINT indices[4]{},pages=0;check(candidateList()->GetPageIndex(indices,4,&pages));
                require(pages==2 && indices[1]==3 && live.store->text==L"abab","Deferred refresh did not use newest settings");
                const auto beforeRejected=live.store->rejectedLocks;
                live.store->rejectLocks=true;
                publish(L"config.txt",initialConfig+"\n每页候选个数 1\n");
                waitFor([&]{return live.store->rejectedLocks>beforeRejected;},"Live refresh denial was not exercised");
                require(pageCount()==2 && live.store->text==L"abab","Denied refresh changed candidates or preedit");
                live.store->rejectLocks=false;
                waitFor([&]{return pageCount()==4;},"Denied refresh did not retry after the lock became available");
                publish(L"config.txt","当前码表 MissingSchema\n每页候选个数 5\n");settle();
                require(pageCount()==4 && live.store->text==L"abab","Invalid schema replaced the last good live configuration");
                publish(L"config.txt",initialConfig+"\n每页候选个数 3\n");
                waitFor([&]{return pageCount()==2;},"Valid configuration did not recover after invalid schema");
                live.store->deferLocks=true;
                publish(L"config.txt",initialConfig+"\n每页候选个数 1\n");
                waitFor([&]{return live.store->pendingLock!=0;},"Stale-focus refresh was not deferred");
                check(focusSink->OnSetFocus(first.manager.Get(),live.manager.Get()));
                const auto focusBegins=ui->begins;
                live.store->deferLocks=false;check(live.store->grantPendingLock());pump();
                require(live.store->text==L"abab" && first.store->text.empty() && ui->begins==focusBegins && ui->id==TF_INVALID_UIELEMENTID,
                    "Old refresh edited text or restored candidates after focus moved");
                check(focusSink->OnSetFocus(live.manager.Get(),first.manager.Get()));
                require(pageCount()==4,"Returning focus did not recover the latest settings");
                ComPtr<ITfThreadFocusSink> liveThreadFocus;check(service.As(&liveThreadFocus));
                for(int notification=0;notification<2;++notification) {
                    live.store->deferLocks=true;
                    publish(L"config.txt",initialConfig+"\n每页候选个数 3\n");
                    waitFor([&]{return live.store->pendingLock!=0;},"Background refresh fixture did not defer");
                    if(notification==0)check(liveThreadFocus->OnKillThreadFocus());
                    else check(keySink->OnSetFocus(FALSE));
                    const auto backgroundBegins=ui->begins;
                    live.store->deferLocks=false;check(live.store->grantPendingLock());pump();
                    publish(L"config.txt",initialConfig+"\n每页候选个数 1\n");settle();
                    require(live.store->text==L"abab" && ui->begins==backgroundBegins && ui->id==TF_INVALID_UIELEMENTID,
                        "Background refresh changed preedit or reopened candidates");
                    if(notification==0) {
                        check(liveThreadFocus->OnSetThreadFocus());
                        check(focusSink->OnSetFocus(live.manager.Get(),nullptr));
                    } else {
                        // Returning foreground alone must release pending
                        // refresh work, without another document-focus event.
                        check(keySink->OnSetFocus(TRUE));
                        waitFor([&]{return ui->id!=TF_INVALID_UIELEMENTID && pageCount()==4;},"Foreground-only notification did not resume refresh");
                    }
                    require(pageCount()==4 && live.store->text==L"abab","Foreground recovery lost current settings or raw code");
                }
                publish(L"config.txt",initialConfig);publish(L"自定义选重键.txt","2选 VK_Q\n");
                waitFor([&]{return pageCount()==1;},"Default page size did not return");settle();
                publish(L"自定义选重键.txt","invalid selection fixture\n");
                publish(L"config.txt",initialConfig+"\n每页候选个数 2\n");
                waitFor([&]{return pageCount()==2;},"Independent valid settings stopped at malformed selection file");
                settle();
                BSTR label=nullptr;check(candidateList()->GetString(1,&label));
                const auto expected=L"ab"+std::wstring(label,SysStringLen(label));SysFreeString(label);
                require(tapLive('Q') && live.store->text==expected,"Live selection binding did not reach dispatch");
                publish(L"config.txt",initialConfig);publish(L"自定义选重键.txt","2选 VK_Q\n");settle();
                require(tapLive('A') && tapLive('B'),"Live journal fixture did not compose");
                const auto preedit=live.store->text;
                wchar_t self[32768]{};require(GetModuleFileNameW(nullptr,self,32768)>0,"Cannot locate journal writer");
                const auto writer=std::filesystem::path(self).parent_path()/L"user_store_probe.exe";
                auto dictionary=root/L"schemas"/L"SchemaTest"/L"tiger-v2.tcd";
                const auto journal=root/L"schemas"/L"SchemaTest"/L"user.tcu";
                std::wstring command=L"\""+writer.wstring()+L"\" \""+dictionary.wstring()+L"\" \""+journal.wstring()+L"\" write ab live 1";
                STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
                require(CreateProcessW(writer.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)!=FALSE,"Cannot start independent journal writer");
                CloseHandle(child.hThread);const auto waited=WaitForSingleObject(child.hProcess,10000);
                if(waited!=WAIT_OBJECT_0) {TerminateProcess(child.hProcess,1);WaitForSingleObject(child.hProcess,1000);}
                DWORD exitCode=1;GetExitCodeProcess(child.hProcess,&exitCode);CloseHandle(child.hProcess);
                require(waited==WAIT_OBJECT_0 && exitCode==0,"Independent journal writer failed");
                auto candidateCount=[&] {UINT count=0;check(candidateList()->GetCount(&count));return count;};
                waitFor([&]{return candidateCount()==5;},"Other-process journal did not refresh live candidates");
                require(live.store->text==preedit,"Journal refresh changed active preedit");
                publish(L"config.txt","当前码表 虎码字词\n");
                waitFor([&]{return candidateCount()==4;},"Live schema switch retained old journal");
                require(live.store->text==preedit,"Schema switch changed active preedit");
                publish(L"config.txt",initialConfig);
                waitFor([&]{return candidateCount()==5;},"Live schema restore lost journal");
                if(std::filesystem::exists(root/L".generation-tsf-test")) {
                    const auto generation=root/L"schemas"/L"SchemaTest"/L"generations"/L"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
                    std::filesystem::create_directories(generation);
                    std::filesystem::copy_file(dictionary,generation/L"tiger-v2.tcd");
                    publish(L"schemas\\SchemaTest\\current.txt","generation\tbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n");
                    waitFor([&] {try {verifyMappedDictionary(generation/L"tiger-v2.tcd");return true;}catch(const std::exception&){return false;}},"Same-name generation did not remap without focus change");
                    require(live.store->text==preedit && candidateCount()==5,"Generation refresh lost preedit or journal");
                }
                live.store->deferLocks=true;
                publish(L"config.txt",initialConfig+"\n每页候选个数 2\n");
                waitFor([&]{return live.store->pendingLock!=0;},"Reactivation refresh was not deferred");
                check(service->Deactivate());
                require(timerWindows(L"NativeTiger.DataRefresh.")==0,"Old activation retained refresh scheduler");
                publish(L"config.txt",initialConfig);
                check(service->ActivateEx(manager.Get(),serviceClient,0));
                check(focusSink->OnSetFocus(live.manager.Get(),first.manager.Get()));
                const auto reactivationBegins=ui->begins;
                live.store->deferLocks=false;check(live.store->grantPendingLock());pump();
                require(live.store->text==preedit && compositionCount(live)==0 &&
                    ui->id==TF_INVALID_UIELEMENTID && ui->begins==reactivationBegins,
                    "Old activation's refresh replayed text or candidates into reactivated context");
                require(timerWindows(L"NativeTiger.DataRefresh.")==1,"Reactivation did not own exactly one refresh scheduler");
                // Windows prevents renaming a directory with mapped dictionary
                // descendants. Use the bundled table outside this user root,
                // after releasing the previous activation's schema mappings.
                check(service->Deactivate());
                const std::string rootConfig="当前码表 虎码字词\n每页候选个数 1\n";
                publish(L"config.txt",rootConfig);
                check(service->ActivateEx(manager.Get(),serviceClient,0));
                check(focusSink->OnSetFocus(live.manager.Get(),nullptr));
                require(tapLive('A') && tapLive('B') && candidateCount()==4 && pageCount()==4,"Root recovery fixture did not use bundled dictionary");
                const auto rootPreedit=live.store->text;
                {
                    const auto displaced=std::filesystem::path(root.wstring()+L".displaced");
                    require(!std::filesystem::exists(displaced),"Displaced root fixture already exists");
                    struct RestoreRoot {
                        std::filesystem::path root,displaced;
                        ~RestoreRoot() {
                            std::error_code error;
                            if(std::filesystem::exists(displaced,error) && !std::filesystem::exists(root,error))
                                std::filesystem::rename(displaced,root,error);
                        }
                    } restore{root,displaced};
                    std::filesystem::rename(root,displaced);
                    settle(5500);
                    require(!std::filesystem::exists(root),"Missing root was silently recreated with default settings");
                    require(live.store->text==rootPreedit && candidateCount()==4 && pageCount()==4,"Missing root lost active settings or raw code");
                    check(focusSink->OnSetFocus(live.manager.Get(),nullptr));
                    require(live.store->text==rootPreedit && pageCount()==4,"Focus refresh discarded cached input while root was absent");
                    std::filesystem::rename(displaced,root);
                    settle(5500);
                    publish(L"config.txt",rootConfig+"每页候选个数 2\n");
                    waitFor([&]{return pageCount()==2;},"Restored root did not resume change notifications");
                    require(live.store->text==rootPreedit && candidateCount()==4,"Restored root lost active input state");
                }
                require(tapLive(VK_ESCAPE),"Cannot clear root recovery composition");
                const auto savedText=live.store->text;
                require(tapLive('A') && tapLive('B'),"Settings-save composition missing");
                const auto saveEnglish=initialConfig+"\n默认中文 否\n_native_settings_reload save-1\n";
                publish(L"config.txt",saveEnglish);
                waitFor([&]{return compositionCount(live)==0 && readMode(openMode.Get())==0;},"Settings save did not cancel composition and apply English default");
                require(live.store->text==savedText,"Settings save committed raw code instead of cancelling it");
                writeMode(openMode.Get(),1);
                require(tapLive('A') && tapLive('B'),"Repeated-save fixture missing");
                publish(L"config.txt",saveEnglish);settle(5500);
                require(live.store->text==savedText+L"ab" && compositionCount(live)==1 && readMode(openMode.Get())==1,
                    "Repeated save notification cleared new input or reset mode");
                live.store->deferLocks=true;
                publish(L"config.txt",initialConfig+"\n默认中文 否\n_native_settings_reload save-2\n");
                waitFor([&]{return live.store->pendingLock!=0;},"Settings save did not defer composition edit");
                publish(L"config.txt",initialConfig+"\n默认中文 是\n_native_settings_reload save-3\n");settle();
                require(live.store->text==savedText+L"ab","Deferred settings save modified text before lock grant");
                live.store->deferLocks=false;check(live.store->grantPendingLock());pump();
                require(live.store->text==savedText && compositionCount(live)==0 && readMode(openMode.Get())==1,
                    "Deferred settings save did not apply newest request without committing code");
                require(tapLive('A') && tapLive('B'),"Denied settings-save fixture missing");
                live.store->rejectLocks=true;
                const auto saveRejects=live.store->rejectedLocks;
                publish(L"config.txt",initialConfig+"\n默认中文 否\n_native_settings_reload save-4\n");
                waitFor([&]{return live.store->rejectedLocks>saveRejects;},"Settings save did not exercise denied lock");
                require(live.store->text==savedText+L"ab" && compositionCount(live)==1,"Denied save consumed composition");
                live.store->rejectLocks=false;
                waitFor([&]{return compositionCount(live)==0 && readMode(openMode.Get())==0;},"Settings save was not retried after lock recovery");
                require(live.store->text==savedText,"Recovered save inserted cancelled code");
                writeMode(openMode.Get(),1);
                require(tapLive('A') && tapLive('B'),"Background settings-save fixture missing");
                live.store->deferLocks=true;
                publish(L"config.txt",initialConfig+"\n默认中文 否\n_native_settings_reload save-5\n");
                waitFor([&]{return live.store->pendingLock!=0;},"Background save did not queue edit");
                check(focusSink->OnSetFocus(first.manager.Get(),live.manager.Get()));
                live.store->deferLocks=false;check(live.store->grantPendingLock());pump();
                require(live.store->text==savedText+L"ab" && compositionCount(live)==1,
                    "Stale settings-save edit modified background document");
                check(focusSink->OnSetFocus(live.manager.Get(),first.manager.Get()));
                waitFor([&]{return compositionCount(live)==0 && readMode(openMode.Get())==0;},"Returning context lost its pending settings reload");
                require(live.store->text==savedText,"Returning context committed cancelled input");
                writeMode(openMode.Get(),1);
                publish(L"config.txt",initialConfig);
                require(SetKeyboardState(saved)!=FALSE,"Cannot restore live refresh keyboard state");
                std::filesystem::remove(root/L"自定义选重键.txt");
                check(focusSink->OnSetFocus(first.manager.Get(),live.manager.Get()));
                check(live.manager->Pop(TF_POPF_ALL));settle();
            }
            writeMode(openMode.Get(),0);
            check(focusSink->OnSetFocus(second.manager.Get(),first.manager.Get()));
            require(readMode(openMode.Get())==0,"New context did not inherit English mode");
            writeMode(openMode.Get(),1);
            check(focusSink->OnSetFocus(first.manager.Get(),second.manager.Get()));
            require(readMode(openMode.Get())==0,"First context did not retain English mode");
            check(focusSink->OnSetFocus(second.manager.Get(),first.manager.Get()));
            require(readMode(openMode.Get())==1,"Second context did not retain Chinese mode");
            BYTE savedModeKeys[256],emptyModeKeys[256]{};
            require(GetKeyboardState(savedModeKeys)!=FALSE,"Cannot save keyboard state");
            require(SetKeyboardState(emptyModeKeys)!=FALSE,"Cannot clear keyboard state");
            // A handled Ctrl+Space down must not leak its repeat/up to the host.
            // Preview must leave both mode and the consumed-key latch unchanged.
            for(bool controlFirst:{false,true}) {
                BYTE chordKeys[256]{};chordKeys[VK_CONTROL]=chordKeys[VK_LCONTROL]=0x80;
                require(SetKeyboardState(chordKeys)!=FALSE,"Cannot set Ctrl+Space modifiers");
                auto chordEvent=[&](UINT vk,bool down,bool repeat,bool mustEat) {
                    const LPARAM flags=1 | (repeat?(1LL<<30):0) | (down?0:((1LL<<30)|(1LL<<31)));
                    BOOL eaten=FALSE;const auto before=readMode(openMode.Get());
                    for(int preview=0;preview<2;++preview) {
                        if(down)check(keySink->OnTestKeyDown(second.context.Get(),vk,flags,&eaten));
                        else check(keySink->OnTestKeyUp(second.context.Get(),vk,flags,&eaten));
                        require(!mustEat || eaten,"Ctrl+Space repeat/release leaked from TSF preview");
                        require(readMode(openMode.Get())==before,"Ctrl+Space preview changed mode");
                    }
                    if(eaten) {
                        if(down)check(keySink->OnKeyDown(second.context.Get(),vk,flags,&eaten));
                        else check(keySink->OnKeyUp(second.context.Get(),vk,flags,&eaten));
                        require(!mustEat || eaten,"Ctrl+Space dispatch leaked to host");
                    }
                    pump();
                };
                const auto initial=readMode(openMode.Get());
                chordEvent(VK_LCONTROL,true,false,false);
                chordEvent(VK_SPACE,true,false,true);
                require(readMode(openMode.Get())!=initial,"Ctrl+Space did not toggle on down");
                chordEvent(VK_SPACE,true,true,true);
                require(readMode(openMode.Get())!=initial,"Ctrl+Space repeat toggled mode again");
                if(controlFirst) {SetKeyboardState(emptyModeKeys);chordEvent(VK_LCONTROL,false,false,false);}
                chordEvent(VK_SPACE,false,false,true);
                require(readMode(openMode.Get())!=initial,"Ctrl+Space release toggled mode again");
                if(!controlFirst) {SetKeyboardState(emptyModeKeys);chordEvent(VK_LCONTROL,false,false,false);}
            }
            require(readMode(openMode.Get())==1 && second.store->text.empty(),"Ctrl+Space regression altered final mode/text");
            BOOL modeEaten=FALSE;
            check(keySink->OnKeyDown(second.context.Get(),'A',1,&modeEaten));
            require(modeEaten && second.store->text==L"a","Mode fixture did not start composition");
            second.store->rejectLocks=true;
            writeMode(openMode.Get(),0);
            if(!(second.store->rejectedLocks>0 && readMode(openMode.Get())==1 && second.store->text==L"a"))
                throw std::runtime_error("Rejected mode edit did not restore Chinese mode and composition: rejected="+
                    std::to_string(second.store->rejectedLocks)+" open="+std::to_string(readMode(openMode.Get()))+
                    " text_length="+std::to_string(second.store->text.size()));
            second.store->rejectLocks=false;
            second.store->deferLocks=true;
            writeMode(openMode.Get(),0);
            require(second.store->pendingLock!=0,"Mode edit was not deferred by text store");
            writeMode(openMode.Get(),1);
            second.store->deferLocks=false;
            check(second.store->grantPendingLock()); pump();
            require(readMode(openMode.Get())==1 && second.store->text==L"a",
                "Stale deferred request replaced newer Chinese mode");
            second.store->deferLocks=true;
            writeMode(openMode.Get(),0);
            require(second.store->pendingLock!=0,"Thread-focus mode edit was not deferred");
            ComPtr<ITfThreadFocusSink> threadFocus; check(service.As(&threadFocus));
            check(threadFocus->OnKillThreadFocus());
            second.store->deferLocks=false;
            check(second.store->grantPendingLock()); pump();
            require(readMode(openMode.Get())==1 && second.store->text==L"a",
                "Losing thread focus did not invalidate pending mode edit");
            check(keySink->OnSetFocus(TRUE));
            check(focusSink->OnSetFocus(second.manager.Get(),nullptr));
            second.store->deferLocks=true;
            writeMode(openMode.Get(),0);
            require(second.store->pendingLock!=0,"Focus-race mode edit was not deferred");
            check(focusSink->OnSetFocus(first.manager.Get(),second.manager.Get()));
            require(readMode(openMode.Get())==0,"Focus-race fixture lost first context's English mode");
            second.store->deferLocks=false;
            check(second.store->grantPendingLock()); pump();
            require(readMode(openMode.Get())==0 && first.store->text.empty() && second.store->text==L"a",
                "Old context mode edit changed the focused context");
            check(focusSink->OnSetFocus(second.manager.Get(),first.manager.Get()));
            require(readMode(openMode.Get())==1,"Discarded edit changed the unfocused engine mode");
            second.store->deferLocks=true;
            writeMode(openMode.Get(),0);
            require(second.store->pendingLock && readMode(openMode.Get())==1,
                "Pending edit advertised uncommitted English mode");
            check(focusSink->OnSetFocus(second.manager.Get(),second.manager.Get()));
            second.store->deferLocks=false;
            check(second.store->grantPendingLock()); pump();
            require(readMode(openMode.Get())==0,"Granted mode edit did not publish English");
            require(second.store->text==L"a","English switch did not retain raw code");
            ComPtr<ITfContextComposition> compositions; check(second.context.As(&compositions));
            ComPtr<IEnumITfCompositionView> views; check(compositions->EnumCompositions(&views));
            ComPtr<ITfCompositionView> remaining; ULONG fetched=0;
            check(views->Next(1,&remaining,&fetched));
            require(fetched==0,"English switch left a TSF composition active");
            modeEaten=TRUE;
            check(keySink->OnKeyDown(second.context.Get(),'B',1,&modeEaten));
            require(!modeEaten && second.store->text==L"a","English switch did not update engine");
            check(keySink->OnKeyDown(second.context.Get(),VK_LSHIFT,1,&modeEaten));
            check(keySink->OnKeyUp(second.context.Get(),VK_LSHIFT,1,&modeEaten));
            // Compartment publication may use the existing posted retry when
            // TSF refuses a write inside its notification dispatch. Validate
            // the completed synchronization rather than requiring inline delivery.
            const auto modeDeadline=GetTickCount64()+1000;
            while((readMode(openMode.Get())!=1 || !(readMode(conversionMode.Get())&TF_CONVERSIONMODE_NATIVE)) &&
                GetTickCount64()<modeDeadline) {pump();Sleep(10);}
            if(readMode(openMode.Get())!=1 || !(readMode(conversionMode.Get())&TF_CONVERSIONMODE_NATIVE))
                throw std::runtime_error("Keyboard toggle did not publish Chinese mode: open="+std::to_string(readMode(openMode.Get()))+
                    " conversion="+std::to_string(readMode(conversionMode.Get()))+" eaten="+std::to_string(modeEaten));
            ComPtr<ITfLangBarItem> barItem; check(barManager->GetItem(tiger::tsf::LanguageBar::ItemId,&barItem));
            TF_LANGBARITEMINFO modeInfo{};check(barItem->GetInfo(&modeInfo));
            require(modeInfo.guidItem==GUID_LBI_INPUTMODE && modeInfo.clsidService==clsid,"Taskbar input-mode item identity incorrect");
            ComPtr<ITfLangBarItemButton> barButton; check(barItem.As(&barButton));
            TF_LANGBARITEMINFO barInfo{}; check(barItem->GetInfo(&barInfo));
            require(barInfo.clsidService==clsid,"Language-bar item belongs to another service");
            auto barText=[&](const wchar_t* expected) {
                BSTR text=nullptr; check(barButton->GetText(&text));
                const bool equal=text && std::wstring(text,SysStringLen(text))==expected;
                SysFreeString(text); require(equal,"Language-bar text does not match engine mode");
                HICON icon=nullptr; check(barButton->GetIcon(&icon));
                require(icon!=nullptr,"Language-bar icon missing"); DestroyIcon(icon);
            };
            ComPtr<MenuCapture> menu;menu.Attach(new MenuCapture);
            check(barButton->InitMenu(menu.Get()));
            require(menu->items.size()==12 && menu->children.count(10) && menu->children.count(11),"Language-bar management menu differs");
            require(menu->children[11]->items.size()==9,"Theme submenu is incomplete");
            require(barButton->OnMenuSelect(999)==E_INVALIDARG,"Unknown management action accepted");
            barText(L"中");
            check(barButton->OnClick(TF_LBI_CLK_LEFT,POINT{},nullptr)); pump();
            require(readMode(openMode.Get())==0,"Language-bar click did not switch to English"); barText(L"英");
            check(barButton->OnClick(TF_LBI_CLK_RIGHT,POINT{},nullptr));
            require(readMode(openMode.Get())==0,"Right click unexpectedly toggled input mode");
            check(barButton->OnClick(TF_LBI_CLK_LEFT,POINT{},nullptr)); pump(); barText(L"中");
            check(focusSink->OnSetFocus(nullptr,second.manager.Get()));
            DWORD barStatus=0; check(barItem->GetStatus(&barStatus));
            require((barStatus&TF_LBI_STATUS_DISABLED)!=0,"Unfocused language bar is enabled");
            check(barButton->OnClick(TF_LBI_CLK_LEFT,POINT{},nullptr));
            require(readMode(openMode.Get())==1,"Disabled language bar changed input mode");
            check(focusSink->OnSetFocus(second.manager.Get(),nullptr));
            check(keySink->OnKeyDown(second.context.Get(),'C',1,&modeEaten));
            require(modeEaten && second.store->text==L"ac","Deactivation fixture did not start composition");
            second.store->deferLocks=true;
            writeMode(openMode.Get(),0);
            require(second.store->pendingLock!=0,"Deactivation mode edit was not deferred");
            require(SetKeyboardState(savedModeKeys)!=FALSE,"Cannot restore keyboard state");
            check(service->Deactivate()); keySink.Reset(); focusSink.Reset(); service.Reset();
            require(timerWindows(L"NativeTiger.DataRefresh.")==0,"Deactivation retained live refresh scheduler");
            second.store->deferLocks=false;
            check(second.store->grantPendingLock()); pump();
            require(readMode(openMode.Get())==1 && second.store->text==L"ac",
                "Deferred edit changed text or mode after deactivation");
            views.Reset(); remaining.Reset(); fetched=0;
            check(compositions->EnumCompositions(&views)); check(views->Next(1,&remaining,&fetched));
            require(fetched==0,"Deactivation left a composition after the pending lock was granted");
            require(barButton->OnMenuSelect(1)==S_FALSE,"Detached button launched management tool");
            require(barButton->OnClick(TF_LBI_CLK_LEFT,POINT{},nullptr)==S_FALSE,"Detached language-bar button remained active");
            check(first.manager->Pop(TF_POPF_ALL)); check(second.manager->Pop(TF_POPF_ALL));
            check(source->UnadviseSink(uiCookie)); check(manager->Deactivate()); DestroyWindow(secondWindow); DestroyWindow(window);
            std::cout<<"{\"events\":0,\"private_activation\":"<<(registeredActivation?"false":"true")<<",\"module_verified\":true,\"dictionary_verified\":true,\"mode_compartments\":true,\"explicit_focus_mode_restore\":true,\"explicit_key_callbacks\":"<<(5+liveKeyCallbacks)<<",\"live_settings_refresh\":"<<(schemaActivation?"true":"false")<<",\"settings_save_reload\":"<<(schemaActivation?"true":"false")<<",\"duplicate_focus_preserves_state\":"<<(schemaActivation?"true":"false")<<",\"live_refresh_error_recovery\":"<<(schemaActivation?"true":"false")<<",\"live_refresh_stale_focus\":"<<(schemaActivation?"true":"false")<<",\"live_root_restore\":"<<(schemaActivation?"true":"false")<<",\"live_refresh_background\":"<<(schemaActivation?"true":"false")<<",\"live_refresh_reactivation\":"<<(schemaActivation?"true":"false")<<",\"live_journal_refresh\":"<<(schemaActivation?"true":"false")<<",\"live_schema_refresh\":"<<(schemaActivation?"true":"false")<<",\"mode_composition_switch\":true,\"keyboard_mode_publish\":true,\"language_bar_callbacks\":true,\"management_menu_contract\":true,\"mode_lock_rejection\":true,\"mode_deferred_superseded\":true,\"mode_deferred_granted\":true,\"mode_deferred_focus\":true,\"mode_deferred_thread_focus\":true,\"mode_deferred_deactivation\":true,\"physical_focus_validated\":false,\"alternate_schema\":"<<(schemaActivation?"true":"false")<<",\"status\":\"activation-only-passed\"}\n";
            return 0;
        }
        // Windows may restore English mode for this profile. Establish the
        // Chinese-mode precondition through the standard TSF compartment.
        ComPtr<ITfCompartmentMgr> initialModes;check(manager.As(&initialModes));
        ComPtr<ITfCompartment> initialOpen;check(initialModes->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,&initialOpen));
        VARIANT initialChinese;VariantInit(&initialChinese);initialChinese.vt=VT_I4;initialChinese.lVal=1;
        check(initialOpen->SetValue(client,&initialChinese));pump();
        BYTE savedKeys[256]; GetKeyboardState(savedKeys);
        BYTE emptyKeys[256]{}; SetKeyboardState(emptyKeys);
        unsigned events=0; bool timerPreview=false; ULONGLONG reminderDelay=0;
        const bool traceKeys=GetEnvironmentVariableW(L"NATIVE_TIGER_TEST_TRACE",nullptr,0)!=0;
        auto traceState=[&](const char* phase,Document& doc,WPARAM vk,bool down) {
            if(!traceKeys)return;
            std::cerr<<phase<<" event="<<events<<" vk="<<vk<<" down="<<down<<" comp_events="<<doc.store->compositionStarts<<","<<doc.store->compositionEnds<<" text=";
            for(auto ch:doc.store->text)std::cerr<<static_cast<unsigned>(ch)<<",";
            std::cerr<<" selection="<<doc.store->selection.acpStart<<","<<doc.store->selection.acpEnd<<" tick="<<GetTickCount64();
            std::cerr<<" shift="<<GetKeyState(VK_SHIFT)<<" ctrl="<<GetKeyState(VK_CONTROL)<<" alt="<<GetKeyState(VK_MENU)<<"\n";
        };
        auto event=[&](Document& doc,WPARAM vk,bool down,bool duplicateTest=false) {
            recordTerminationStacks=true;
            observeKey(events,vk,down,false,false,doc);
            traceState("before",doc,vk,down);
            if(GetForegroundWindow()!=doc.store->window || GetFocus()!=doc.store->window)
                throw std::runtime_error("Host lost OS focus before event "+std::to_string(events)+" vk="+std::to_string(vk));
            ComPtr<ITfDocumentMgr> focusedDocument;
            check(manager->GetFocus(&focusedDocument));
            require(focusedDocument.Get()==doc.manager.Get(),"Host TSF focus differs from target document");
            LPARAM flags=1|(static_cast<LPARAM>(MapVirtualKeyW(static_cast<UINT>(vk),MAPVK_VK_TO_VSC))<<16);
            if(!down) flags|=static_cast<LPARAM>(0xc0000000u);
            BOOL tested=FALSE,eaten=FALSE;
            auto dispatch=[&] {
            const auto before=doc.store->text;
            check(down?keys->TestKeyDown(vk,flags,&tested):keys->TestKeyUp(vk,flags,&tested));
            if(timerPreview) require(timerWindows()==0 && reminderChildren().empty(),"Timer preview started a reminder");
            if(tested) require(doc.store->text==before,"Preview changed document text");
            if(duplicateTest) {
                BOOL again=FALSE;
                check(down?keys->TestKeyDown(vk,flags,&again):keys->TestKeyUp(vk,flags,&again));
                require(again==tested,"Repeated preview changed decision");
            }
            if(tested) check(down?keys->KeyDown(vk,flags,&eaten):keys->KeyUp(vk,flags,&eaten));
            require(tested==eaten,"Test/dispatch handling differs");
            };
            if(GetEnvironmentVariableW(L"NATIVE_TIGER_TEST_QUEUED_KEYS",nullptr,0)!=0) {
                bool delivered=false;
                struct ResetQueue {~ResetQueue(){queuedKeyDispatch={};}} resetQueue;
                queuedKeyDispatch=[&](const MSG& message) {
                    if(message.hwnd!=doc.store->window || message.message!=static_cast<UINT>(down?WM_KEYDOWN:WM_KEYUP) ||
                        message.wParam!=vk || message.lParam!=flags)return false;
                    require(!delivered,"Duplicate queued test key");delivered=true;dispatch();return eaten!=FALSE;
                };
                require(PostMessageW(doc.store->window,down?WM_KEYDOWN:WM_KEYUP,vk,flags)!=FALSE,"Cannot post test key");
                const auto deadline=GetTickCount64()+2000;
                do {pump();if(delivered)break;MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);}while(GetTickCount64()<deadline);
                require(delivered,"Queued test key was not dispatched");
            } else dispatch(); ++events; pump(); observeKey(events,vk,down,true,eaten!=FALSE,doc); traceState("after",doc,vk,down); return eaten!=FALSE;
        };
        auto tap=[&](Document& doc,WPARAM vk) { const bool eaten=event(doc,vk,true,true); event(doc,vk,false); return eaten; };
        if(candidateMouse) {
            std::ofstream result(std::filesystem::path(argv[5])/L"result.json");
            try {
                ShowWindow(window,SW_SHOWNORMAL);SetForegroundWindow(window);SetFocus(window);
                check(manager->SetFocus(first.manager.Get()));pump();
                require(tap(first,'A') && tap(first,'B'),"Candidate input failed");
                auto wait=[&](unsigned ms){auto until=GetTickCount64()+ms;do{pump();MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);}while(GetTickCount64()<until);};
                wait(150);
                const auto candidate=ownCandidateWindow();require(candidate && IsWindowVisible(candidate),"Candidate missing");
                const bool masked=std::filesystem::exists(std::filesystem::path(argv[5])/L".tsf-candidate-mask-test");
                const auto original=first.store->text;require(original==(masked?L"甲😀":L"ab"),"Unexpected initial composition");
                if(masked) {
                    tap(first,VK_BACK);require(first.store->text==L"甲","Masked backspace damaged preedit");
                    tap(first,'B');require(first.store->text==original,"Masked preedit did not recover");
                    ComPtr<ITfUIElementMgr> elements;check(manager.As(&elements));ComPtr<ITfUIElement> element;
                    check(elements->GetUIElement(ui->id,&element));ComPtr<ITfCandidateListUIElement> list;check(element.As(&list));
                    BSTR label=nullptr;check(list->GetString(0,&label));
                    const bool unmasked=std::wstring(label,SysStringLen(label))==L"交";SysFreeString(label);
                    require(unmasked,"Code masking altered candidate data");
                }
                RECT before{},after{};GetWindowRect(candidate,&before);
                SendMessageW(candidate,WM_MOUSEWHEEL,MAKEWPARAM(0,2400),0);wait(250);
                GetWindowRect(candidate,&after);require(after.bottom-after.top>before.bottom-before.top,"Wheel did not enlarge candidate");
                require(first.store->text==original && compositionCount(first)==1,"Wheel changed composition");
                std::ifstream config(std::filesystem::path(argv[5])/L"config.txt",std::ios::binary);
                std::string text((std::istreambuf_iterator<char>(config)),{});require(text.find("27.00")!=std::string::npos,"Wheel size not persisted");config.close();
                SendMessageW(candidate,WM_MOUSEWHEEL,MAKEWPARAM(0,static_cast<WORD>(-2400)),0);wait(250);
                GetWindowRect(candidate,&after);require(after.bottom-after.top==before.bottom-before.top,"Wheel shrink did not restore size");
                static bool menuSeen=false;
                const auto timer=SetTimer(nullptr,0,100,[](HWND,UINT,UINT_PTR id,DWORD){
                    HWND popup=nullptr;
                    while((popup=FindWindowExW(nullptr,popup,L"#32768",nullptr))) {
                        DWORD pid=0;GetWindowThreadProcessId(popup,&pid);if(pid!=GetCurrentProcessId())continue;
                        const auto menu=reinterpret_cast<HMENU>(SendMessageW(popup,0x01e1,0,0));
                        if(menu && GetMenuItemCount(menu)==12)menuSeen=true;
                    }
                    EndMenu();KillTimer(nullptr,id);
                });
                require(timer!=0,"Cannot schedule menu cancel");
                SendMessageW(candidate,WM_RBUTTONDOWN,MK_RBUTTON,0);KillTimer(nullptr,timer);wait(100);
                require(menuSeen,"Candidate did not show shared menu");
                require(first.store->text==original && compositionCount(first)==1,"Menu cancel changed composition");
                require(GetForegroundWindow()==window && GetFocus()==window,"Menu stole input focus");
                require(IsWindowVisible(candidate),"Menu hid candidate");
                // Default vertical -> code-only -> horizontal -> vertical.
                RECT verticalRect{},codeRect{},horizontalRect{},restoredRect{};GetWindowRect(candidate,&verticalRect);
                SendMessageW(candidate,WM_MBUTTONUP,0,0);wait(150);GetWindowRect(candidate,&codeRect);
                require(codeRect.bottom-codeRect.top<verticalRect.bottom-verticalRect.top,"Middle click did not enter code-only mode");
                SendMessageW(candidate,WM_MBUTTONUP,0,0);wait(150);GetWindowRect(candidate,&horizontalRect);
                require(horizontalRect.right-horizontalRect.left>codeRect.right-codeRect.left,"Middle click did not enter horizontal mode");
                SendMessageW(candidate,WM_MBUTTONUP,0,0);wait(150);GetWindowRect(candidate,&restoredRect);
                require(restoredRect.bottom-restoredRect.top==verticalRect.bottom-verticalRect.top,"Middle click did not restore vertical mode");
                require(first.store->text==original && compositionCount(first)==1 && GetFocus()==window,"Middle cycle changed composition or focus");
                tap(first,VK_SPACE);require(first.store->text==L"交","Commit after mouse actions failed");
                result<<"{\"status\":\"passed\",\"mask_preedit_backspace_commit\":true,\"middle_cycle\":true,\"wheel_live_resize\":true,\"wheel_persisted\":true,\"menu_items\":12,\"composition_preserved\":true,\"focus_preserved\":true,\"commit_after_menu\":true}";
                SetKeyboardState(savedKeys);return 0;
            }catch(const std::exception& error){result<<"{\"status\":\"failed\"}";std::ofstream(std::filesystem::path(argv[5])/L"error.txt")<<error.what();throw;}
        }
        if(!tap(first,'A')) {
            ComPtr<ITfCompartmentMgr> modeManager;check(manager.As(&modeManager));
            ComPtr<ITfCompartment> mode;check(modeManager->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,&mode));
            VARIANT value;VariantInit(&value);check(mode->GetValue(&value));
            const auto open=value.vt==VT_I4?value.lVal:-1;VariantClear(&value);
            ComPtr<ITfLangBarItemMgr> bars;check(manager.As(&bars));ComPtr<ITfLangBarItem> item;
            check(bars->GetItem(tiger::tsf::LanguageBar::ItemId,&item));DWORD status=0;check(item->GetStatus(&status));
            throw std::runtime_error("a was not handled: open="+std::to_string(open)+" bar_status="+std::to_string(status)+
                " ctrl="+std::to_string(GetKeyState(VK_CONTROL))+" shift="+std::to_string(GetKeyState(VK_SHIFT)));
        }
        require(first.store->text==L"a","No first preedit");
        first.store->layoutReady=false;
        tap(first,'B'); require(first.store->text==L"ab","Preedit not replaced");
        ComPtr<ITfUIElementMgr> uiManager; check(manager.As(&uiManager));
        ComPtr<ITfUIElement> element; check(uiManager->GetUIElement(ui->id,&element));
        ComPtr<ITfCandidateListUIElementBehavior> candidates; check(element.As(&candidates));
        UINT count=0; check(candidates->GetCount(&count));
        if(count!=4) throw std::runtime_error("Wrong candidate count: "+std::to_string(count));
        BSTR label=nullptr; check(candidates->GetString(0,&label)); require(std::wstring(label,SysStringLen(label))==L"交","Wrong UI-less candidate"); SysFreeString(label);
        BOOL shown=TRUE; check(candidates->IsShown(&shown)); require((shown!=FALSE)==nativeUI,"Host UI choice ignored");
        if(nativeUI) {
            HWND candidateWindow=ownCandidateWindow();
            require(!candidateWindow || !IsWindowVisible(candidateWindow),"Stale candidate geometry visible without layout");
        }
        first.store->layoutReady=true;
        check(first.store->sink->OnLayoutChange(TS_LC_CHANGE,1)); pump();
        if(nativeUI) {
            // OnLayoutChange requests an ASYNCDONTCARE edit; its UI update
            // need not be granted during the first message-queue drain.
            HWND candidateWindow=nullptr;
            const auto deadline=GetTickCount64()+2000;
            do {
                pump();candidateWindow=ownCandidateWindow();
                if(candidateWindow && IsWindowVisible(candidateWindow))break;
                MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);
            } while(GetTickCount64()<deadline);
            DWORD process=0; GetWindowThreadProcessId(candidateWindow,&process);
            require(candidateWindow && process==GetCurrentProcessId() && IsWindowVisible(candidateWindow),"Native candidate window not visible");
            capture(candidateWindow,argv[2]);
        }
        UINT pages=0,index=99; check(candidates->GetPageIndex(&index,1,&pages)); require(pages==1 && index==0,"Wrong UI-less pages");
        tap(first,VK_SPACE); require(first.store->text==L"交","Space commit duplicated or lost text");
        require(ui->id==TF_INVALID_UIELEMENTID,"Candidate UI did not end");
        tap(first,'A'); tap(first,'B');
        require(compositionCount(first)==1,"First input has no TSF composition");
        ShowWindow(secondWindow,SW_SHOWNORMAL); SetForegroundWindow(secondWindow); SetFocus(secondWindow);
        check(manager->SetFocus(second.manager.Get())); pump();
        tap(second,'D'); tap(second,'K'); tap(second,VK_SPACE);
        require(second.store->text==L"口","Second context commit failed");
        require(first.store->text==L"交ab","Second context changed first composition");
        SetForegroundWindow(window); SetFocus(window);
        check(manager->SetFocus(first.manager.Get())); pump();
        // Windows can terminate a composition when a document loses focus.
        // Respect the authoritative TSF composition, rather than resurrecting
        // an engine buffer after its composition has already ended.
        const bool retained=compositionCount(first)!=0;
        std::wstring expected;
        if(retained) {
            require(tap(first,'2'),"Retained composition lost its engine state");
            expected=L"交疒";
        } else {
            require(!tap(first,'2'),"Terminated composition left a stale engine buffer");
            require(first.store->text==L"交ab","Termination changed committed document text");
            tap(first,'A'); tap(first,'B'); tap(first,'2');
            expected=L"交ab疒";
        }
        if(first.store->text!=expected) {
            auto units=[](const std::wstring& text) {std::string result;for(auto ch:text)result+=std::to_string(static_cast<unsigned>(ch))+",";return result;};
            throw std::runtime_error("Context state disagrees with TSF composition: retained="+std::to_string(retained)+
                " expected="+units(expected)+" actual="+units(first.store->text)+" compositions="+std::to_string(compositionCount(first)));
        }
        tap(first,'A'); tap(first,'B'); tap(first,VK_ESCAPE);
        require(first.store->text==expected,"Escape did not remove preedit");
        require(!tap(first,VK_RETURN),"Idle Enter should pass");
        require(first.store->text==expected,"Pass-through Enter inserted duplicate newline");
        tap(first,'A'); tap(first,'B');
        check(uiManager->GetUIElement(ui->id,element.ReleaseAndGetAddressOf())); check(element.As(&candidates));
        if(nativeUI) {
            HWND candidateWindow=ownCandidateWindow();
            // The default first candidate is not highlighted. Select item two
            // before locating its highlight to exercise mouse hit testing.
            check(candidates->SetSelection(1));
            const auto point=highlightedCandidate(candidateWindow);
            SendMessageW(candidateWindow,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
        } else { check(candidates->SetSelection(1)); check(candidates->Finalize()); }
        pump();
        require(first.store->text==expected+L"疒","Candidate finalization failed");
        if(schemaTest) {
            wchar_t rootText[32768];
            auto length=GetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",rootText,32768);
            require(length>0 && length<32768,"Schema test requires isolated user root");
            const std::filesystem::path root(rootText);
            require(std::filesystem::exists(root/L".schema-tsf-test"),"Schema fixture marker missing");
            auto configure=[&](const wchar_t* name) {
                const std::wstring text=std::wstring(L"\ufeff当前码表\t")+name+L"\r\n最大码长\t16\r\nCtrl+m切换最近码表\t是\r\n";
                std::ofstream out(root/L"config.txt",std::ios::binary|std::ios::trunc);
                out.write(reinterpret_cast<const char*>(text.data()),text.size()*sizeof(wchar_t));
                out.close(); require(!out.fail(),"Cannot write isolated schema setting");
            };
            auto focus=[&](Document& doc) {
                ShowWindow(doc.store->window,SW_SHOWNORMAL);
                SetForegroundWindow(doc.store->window); SetFocus(doc.store->window);
                check(manager->SetFocus(doc.manager.Get())); pump();
            };
            auto word=[&](Document& doc,const wchar_t* text) {
                const auto baseline=doc.store->text;
                for(int i=0;i<6;++i) tap(doc,'Q');
                tap(doc,VK_SPACE);
                require(doc.store->text==baseline+text,"Schema-specific user word committed incorrectly");
            };
            configure(L"SchemaTest");focus(second);
            verifyMappedDictionary(root/L"schemas"/L"SchemaTest"/L"tiger-v2.tcd");
            word(second,L"方案0");
            // The previously active context must also leave its old lexicon.
            focus(first);word(first,L"方案0");
            configure(L"MissingSchema");focus(second);word(second,L"方案0");
            configure(L"../SchemaTest");focus(first);word(first,L"方案0");
            configure(L"虎码字词");focus(second);word(second,L"内置0");
            focus(first);word(first,L"内置0");
            auto configBytes=[&]() {
                std::ifstream in(root/L"config.txt",std::ios::binary);
                return std::string(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
            };
            for(const auto* selectedWord:{L"方案0",L"内置0"}) {
                const auto baseline=first.store->text;
                for(int i=0;i<6;++i) tap(first,'Q');
                const auto beforeConfig=configBytes();
                BYTE ctrl[256]{};ctrl[VK_CONTROL]=ctrl[VK_LCONTROL]=0x80;SetKeyboardState(ctrl);
                BOOL preview=FALSE;
                const LPARAM flags=1|(static_cast<LPARAM>(MapVirtualKeyW('M',MAPVK_VK_TO_VSC))<<16);
                check(keys->TestKeyDown('M',flags,&preview));
                require(preview && configBytes()==beforeConfig,"Schema preview changed configuration");
                require(event(first,'M',true,true),"Recent schema shortcut was not handled");
                const auto switchedConfig=configBytes();
                require(switchedConfig!=beforeConfig,"Recent shortcut did not publish selection");
                require(first.store->text==baseline+L"qqqqqq" && compositionCount(first)==1,"Schema shortcut lost active composition");
                event(first,'M',false);
                require(event(first,'M',true,true),"Held modifier repeat was not consumed");
                require(event(first,VK_OEM_PLUS,true,true),"Held modifier rollover was not consumed");
                require(configBytes()==switchedConfig,"Held Ctrl switched schema twice");
                event(first,'M',false);event(first,VK_OEM_PLUS,false);
                SetKeyboardState(emptyKeys);event(first,VK_CONTROL,false);
                tap(first,VK_SPACE);
                require(first.store->text==baseline+selectedWord,"Schema shortcut committed from old dictionary");
            }
        }
        if(mixed) {
            const auto baseline=first.store->text;
            for(auto vk:{'A','B','A','B','A'}) tap(first,vk);
            require(first.store->text==baseline+L"交交a","Mixed surface did not replace raw code");
            require(compositionCount(first)==1,"Mixed prefix split the TSF composition");
            tap(first,VK_BACK);
            require(first.store->text==baseline+L"交ab","Mixed backspace did not reopen previous segment");
            tap(first,'A'); tap(first,'B');
            check(uiManager->GetUIElement(ui->id,element.ReleaseAndGetAddressOf())); check(element.As(&candidates));
            if(nativeUI) {
                HWND candidateWindow=ownCandidateWindow();
                check(candidates->SetSelection(1));
                const auto point=highlightedCandidate(candidateWindow);
                SendMessageW(candidateWindow,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
            } else { check(candidates->SetSelection(1)); check(candidates->Finalize()); }
            pump();
            require(first.store->text==baseline+L"交交疒","Mixed finalization dropped or duplicated prefix");
            require(compositionCount(first)==0,"Mixed commit retained TSF composition");
            const auto committed=first.store->text;
            for(auto vk:{'A','B','A','B','A'}) tap(first,vk);
            tap(first,VK_RETURN);
            require(first.store->text==committed+L"ababa","Mixed raw Enter lost original keys");
            for(auto vk:{'A','B','A','B','A'}) tap(first,vk);
            tap(first,VK_ESCAPE);
            require(first.store->text==committed+L"ababa","Mixed cancel left decoded prefix in document");
        }
        if(addWord) {
            // Hold the isolated journal without write sharing so a real
            // Ctrl+2 adjustment fails before append inside the loaded DLL.
            const auto beforeFailure=first.store->text;
            tap(first,'A'); tap(first,'B');
            wchar_t userRoot[32768]{};
            require(GetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",userRoot,32768)>0,"Failure test requires isolated user data");
            const auto journal=std::filesystem::path(userRoot)/L"user"/L"tiger-words.tcu";
            HANDLE blocked=CreateFileW(journal.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            require(blocked!=INVALID_HANDLE_VALUE,"Cannot deny journal writes for failure test");
            BYTE failedCtrl[256]{}; failedCtrl[VK_CONTROL]=failedCtrl[VK_LCONTROL]=0x80;
            SetKeyboardState(failedCtrl);
            try { require(event(first,'2',true,true),"Adjustment key was not handled"); }
            catch(...) { CloseHandle(blocked); throw; }
            CloseHandle(blocked); SetKeyboardState(emptyKeys); event(first,'2',false);
            check(uiManager->GetUIElement(ui->id,element.ReleaseAndGetAddressOf())); check(element.As(&candidates));
            BSTR firstLabel=nullptr; check(candidates->GetString(0,&firstLabel));
            const std::wstring restored(firstLabel,SysStringLen(firstLabel)); SysFreeString(firstLabel);
            require(restored==L"交","Failed adjustment left unpersisted candidate order");
            tap(first,VK_SPACE);
            require(first.store->text==beforeFailure+L"交","Failed adjustment affected subsequent commit");
            const auto baseline=first.store->text;
            AddWordCheck dialogCheck{&first.store->lock}; addWordCheck=&dialogCheck;
            const auto timer=SetTimer(nullptr,0,20,saveAddedWord);
            require(timer!=0,"Cannot schedule add-word test");
            BYTE ctrlKeys[256]{}; ctrlKeys[VK_CONTROL]=ctrlKeys[VK_LCONTROL]=0x80;
            SetKeyboardState(ctrlKeys);
            const bool opened=event(first,VK_OEM_PLUS,true,true);
            KillTimer(nullptr,timer); addWordCheck=nullptr;
            SetKeyboardState(emptyKeys);
            require(opened && dialogCheck.seen,"TSF shortcut did not open add-word dialog");
            require(dialogCheck.error.empty(),dialogCheck.error.c_str());
            require(GetForegroundWindow()==window && GetFocus()==window,"Add-word close did not restore host focus");
            ComPtr<ITfDocumentMgr> restoredDocument; check(manager->GetFocus(&restoredDocument));
            require(restoredDocument.Get()==first.manager.Get(),"Add-word close did not restore TSF document focus");
            event(first,VK_OEM_PLUS,false);
            for(int i=0;i<6;++i) tap(first,'A');
            tap(first,VK_SPACE);
            require(first.store->text==baseline+L"原生加词验证","Saved word unavailable through TSF engine");
            const auto secondBaseline=second.store->text;
            SetForegroundWindow(secondWindow); SetFocus(secondWindow);
            check(manager->SetFocus(second.manager.Get())); pump();
            for(int i=0;i<6;++i) tap(second,'A');
            tap(second,VK_SPACE);
            require(second.store->text==secondBaseline+L"原生加词验证","Saved word unavailable in second TSF context");
        }
        if(timerTest) {
            require(timerWindows()==0 && reminderChildren().empty(),"Unexpected timer before command");
            const auto beforeTimer=first.store->text;
            BYTE shifted[256]{};shifted[VK_SHIFT]=shifted[VK_LSHIFT]=0x80;
            SetKeyboardState(shifted);event(first,'D',true,true);
            SetKeyboardState(emptyKeys);event(first,'D',false);
            tap(first,'S');tap(first,'1');
            require(first.store->text==beforeTimer+L"Ds1","Timer command preedit differs");
            timerPreview=true;
            const auto started=GetTickCount64();
            event(first,VK_SPACE,true,true);timerPreview=false;event(first,VK_SPACE,false);
            const auto children=reminderChildren();
            require(first.store->text==beforeTimer && timerWindows()==0 && children.size()==1,"Timer command did not launch exactly one reminder helper");
            reminderProcessId=children.front();
            OwnedReminder owned{OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE,FALSE,reminderProcessId)};
            require(owned.process!=nullptr,"Cannot track own reminder helper");
            ReminderCheck reminder;
            while(!reminder.window && GetTickCount64()-started<70000) {
                pump();EnumWindows(findReminder,reinterpret_cast<LPARAM>(&reminder));Sleep(5);
            }
            reminderDelay=GetTickCount64()-started;
            require(reminder.window && reminder.text,"Timer expiry did not show expected reminder");
            require(reminderDelay>=59000,"One-minute timer fired early");
            const auto closed=GetTickCount64();
            while(IsWindow(reminder.window) && GetTickCount64()-closed<2000) {
                EnumChildWindows(reminder.window,reminderText,reinterpret_cast<LPARAM>(&reminder));
                if(reminder.button) PostMessageW(reminder.button,BM_CLICK,0,0);
                pump();Sleep(20);
            }
            require(!IsWindow(reminder.window),"Timer reminder did not close");
            require(WaitForSingleObject(owned.process,5000)==WAIT_OBJECT_0,"Dismissed reminder helper stayed resident");
        }
        verifyModule(module);
        SetKeyboardState(savedKeys);
        check(profiles->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,clsid,profile,nullptr,TF_IPPMF_FORPROCESS)); pump();
        check(first.manager->Pop(TF_POPF_ALL)); check(second.manager->Pop(TF_POPF_ALL));
        check(source->UnadviseSink(uiCookie)); check(manager->Deactivate()); DestroyWindow(secondWindow); DestroyWindow(window);
        if(timerTest) require(timerWindows()==0 && reminderChildren().empty(),"Completed timer retained a helper or scheduler");
        std::cout<<"{\"events\":"<<events<<",\"contexts\":2,\"retained_inactive_composition\":"<<(retained?"true":"false")
            <<",\"timer_expiry_ms\":"<<reminderDelay
            <<",\"schema_switch\":"<<(schemaTest?"true":"false")
            <<",\"dialog_key_events\":"<<(addWord?12:0)<<",\"dialog_focus_restored\":"<<(addWord?"true":"false")
            <<",\"real_tsf_edit_sessions\":true,\"ui_less\":"<<(nativeUI?"false":"true")
            <<",\"mouse_selection\":"<<(nativeUI?"true":"false")<<",\"mixed_input\":"<<(mixed?"true":"false")<<",\"add_word\":"<<(addWord?"true":"false")<<",\"layout_recovery\":true,\"private_activation\":"<<(argc>=4?"true":"false")<<",\"module_verified\":true,\"status\":\"passed\"}\n";
        // COM smart pointers are released before process teardown; no forced DLL unload.
        return 0;
    } catch(const std::exception& error) { dumpKeyObservations();dumpTerminationObservations();std::cerr<<error.what()<<'\n'; return 1; }
}
