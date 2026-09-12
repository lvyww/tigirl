// Actual staged DLL + TSF contexts/edit sessions. Seed only the geometry memory
// to isolate its lifecycle from rendering; the separate UI probe tests pixels.
#define wmain legacyTsfHostMain
#include "tsf_host.cpp"
#undef wmain
#include "candidate_layout_thread_manager.h"
#include "../native/tsf/Service.h"

namespace tiger::tsf {
struct ServicePlacementProbe {
    static std::shared_ptr<Context> find(ITfTextInputProcessorEx* object,ITfContext* context) {
        auto service=static_cast<Service*>(object); // Matching, locally built DLL only.
        for(const auto& item:service->contexts_)
            if(item.second->context.Get()==context)return item.second;
        throw std::runtime_error("Service context missing");
    }
    static void seed(const std::shared_ptr<Context>& state) {
        ComPtr<ITfContextView> view;
        check(state->context->GetActiveView(&view));
        check(view.As(&state->placementView));
        CandidatePlacementEnvironment env;env.work={0,0,1920,1040};POINT p{};
        state->placement.reset();
        require(state->placement.place({500,980,501,1000},env,240,120,p) && state->placement.above(),"Cannot seed above memory");
    }
};
}
namespace {
void drainPlacement(unsigned milliseconds=30) {
    const auto end=GetTickCount64()+milliseconds;
    do {pump();MsgWaitForMultipleObjects(0,nullptr,FALSE,1,QS_ALLINPUT);}while(GetTickCount64()<end);
}
struct PlacementKeyboardGuard {
    BYTE saved[256]{};
    PlacementKeyboardGuard() {
        require(GetKeyboardState(saved)!=FALSE,"Cannot save keyboard state");
        BYTE empty[256]{};require(SetKeyboardState(empty)!=FALSE,"Cannot clear keyboard state");
    }
    ~PlacementKeyboardGuard(){SetKeyboardState(saved);}
};
}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==5,"candidate_placement_host <staged-DLL> <isolated-root> <code> <unused-sixth>");
        const std::filesystem::path root=argv[2];
        require(root.is_absolute() && std::filesystem::is_regular_file(root/L".candidate-layout-test"),"Isolated marked root required");
        require(SetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",root.c_str())!=FALSE,"Cannot isolate test data");
        check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));PlacementKeyboardGuard keyboard;
        HMODULE module=LoadLibraryW(argv[1]);require(module!=nullptr,"Cannot load staged DLL");
        auto getClass=reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID,REFIID,void**)>(GetProcAddress(module,"DllGetClassObject"));
        require(getClass!=nullptr,"Class factory missing");
        CLSID clsid;check(CLSIDFromString(L"{D2291A80-84D8-4641-9AB2-BDD1472C846B}",&clsid));
        ComPtr<IClassFactory> factory;check(getClass(clsid,IID_PPV_ARGS(&factory)));
        ComPtr<ITfTextInputProcessorEx> service;check(factory->CreateInstance(nullptr,IID_PPV_ARGS(&service)));
        ComPtr<ITfThreadMgrEx> manager;check(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager)));
        TfClientId client;check(manager->ActivateEx(&client,TF_TMAE_NOACTIVATETIP|TF_TMAE_NOACTIVATEKEYBOARDLAYOUT));
        ComPtr<LayoutThreadManager> adapter;adapter.Attach(new LayoutThreadManager(manager.Get(),client));
        ComPtr<ITfSource> source;check(manager.As(&source));
        ComPtr<UISink> ui;ui.Attach(new UISink);DWORD cookie=TF_INVALID_COOKIE;
        check(source->AdviseSink(IID_ITfUIElementSink,ui.Get(),&cookie));
        HWND window=CreateWindowExW(0,L"STATIC",L"Placement lifetime",WS_OVERLAPPEDWINDOW,100,100,600,500,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        require(window!=nullptr,"Cannot create owner");
        auto first=document(manager.Get(),client,window),second=document(manager.Get(),client,window);
        check(manager->SetFocus(first.manager.Get()));
        check(service->ActivateEx(adapter.Get(),client,TF_TMAE_UIELEMENTENABLEDONLY));
        ComPtr<ITfThreadMgrEventSink> focus;check(service.As(&focus));
        ComPtr<ITfThreadFocusSink> threadFocus;check(service.As(&threadFocus));
        ComPtr<ITfKeyEventSink> keys;check(service.As(&keys));
        ComPtr<ITfTextLayoutSink> layout;check(service.As(&layout));
        ComPtr<ITfUIElementMgr> elements;check(manager.As(&elements));
        check(focus->OnSetFocus(first.manager.Get(),nullptr));drainPlacement(400);
        const std::wstring code=argv[3];
        auto type=[&] {
            for(auto c:code) {
                require(c>=L'a' && c<=L'z',"Invalid fixture code");BOOL eaten=FALSE;
                const UINT vk=static_cast<UINT>(c-L'a'+L'A');
                check(keys->OnTestKeyDown(first.context.Get(),vk,1,&eaten));require(eaten!=FALSE,"Code not handled");
                check(keys->OnKeyDown(first.context.Get(),vk,1,&eaten));
                check(keys->OnTestKeyUp(first.context.Get(),vk,0xc0000001,&eaten));
                if(eaten)check(keys->OnKeyUp(first.context.Get(),vk,0xc0000001,&eaten));pump();
            }
        };
        auto candidates=[&] {
            require(ui->id!=TF_INVALID_UIELEMENTID,"UI element missing");
            ComPtr<ITfUIElement> element;check(elements->GetUIElement(ui->id,&element));
            ComPtr<ITfCandidateListUIElementBehavior> list;check(element.As(&list));return list;
        };
        using Probe=tiger::tsf::ServicePlacementProbe;
        type();auto state=Probe::find(service.Get(),first.context.Get());Probe::seed(state);
        check(candidates()->Finalize());drainPlacement();
        require(!state->composition && state->placement.above(),"Real commit cleared direction memory");
        require(ui->id==TF_INVALID_UIELEMENTID,"Commit kept old UI alive");
        const auto committed=first.store->text;
        type();require(Probe::find(service.Get(),first.context.Get())==state && state->placement.above(),"New composition lost context memory");
        first.store->layoutReady=false;check(layout->OnLayoutChange(first.context.Get(),TF_LC_CHANGE,nullptr));drainPlacement();
        require(state->placement.above(),"TS_E_NOLAYOUT cleared direction memory");
        first.store->layoutReady=true;check(layout->OnLayoutChange(first.context.Get(),TF_LC_CHANGE,nullptr));drainPlacement();
        require(state->placement.above(),"Ordinary layout reset memory");
        check(candidates()->Abort());drainPlacement();
        require(!state->composition && state->placement.above() && first.store->text==committed,"Real cancel cleared memory or changed text");
        check(focus->OnSetFocus(first.manager.Get(),first.manager.Get()));drainPlacement();
        require(state->placement.above(),"Duplicate focus callback cleared memory");
        check(keys->OnSetFocus(FALSE));require(!state->placement.valid(),"Key focus loss retained memory");
        check(keys->OnSetFocus(TRUE));Probe::seed(state);
        check(threadFocus->OnKillThreadFocus());require(!state->placement.valid(),"Thread focus loss retained memory");
        check(threadFocus->OnSetThreadFocus());Probe::seed(state);
        check(manager->SetFocus(second.manager.Get()));check(focus->OnSetFocus(second.manager.Get(),first.manager.Get()));
        auto other=Probe::find(service.Get(),second.context.Get());
        require(!state->placement.valid() && !other->placement.valid(),"Same-HWND context switch inherited memory");
        check(manager->SetFocus(first.manager.Get()));check(focus->OnSetFocus(first.manager.Get(),second.manager.Get()));
        require(!state->placement.valid(),"Returning focus restored stale memory");Probe::seed(state);
        check(layout->OnLayoutChange(first.context.Get(),TF_LC_DESTROY,nullptr));
        require(!state->placement.valid() && !state->placementView,"View destruction retained memory/view reference");
        Probe::seed(other);check(focus->OnPopContext(second.context.Get()));
        require(!other->placement.valid() && !other->placementView,"Context pop retained memory");
        Probe::seed(state);verifyModule(module);check(service->Deactivate());
        require(!state->placement.valid() && !state->placementView,"Deactivation retained memory");
        state.reset();other.reset();
        check(first.manager->Pop(TF_POPF_ALL));check(second.manager->Pop(TF_POPF_ALL));
        check(source->UnadviseSink(cookie));check(manager->Deactivate());DestroyWindow(window);
        std::cout<<"{\"status\":\"passed\",\"probe\":\"candidate_placement_lifetime\",\"real_tsf_edit_sessions\":true,"
                   "\"seeded_geometry\":true,\"commit_cancel_preserved\":true,\"duplicate_focus_preserved\":true,"
                   "\"focus_context_view_deactivation_reset\":true,\"physical_input_tested\":false}\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
