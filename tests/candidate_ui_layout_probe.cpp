// Production CandidateUI/model/rendering, with an unactivated owner and a
// notification observer for deterministic failure/reentrancy injection.
#define NOMINMAX
#include <windows.h>
#include <msctf.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include "../native/LexiconSerialize.h"
#include "../native/tsf/CandidateUI.cpp"
#include "candidate_ui_probe_owner.h"

namespace {
using namespace tiger;
using namespace tiger::tsf;
unsigned checks=0;
void require(bool value,const char* reason) {
    ++checks;if(!value)throw std::runtime_error(reason);
}
void check(HRESULT hr) { require(SUCCEEDED(hr),"COM call failed"); }
void pump() {
    MSG message{};
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
        TranslateMessage(&message);DispatchMessageW(&message);
    }
}
void press(Engine& engine,int vk) { KeyEvent key;key.vk=vk;engine.process(key); }
struct Manager final:ITfUIElementMgr {
    CandidateUI* ui=nullptr;
    unsigned calls=0;
    DWORD flags=0;
    bool fail=false;
    std::function<void()> during;
    STDMETHODIMP QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(iid!=IID_IUnknown && iid!=IID_ITfUIElementMgr)return E_NOINTERFACE;
        *out=static_cast<ITfUIElementMgr*>(this);AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override {return 2;}
    STDMETHODIMP_(ULONG) Release() override {return 1;}
    STDMETHODIMP BeginUIElement(ITfUIElement*,BOOL*,DWORD*) override {return E_NOTIMPL;}
    STDMETHODIMP EndUIElement(DWORD) override {return E_NOTIMPL;}
    STDMETHODIMP GetUIElement(DWORD,ITfUIElement**) override {return E_NOTIMPL;}
    STDMETHODIMP EnumUIElements(IEnumTfUIElements**) override {return E_NOTIMPL;}
    STDMETHODIMP UpdateUIElement(DWORD id) override {
        require(id==1,"Wrong UI element notified");++calls;
        check(ui->GetUpdatedFlags(&flags));require(flags!=0,"Empty candidate notification");
        if(during)during();
        return fail?E_FAIL:S_OK;
    }
};
std::shared_ptr<const Lexicon> fixture(const std::filesystem::path& path) {
    ImportedLexicon data;
    data.main={{u"a",{u"甲",u"乙",u"丙",u"丁",u"戊",u"己",u"庚",u"辛",u"壬",u"癸",u"子",u"丑"}},
               {u"ab",{u"你好",u"世界",u"候选内容更新"}}};
    data.indexedMain={{u"a",8,0},{u"ab",8,1}};
    const auto bytes=serializeImportedLexicon(data);
    require(!std::filesystem::exists(path),"Fixture path already exists");
    std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    file.close();require(static_cast<bool>(file),"Fixture write failed");
    return std::make_shared<Lexicon>(Dictionary::Open(path));
}
std::vector<UINT> pages(CandidateUI& ui) {
    UINT size=0;check(ui.GetPageIndex(nullptr,0,&size));
    std::vector<UINT> result(size);check(ui.GetPageIndex(result.data(),size,&size));return result;
}
UINT selection(CandidateUI& ui) {UINT result=0;check(ui.GetSelection(&result));return result;}
DWORD pending(CandidateUI& ui) {DWORD result=0;check(ui.GetUpdatedFlags(&result));return result;}
std::u16string label(CandidateUI& ui,UINT index) {
    BSTR value=nullptr;check(ui.GetString(index,&value));
    std::u16string result(reinterpret_cast<const char16_t*>(value),SysStringLen(value));SysFreeString(value);return result;
}
void run(const std::shared_ptr<const Lexicon>& lexicon,bool native) {
    ComPtr<Service> owner;owner.Attach(new Service);
    Config config;config.maxCodeLength=16;
    auto state=std::make_shared<Context>(nullptr,lexicon,config);
    press(state->engine,'A');
    CandidateStyle style;style.animationEnabled=false;
    style.candidateDelayMs=style.annotationDelayMs=0;
    ComPtr<CandidateUI> ui;ui.Attach(new CandidateUI(owner.Get(),state,style,nullptr));
    Manager manager;manager.ui=ui.Get();
    RECT caret{100,100,102,125};
    check(ui->Show(native?TRUE:FALSE));
    // Initial model exists before BeginUIElement; its host selection is retained.
    UINT custom[]={0,2,5};check(ui->SetPageIndex(custom,3));check(ui->SetSelection(5));
    ui->update(&caret,nullptr,false,CandidateUpdate::Layout);pump();
    const std::vector<UINT> expected(std::begin(custom),std::end(custom));
    require(selection(*ui.Get())==5 && pages(*ui.Get())==expected,"Layout reset host selection or paging");
    check(ui->notifyUpdated(&manager,1));require(manager.calls==1 && pending(*ui.Get())==0,"Initial notification missing");
    require((manager.flags&TF_CLUIE_DOCUMENTMGR)!=0,"Initial document flag missing");
    HWND window=nullptr;
    if(native)EnumThreadWindows(GetCurrentThreadId(),[](HWND w,LPARAM data)->BOOL {
        wchar_t name[128]{};GetClassNameW(w,name,128);
        if(wcscmp(name,L"NativeTiger.Candidate.v1")!=0)return TRUE;
        *reinterpret_cast<HWND*>(data)=w;return FALSE;
    },reinterpret_cast<LPARAM>(&window));
    RECT original{};
    if(native)require(window && IsWindowVisible(window) && GetWindowRect(window,&original),"Native frame not visible");
    for(unsigned i=0;i<12;++i) {
        KeyEvent up;up.vk='A';up.down=false;state->engine.process(up);++state->revision;
        caret.left+=3;caret.right+=3;caret.top+=2;caret.bottom+=2;
        ui->update(i%3?&caret:nullptr,nullptr,i%3==0,CandidateUpdate::Layout);pump();
        require(ui->notifyUpdated(&manager,1)==S_FALSE,"Layout-only update notified the model");
        require(selection(*ui.Get())==5 && pages(*ui.Get())==expected,"Layout reset host selection or paging");
        require(label(*ui.Get(),5)==u"己" && pending(*ui.Get())==0,"Layout changed candidate data");
    }
    if(native) {
        RECT moved{};require(IsWindowVisible(window) && GetWindowRect(window,&moved),"Layout recovery lost frame");
        require(moved.left!=original.left && moved.top!=original.top &&
            moved.right-moved.left==original.right-original.left && moved.bottom-moved.top==original.bottom-original.top,
            "Layout did not move geometry independently of content");
    }
    // Real content update replaces the custom page table and reports only the
    // changed fields (the document manager and count remain unchanged here).
    state->engine.setPage(1);
    ui->update(&caret,nullptr);check(ui->notifyUpdated(&manager,1));
    require(selection(*ui.Get())==5 && pages(*ui.Get())==std::vector<UINT>({0,5,10}),"Content paging not published");
    require((manager.flags&TF_CLUIE_PAGEINDEX) && (manager.flags&TF_CLUIE_CURRENTPAGE) &&
        !(manager.flags&(TF_CLUIE_COUNT|TF_CLUIE_DOCUMENTMGR|TF_CLUIE_SELECTION)),"Incorrect paging change flags");
    // Failed notification: retry the pending flags, never reset host state in
    // order to retry, and never turn every subsequent layout into a notification.
    press(state->engine,'B');ui->update(&caret,nullptr);
    manager.fail=true;require(ui->notifyUpdated(&manager,1)==E_FAIL && pending(*ui.Get()),"Failed notification was lost");
    require((manager.flags&TF_CLUIE_COUNT) && label(*ui.Get(),0)==u"你好","Content update did not replace strings/count");
    UINT shortPages[]={0,1};check(ui->SetPageIndex(shortPages,2));check(ui->SetSelection(1));
    manager.fail=false;ui->update(&caret,nullptr,false,CandidateUpdate::Layout);check(ui->notifyUpdated(&manager,1));
    require(selection(*ui.Get())==1 && pages(*ui.Get())==std::vector<UINT>({0,1}),"Notification retry reset host state");
    const auto notified=manager.calls;
    ui->update(&caret,nullptr,false,CandidateUpdate::Layout);require(ui->notifyUpdated(&manager,1)==S_FALSE && manager.calls==notified,"Notification acknowledged more than once");
    // Reentrant content during UpdateUIElement must remain pending. Recursively
    // notifying the manager would loop; acknowledging the older revision loses it.
    state->engine.cancel();press(state->engine,'A');ui->update(&caret,nullptr);
    manager.during=[&] {
        press(state->engine,'B');ui->update(&caret,nullptr);
        require(ui->notifyUpdated(&manager,1)==S_FALSE,"Reentrant notification not coalesced");
    };
    check(ui->notifyUpdated(&manager,1));manager.during={};
    require(pending(*ui.Get())!=0,"Old notification acknowledged new content");
    check(ui->SetSelection(1));ui->update(&caret,nullptr,false,CandidateUpdate::Layout);
    check(ui->notifyUpdated(&manager,1));require(pending(*ui.Get())==0 && selection(*ui.Get())==1,"Reentrant update not delivered");
    // A layout during the notification is not a new model revision.
    ui->update(&caret,nullptr);manager.during=[&] {ui->update(&caret,nullptr,false,CandidateUpdate::Layout);};
    check(ui->notifyUpdated(&manager,1));manager.during={};require(pending(*ui.Get())==0,"Reentrant layout dirtied the model");
    // The notification may synchronously end this UI. Its lifetime is retained,
    // and a late layout/retry must not recreate or notify the detached element.
    ui->update(&caret,nullptr);manager.during=[&] {ui->detach();};
    check(ui->notifyUpdated(&manager,1));manager.during={};
    ui->update(&caret,nullptr,false,CandidateUpdate::Layout);
    require(ui->notifyUpdated(&manager,1)==S_FALSE && pending(*ui.Get())==0,"Detached model notified again");
    if(native)require(!IsWindow(window),"Detached window survived");
    pump();ui.Reset();owner.Reset();require(candidate_probe::dllRefs==0,"UI reference leaked");
}
}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==2,"candidate_ui_layout_probe <new-fixture>");
        check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
        const auto lexicon=fixture(argv[1]);run(lexicon,false);run(lexicon,true);
        std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"native_and_uiless\":true,"
            "\"notification_failures_and_reentry\":true,\"real_renderer\":true,\"tsf_owner_mocked\":true}\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
