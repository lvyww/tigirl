// Reuse the existing real TSF manager/text-store test support, not a mock engine.
// Rename its legacy entry point; this probe never calls profile registration,
// switches the foreground window or injects global keyboard input.
#define wmain legacyTsfHostMain
#include "tsf_host.cpp"
#undef wmain

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
void drainLayout() {
    const auto deadline=GetTickCount64()+50;
    do { pump(); MsgWaitForMultipleObjects(0,nullptr,FALSE,1,QS_ALLINPUT); }
    while(GetTickCount64()<deadline);
}
}

int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==5,"candidate_layout_host <staged-DLL> <isolated-root> <code> <expected-sixth>");
        const std::filesystem::path root=argv[2];
        require(root.is_absolute() && std::filesystem::is_regular_file(root/L".candidate-layout-test"),
            "An isolated, marked test root is required");
        require(SetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",root.c_str())!=FALSE,"Cannot isolate test data");
        check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
        KeyboardStateGuard keyboard;
        HMODULE module=LoadLibraryW(argv[1]);
        require(module!=nullptr,"Cannot load staged DLL");
        auto getClass=reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID,REFIID,void**)>(GetProcAddress(module,"DllGetClassObject"));
        require(getClass!=nullptr,"Class factory export missing");
        CLSID clsid; check(CLSIDFromString(L"{D2291A80-84D8-4641-9AB2-BDD1472C846B}",&clsid));
        ComPtr<IClassFactory> factory; check(getClass(clsid,IID_PPV_ARGS(&factory)));
        ComPtr<ITfTextInputProcessorEx> service; check(factory->CreateInstance(nullptr,IID_PPV_ARGS(&service)));
        ComPtr<ITfThreadMgrEx> manager;
        check(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager)));
        TfClientId appClient; check(manager->ActivateEx(&appClient,TF_TMAE_NOACTIVATEKEYBOARDLAYOUT));
        ComPtr<ITfClientId> ids; check(manager.As(&ids));
        TfClientId serviceClient; check(ids->GetClientId(clsid,&serviceClient));
        ComPtr<ITfSource> source; check(manager.As(&source));
        ComPtr<UISink> ui; ui.Attach(new UISink); DWORD uiCookie=TF_INVALID_COOKIE;
        check(source->AdviseSink(IID_ITfUIElementSink,ui.Get(),&uiCookie));
        HWND window=CreateWindowExW(0,L"STATIC",L"Tigirl layout regression",WS_OVERLAPPEDWINDOW,
            100,100,600,500,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        require(window!=nullptr,"Cannot create hidden text-store owner");
        auto doc=document(manager.Get(),appClient,window);
        check(manager->SetFocus(doc.manager.Get()));
        check(service->ActivateEx(manager.Get(),serviceClient,TF_TMAE_UIELEMENTENABLEDONLY));
        ComPtr<ITfThreadMgrEventSink> focus; check(service.As(&focus));
        check(focus->OnSetFocus(doc.manager.Get(),nullptr));
        ComPtr<ITfKeyEventSink> keys; check(service.As(&keys));
        ComPtr<ITfTextLayoutSink> layout; check(service.As(&layout));
        ComPtr<ITfUIElementMgr> elements; check(manager.As(&elements));
        auto tap=[&](UINT vk) {
            BOOL eaten=FALSE;
            check(keys->OnTestKeyDown(doc.context.Get(),vk,1,&eaten));
            const bool handled=eaten!=FALSE;
            if(eaten) check(keys->OnKeyDown(doc.context.Get(),vk,1,&eaten));
            // Include the key-up state revision, which is not a candidate change.
            check(keys->OnTestKeyUp(doc.context.Get(),vk,0xc0000001,&eaten));
            if(eaten) check(keys->OnKeyUp(doc.context.Get(),vk,0xc0000001,&eaten));
            pump(); return handled;
        };
        const std::wstring code=argv[3],expected=argv[4];
        require(code.size()>=2 && code.size()<=4,"Fixture code must be 2-4 ASCII letters");
        auto typeCode=[&] {
            for(auto c:code) {
                require(c>=L'a' && c<=L'z',"Invalid fixture key");
                require(tap(static_cast<UINT>(c-L'a'+L'A')),"Encoding key was not handled");
            }
        };
        auto candidates=[&] {
            require(ui->id!=TF_INVALID_UIELEMENTID,"Candidate element missing");
            ComPtr<ITfUIElement> element; check(elements->GetUIElement(ui->id,&element));
            ComPtr<ITfCandidateListUIElementBehavior> list; check(element.As(&list)); return list;
        };
        typeCode();
        require(doc.store->text==code && compositionCount(doc)==1,"Preedit fixture failed");
        auto list=candidates(); UINT count=0; check(list->GetCount(&count));
        require(count>=7,"At least seven real candidates are required");
        BSTR label=nullptr; check(list->GetString(5,&label));
        const std::wstring sixth(label,SysStringLen(label)); SysFreeString(label);
        require(sixth==expected,"Real dictionary/UI candidate mismatch");
        UINT customPages[]={0,2,5};
        check(list->SetPageIndex(customPages,3)); check(list->SetSelection(5));
        const UINT updates=ui->updates;
        for(unsigned i=0;i<12;++i) {
            doc.store->layoutReady=(i%3)!=0;
            check(layout->OnLayoutChange(doc.context.Get(),TF_LC_CHANGE,nullptr));
            drainLayout();
            UINT selected=0,page=0,pageCount=0,pages[3]{};
            check(list->GetSelection(&selected)); check(list->GetCurrentPage(&page));
            check(list->GetPageIndex(pages,3,&pageCount));
            require(selected==5 && page==2 && pageCount==3 &&
                std::equal(std::begin(pages),std::end(pages),std::begin(customPages)),
                "Layout notification reset the host's selection or page table");
            require(ui->updates==updates,"Layout-only notification republished the candidate model");
            require(doc.store->text==code,"Layout notification changed document text");
        }
        doc.store->layoutReady=true;
        check(list->Finalize()); drainLayout();
        require(doc.store->text==expected && compositionCount(doc)==0,"Finalization committed the wrong candidate");
        require(ui->id==TF_INVALID_UIELEMENTID,"Finalization retained a candidate element");
        require(list->Finalize()==TF_E_DISCONNECTED,"Detached candidate accepted a late finalization");
        typeCode(); list=candidates();
        check(list->SetPageIndex(customPages,3)); check(list->SetSelection(5));
        require(tap(VK_OEM_PLUS),"Page-down key was not handled");
        list=candidates();
        UINT selected=0,page=0,pageCount=0;
        check(list->GetSelection(&selected)); check(list->GetCurrentPage(&page));
        check(list->GetPageIndex(nullptr,0,&pageCount));
        require(selected==5 && page==1 && pageCount==(count+4)/5,"Content update did not restore engine paging");
        check(list->Abort()); drainLayout();
        require(doc.store->text==expected && compositionCount(doc)==0,"Abort changed committed text");
        require(!tap(VK_RETURN) && doc.store->text==expected,"Idle Enter did not remain pass-through");
        verifyModule(module);
        check(service->Deactivate());
        check(doc.manager->Pop(TF_POPF_ALL));
        check(source->UnadviseSink(uiCookie)); check(manager->Deactivate());
        DestroyWindow(window);
        std::cout<<"{\"status\":\"passed\",\"layout_changes\":12,\"real_tsf_edit_sessions\":true,"
            "\"custom_paging_preserved\":true,\"sixth_candidate_committed\":true,"
            "\"content_update\":true,\"abort\":true,\"late_finalize\":true,"
            "\"physical_input_tested\":false,\"profile_registered\":false,\"model_required\":false}\n";
        // COM objects are released at process teardown; do not force-unload the DLL.
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
