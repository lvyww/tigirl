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
        check(manager->SetFocus(doc.manager.Get()));
        check(service->ActivateEx(adapter.Get(),appClient,TF_TMAE_UIELEMENTENABLEDONLY));
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
            // Key-up can advance Context::revision without changing candidates.
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
        // Settle the initial 250 ms data-reconciliation tick while idle. It is
        // an independent content refresh, not one of the layout events tested.
        drainLayout(400);
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
            DWORD flags=0;check(list->GetUpdatedFlags(&flags));
            require(flags==0,"Layout left spurious model change flags");
        }
        // Duplicate document/foreground notifications must not rebuild the
        // UI element or discard the host's custom selection/paging.
        const auto elementId=ui->id;
        ComPtr<ITfThreadFocusSink> threadFocus;check(service.As(&threadFocus));
        for(unsigned i=0;i<3;++i) {
            check(focus->OnSetFocus(doc.manager.Get(),doc.manager.Get()));
            check(keys->OnSetFocus(TRUE));check(threadFocus->OnSetThreadFocus());drainLayout();
            UINT selected=0,page=0;check(list->GetSelection(&selected));check(list->GetCurrentPage(&page));
            require(ui->id==elementId && selected==5 && page==2,"Duplicate focus rebuilt UI or reset host state");
            require(ui->updates==updates,"Duplicate focus republished the candidate model");
        }
        // Route a shell chord explicitly, without opening the real screenshot
        // tool or injecting a global key into the user's applications.
        BYTE shellKeys[256]{};shellKeys[VK_LWIN]=shellKeys[VK_SHIFT]=shellKeys[VK_LSHIFT]=0x80;
        require(SetKeyboardState(shellKeys)!=FALSE,"Cannot set test shell chord");
        BOOL eaten=TRUE;check(keys->OnTestKeyDown(doc.context.Get(),'S',1,&eaten));
        require(!eaten,"Win+Shift+S was consumed");
        check(keys->OnKeyDown(doc.context.Get(),'S',1,&eaten));
        BYTE noKeys[256]{};require(SetKeyboardState(noKeys)!=FALSE,"Cannot clear test shell chord");
        drainLayout();require(doc.store->text==code && compositionCount(doc)==1 && ui->id==elementId,
            "Win+Shift+S cleared composition or candidate element");
        doc.store->layoutReady=true;
        // Model a wrapped preedit whose insertion point has no rectangle while
        // its final character is visible. Exercise the real range/edit session,
        // native window and recovery, without registering a TIP or global keys.
        ComPtr<ITfUIElement> nativeElement;check(list.As(&nativeElement));
        check(nativeElement->Show(TRUE));
        doc.store->emptyCaretBounds=true;
        auto queries=doc.store->tailBoundsQueries;
        check(layout->OnLayoutChange(doc.context.Get(),TF_LC_CHANGE,nullptr));drainLayout(180);
        require(doc.store->tailBoundsQueries>queries && doc.store->lastExtentStart==static_cast<LONG>(code.size())-1 &&
            doc.store->lastExtentEnd==static_cast<LONG>(code.size()),"Wrapped caret did not query only the final character");
        require(ownCandidateWindow() && IsWindowVisible(ownCandidateWindow()),"Visible trailing character lost native candidates");
        doc.store->emptyCaretNoLayout=true;
        queries=doc.store->tailBoundsQueries;
        check(layout->OnLayoutChange(doc.context.Get(),TF_LC_CHANGE,nullptr));drainLayout(180);
        require(doc.store->tailBoundsQueries>queries && ownCandidateWindow() && IsWindowVisible(ownCandidateWindow()),
            "No-layout insertion point lost a visible trailing character");
        doc.store->emptyCaretNoLayout=false;
        doc.store->tailBoundsClipped=true;
        check(layout->OnLayoutChange(doc.context.Get(),TF_LC_CHANGE,nullptr));drainLayout(180);
        require(ownCandidateWindow() && IsWindowVisible(ownCandidateWindow()),"Clipped range hid the last valid candidate position");
        doc.store->tailBoundsClipped=false;
        doc.store->tailBoundsUnavailable=true;
        check(layout->OnLayoutChange(doc.context.Get(),TF_LC_CHANGE,nullptr));drainLayout(180);
        require(ownCandidateWindow() && IsWindowVisible(ownCandidateWindow()),"Missing character geometry hid active candidates");
        doc.store->emptyCaretBounds=false;doc.store->tailBoundsUnavailable=false;
        queries=doc.store->tailBoundsQueries;
        check(layout->OnLayoutChange(doc.context.Get(),TF_LC_CHANGE,nullptr));drainLayout(180);
        require(ownCandidateWindow() && IsWindowVisible(ownCandidateWindow()),"Recovered caret did not restore native candidates");
        require(doc.store->tailBoundsQueries==queries,"Valid caret unnecessarily used trailing-character fallback");
        const auto retainedWindow=ownCandidateWindow();
        for(unsigned i=0;i<3;++i) {
            check(keys->OnSetFocus(FALSE));check(threadFocus->OnKillThreadFocus());
            require(IsWindow(retainedWindow) && !IsWindowVisible(retainedWindow) && ui->id==elementId,
                "Temporary focus loss destroyed the HWND or UI element");
            require(list->Finalize()==TF_E_DISCONNECTED && list->Abort()==TF_E_DISCONNECTED,
                "Background candidate accepted commit or cancellation");
            doc.store->layoutReady=false;
            check(keys->OnSetFocus(TRUE));check(threadFocus->OnSetThreadFocus());drainLayout(150);
            UINT selected=0,page=0;check(list->GetSelection(&selected));check(list->GetCurrentPage(&page));
            require(ownCandidateWindow()==retainedWindow && IsWindowVisible(retainedWindow) && ui->id==elementId,
                "Focus recovery failed to reuse the HWND and cached geometry");
            require(selected==5 && page==2 && doc.store->text==code && compositionCount(doc)==1,
                "Focus recovery changed text or selection");
            doc.store->layoutReady=true;
        }
        // A host may change its visibility policy while this thread is hidden.
        check(threadFocus->OnKillThreadFocus());check(nativeElement->Show(FALSE));
        check(threadFocus->OnSetThreadFocus());drainLayout();
        BOOL shown=TRUE;check(nativeElement->IsShown(&shown));
        require(!shown && !IsWindowVisible(retainedWindow),"Recovery overrode host Show(FALSE)");
        check(nativeElement->Show(TRUE));check(layout->OnLayoutChange(doc.context.Get(),TF_LC_CHANGE,nullptr));drainLayout();
        // A deferred click belongs to the focus generation in which it was made.
        doc.store->deferLocks=true;check(list->Finalize());
        require(doc.store->pendingLock!=0,"Finalize did not defer");
        check(threadFocus->OnKillThreadFocus());
        doc.store->deferLocks=false;check(doc.store->grantPendingLock());drainLayout();
        require(doc.store->text==code && compositionCount(doc)==1,"Late unfocused finalize changed document text");
        check(threadFocus->OnSetThreadFocus());drainLayout();
        check(nativeElement->Show(FALSE));
        check(list->Finalize()); drainLayout();
        require(doc.store->text==expected && compositionCount(doc)==0,"Finalization committed the wrong candidate");
        require(ui->id==TF_INVALID_UIELEMENTID,"Finalization retained a candidate element");
        require(list->Finalize()==TF_E_DISCONNECTED,"Detached candidate accepted a late finalization");
        check(layout->OnLayoutChange(doc.context.Get(),TF_LC_CHANGE,nullptr));drainLayout();
        require(ui->id==TF_INVALID_UIELEMENTID,"Late layout resurrected a finalized UI");
        typeCode(); list=candidates();
        check(list->SetPageIndex(customPages,3)); check(list->SetSelection(5));
        const auto beforeContent=ui->updates;
        require(tap(VK_OEM_PLUS),"Page-down key was not handled");
        list=candidates();
        UINT selected=0,page=0,pageCount=0;
        check(list->GetSelection(&selected)); check(list->GetCurrentPage(&page));
        check(list->GetPageIndex(nullptr,0,&pageCount));
        require(selected==5 && page==1 && pageCount==(count+4)/5,"Content update did not restore engine paging");
        require(ui->updates>beforeContent,"Content update did not notify the host");
        check(list->Abort()); drainLayout();
        require(doc.store->text==expected && compositionCount(doc)==0,"Abort changed committed text");
        require(!tap(VK_RETURN) && doc.store->text==expected,"Idle Enter did not remain pass-through");
        check(layout->OnLayoutChange(doc.context.Get(),TF_LC_CHANGE,nullptr));drainLayout();
        require(ui->id==TF_INVALID_UIELEMENTID,"Late layout resurrected an aborted UI");
        // Queue an old context's layout read, then create candidates in a new
        // context. Granting that old read must not hide the new candidate UI.
        typeCode();doc.store->deferLocks=true;
        check(layout->OnLayoutChange(doc.context.Get(),TF_LC_CHANGE,nullptr));
        require(doc.store->pendingLock!=0,"Old layout did not defer");
        auto other=document(manager.Get(),appClient,window);
        check(manager->SetFocus(other.manager.Get()));check(focus->OnSetFocus(other.manager.Get(),doc.manager.Get()));
        for(auto c:code) {
            BOOL handled=FALSE;const UINT vk=static_cast<UINT>(c-L'a'+L'A');
            check(keys->OnTestKeyDown(other.context.Get(),vk,1,&handled));require(handled!=FALSE,"Other context key not handled");
            check(keys->OnKeyDown(other.context.Get(),vk,1,&handled));
            check(keys->OnTestKeyUp(other.context.Get(),vk,0xc0000001,&handled));
            if(handled)check(keys->OnKeyUp(other.context.Get(),vk,0xc0000001,&handled));
        }
        drainLayout();const auto otherId=ui->id;auto otherList=candidates();
        require(otherId!=TF_INVALID_UIELEMENTID && other.store->text==code,"Other context candidates missing");
        doc.store->deferLocks=false;check(doc.store->grantPendingLock());drainLayout();
        require(ui->id==otherId && other.store->text==code,"Old layout callback hid or changed the new context");
        check(otherList->Abort());drainLayout();check(other.manager->Pop(TF_POPF_ALL));
        verifyModule(module);
        check(service->Deactivate());
        check(doc.manager->Pop(TF_POPF_ALL));
        check(source->UnadviseSink(uiCookie)); check(manager->Deactivate());
        DestroyWindow(window);
        std::cout<<"{\"status\":\"passed\",\"layout_changes\":12,\"real_tsf_edit_sessions\":true,"
            "\"wrapped_caret_recovery\":true,\"custom_paging_preserved\":true,\"sixth_candidate_committed\":true,"
            "\"content_update\":true,\"abort\":true,\"late_finalize\":true,"
            "\"focus_hwnd_retained\":true,\"shell_shortcut_preserved\":true,\"stale_layout_isolated\":true,"
            "\"keystroke_subscription_mocked\":true,\"physical_input_tested\":false,"
            "\"profile_registered\":false,\"model_required\":false}\n";
        // Do not force-unload a DLL while COM objects are still in scope.
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
