// Reuse real TSF manager/text-store support; never call the legacy entry point.
#define wmain legacyTsfHostMain
#include "tsf_host.cpp"
#undef wmain
#include "candidate_layout_thread_manager.h"

namespace {
struct KeyboardStateGuard {
    BYTE saved[256]{};
    KeyboardStateGuard() {
        require(GetKeyboardState(saved)!=FALSE,"Cannot save thread keyboard state");
        BYTE empty[256]{};
        require(SetKeyboardState(empty)!=FALSE,"Cannot clear thread keyboard state");
    }
    ~KeyboardStateGuard() { SetKeyboardState(saved); }
};
void drainLayout(unsigned milliseconds=50) {
    const auto deadline=GetTickCount64()+milliseconds;
    do { pump(); MsgWaitForMultipleObjects(0,nullptr,FALSE,1,QS_ALLINPUT); }
    while(GetTickCount64()<deadline);
}
}


int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==3,"candidate_sentence_focus_host <staged-DLL> <isolated-root>");
        const std::filesystem::path root=argv[2];
        require(root.is_absolute() && std::filesystem::is_regular_file(root/L".candidate-layout-test"),"Isolated root required");
        require(SetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",root.c_str())!=FALSE,"Cannot isolate test data");
        check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
        KeyboardStateGuard keyboard;
        HMODULE module=LoadLibraryW(argv[1]);
        require(module!=nullptr,"Cannot load staged DLL");
        auto getClass=reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID,REFIID,void**)>(GetProcAddress(module,"DllGetClassObject"));
        require(getClass!=nullptr,"Class factory export missing");
        CLSID clsid; check(CLSIDFromString(L"{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}",&clsid));
        ComPtr<IClassFactory> factory; check(getClass(clsid,IID_PPV_ARGS(&factory)));
        ComPtr<ITfTextInputProcessorEx> service; check(factory->CreateInstance(nullptr,IID_PPV_ARGS(&service)));
        ComPtr<ITfThreadMgrEx> manager;
        check(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager)));
        TfClientId appClient; check(manager->ActivateEx(&appClient,TF_TMAE_NOACTIVATETIP|TF_TMAE_NOACTIVATEKEYBOARDLAYOUT));
        ComPtr<LayoutThreadManager> adapter;adapter.Attach(new LayoutThreadManager(manager.Get(),appClient));
        ComPtr<ITfSource> source; check(manager.As(&source));
        ComPtr<UISink> ui; ui.Attach(new UISink); DWORD uiCookie=TF_INVALID_COOKIE;
        check(source->AdviseSink(IID_ITfUIElementSink,ui.Get(),&uiCookie));
        HWND window=CreateWindowExW(0,L"STATIC",L"Tigirl layout regression",WS_OVERLAPPEDWINDOW,
            100,100,600,500,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        require(window!=nullptr,"Cannot create hidden text-store owner");
        auto doc=document(manager.Get(),appClient,window);
        auto other=document(manager.Get(),appClient,window);
        check(manager->SetFocus(doc.manager.Get()));
        check(service->ActivateEx(adapter.Get(),appClient,TF_TMAE_UIELEMENTENABLEDONLY));
        ComPtr<ITfThreadMgrEventSink> focus; check(service.As(&focus));
        check(focus->OnSetFocus(doc.manager.Get(),nullptr));
        ComPtr<ITfKeyEventSink> keys; check(service.As(&keys));
        ComPtr<ITfTextLayoutSink> layout; check(service.As(&layout));
        ComPtr<ITfUIElementMgr> elements; check(manager.As(&elements));

        sentence_tsf_fixture::run(service.Get(),manager.Get(),doc,other,ui.Get());
        ComPtr<ITfThreadFocusSink> threadFocus;check(service.As(&threadFocus));
        auto tap=[&](UINT vk) {
            BOOL eaten=FALSE;check(keys->OnTestKeyDown(doc.context.Get(),vk,1,&eaten));
            require(eaten!=FALSE,"Sentence key not handled");check(keys->OnKeyDown(doc.context.Get(),vk,1,&eaten));
            check(keys->OnTestKeyUp(doc.context.Get(),vk,0xc0000001,&eaten));
            if(eaten)check(keys->OnKeyUp(doc.context.Get(),vk,0xc0000001,&eaten));
        };
        auto type=[&]{for(auto k:{'A','A','B','B'})tap(k);};
        auto list=[&] {
            require(ui->id!=TF_INVALID_UIELEMENTID,"Sentence candidate element missing");
            ComPtr<ITfUIElement> element;check(elements->GetUIElement(ui->id,&element));
            ComPtr<ITfCandidateListUIElement> result;check(element.As(&result));return result;
        };
        const auto priorText=doc.store->text;
        // Completion arrives while hidden: no document writes, then exactly one
        // foreground publication; the same input no longer needs a new key.
        type();check(threadFocus->OnKillThreadFocus());
        const auto rawText=doc.store->text;const auto writes=ui->updates;
        drainLayout(300);
        require(doc.store->text==rawText && ui->updates==writes,"Background decode changed document or published UI");
        check(threadFocus->OnSetThreadFocus());drainLayout(300);
        auto candidates=list();BSTR top=nullptr;check(candidates->GetString(0,&top));
        const std::wstring topText(top,SysStringLen(top));SysFreeString(top);
        require(topText==L"中国" && compositionCount(doc)==1,"Cached completion did not publish on focus recovery");
        tap(VK_TAB);UINT selected=0;check(list()->GetSelection(&selected));require(selected==1,"Manual sentence selection missing");
        const auto id=ui->id;const auto updates=ui->updates;
        check(threadFocus->OnKillThreadFocus());check(keys->OnSetFocus(FALSE));drainLayout();
        check(keys->OnSetFocus(TRUE));check(threadFocus->OnSetThreadFocus());drainLayout();
        check(list()->GetSelection(&selected));
        require(selected==1 && ui->id==id && ui->updates==updates,"Focus reset or republished manual sentence selection");
        tap(VK_SPACE);require(doc.store->text==priorText+L"中华","Recovered selection committed wrong sentence");
        // Also exercise a worker result already captured by a deferred edit
        // callback when thread focus changes, then restore without a new key.
        const auto committed=doc.store->text;type();doc.store->deferLocks=true;
        const auto deadline=GetTickCount64()+3000;
        while(!doc.store->pendingLock && GetTickCount64()<deadline)drainLayout(5);
        require(doc.store->pendingLock!=0,"Sentence completion did not defer");
        check(threadFocus->OnKillThreadFocus());const auto deferredText=doc.store->text;
        doc.store->deferLocks=false;check(doc.store->grantPendingLock());drainLayout(80);
        require(doc.store->text==deferredText,"Deferred unfocused decode wrote document");
        check(threadFocus->OnSetThreadFocus());drainLayout(150);
        top=nullptr;check(list()->GetString(0,&top));const std::wstring restored(top,SysStringLen(top));SysFreeString(top);
        require(restored==L"中国","Deferred cached completion was stranded on recovery");
        tap(VK_ESCAPE);require(doc.store->text==committed && compositionCount(doc)==0,"Cancel after recovery lost committed text");
        check(service->Deactivate());check(doc.manager->Pop(TF_POPF_ALL));check(other.manager->Pop(TF_POPF_ALL));
        check(source->UnadviseSink(uiCookie));check(manager->Deactivate());DestroyWindow(window);
        std::cout<<"{\"status\":\"passed\",\"real_tsf_sentence_focus\":true,\"background_completion_cached\":true,"
            "\"deferred_completion_recovered\":true,\"manual_selection_retained\":true,\"existing_sentence_fixture\":true,"
            "\"physical_input_tested\":false,\"profile_registered\":false}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
