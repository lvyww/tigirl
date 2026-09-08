namespace word_save_fixture {
inline void run(ITfTextInputProcessorEx* service,ITfThreadMgr* manager,TfClientId client,Document& doc,UISink* ui,const std::filesystem::path& root) {
    ComPtr<ITfThreadMgrEventSink> focus;check(service->QueryInterface(IID_PPV_ARGS(&focus)));
    check(focus->OnSetFocus(doc.manager.Get(),nullptr));
    ComPtr<ITfKeyEventSink> keys;check(service->QueryInterface(IID_PPV_ARGS(&keys)));
    struct Keyboard {BYTE saved[256];Keyboard(){require(GetKeyboardState(saved)!=FALSE,"Cannot save keyboard state");}~Keyboard(){SetKeyboardState(saved);}} keyboard;
    BYTE empty[256]{},ctrl[256]{};ctrl[VK_CONTROL]=ctrl[VK_LCONTROL]=0x80;
    SetKeyboardState(empty);
    auto tap=[&](WPARAM vk) {
        BOOL eaten=FALSE;check(keys->OnTestKeyDown(doc.context.Get(),vk,1,&eaten));require(eaten!=FALSE,"Word-save preview did not consume key");
        check(keys->OnKeyDown(doc.context.Get(),vk,1,&eaten));require(eaten!=FALSE,"Word-save dispatch did not consume key");
        check(keys->OnTestKeyUp(doc.context.Get(),vk,1,&eaten));if(eaten)check(keys->OnKeyUp(doc.context.Get(),vk,1,&eaten));pump();
    };
    ComPtr<ITfLangBarItemMgr> bars;check(manager->QueryInterface(IID_PPV_ARGS(&bars)));
    ComPtr<ITfLangBarItem> item;check(bars->GetItem(tiger::tsf::LanguageBar::ItemId,&item));
    ComPtr<ITfLangBarItemButton> button;check(item.As(&button));
    auto warning=[&] {
        BSTR text=nullptr;check(item->GetTooltipString(&text));const std::wstring value(text,SysStringLen(text));SysFreeString(text);
        return value.find(L"上次词条调整保存失败")!=std::wstring::npos;
    };
    auto firstCandidate=[&] {
        ComPtr<ITfUIElementMgr> elements;check(manager->QueryInterface(IID_PPV_ARGS(&elements)));
        ComPtr<ITfUIElement> element;check(elements->GetUIElement(ui->id,&element));
        ComPtr<ITfCandidateListUIElement> list;check(element.As(&list));
        BSTR text=nullptr;check(list->GetString(0,&text));const std::wstring value(text,SysStringLen(text));SysFreeString(text);return value;
    };
    require(!warning(),"Fresh service has stale word-save warning");
    tap('A');tap('B');require(firstCandidate()==L"交","Unexpected original word order");
    struct File {HANDLE value=INVALID_HANDLE_VALUE;~File(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}} blocked;
    blocked.value=CreateFileW((root/L"user"/L"tiger-words.tcu").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(blocked.value!=INVALID_HANDLE_VALUE,"Cannot deny fixture journal writes");
    SetKeyboardState(ctrl);
    BOOL eaten=FALSE;check(keys->OnTestKeyDown(doc.context.Get(),'2',1,&eaten));
    require(eaten && !warning(),"Preview published a word-save warning");
    check(keys->OnKeyDown(doc.context.Get(),'2',1,&eaten));require(eaten!=FALSE,"Failed adjustment was not handled");
    check(keys->OnTestKeyUp(doc.context.Get(),'2',1,&eaten));if(eaten)check(keys->OnKeyUp(doc.context.Get(),'2',1,&eaten));
    SetKeyboardState(empty);pump();
    require(warning() && firstCandidate()==L"交","Failure warning or persisted candidate order missing");
    BSTR label=nullptr;check(button->GetText(&label));require(std::wstring(label,SysStringLen(label))==L"中!","Failure marker missing");SysFreeString(label);
    HICON icon=nullptr;check(button->GetIcon(&icon));require(icon!=nullptr,"Warning icon missing");DestroyIcon(icon);
    CloseHandle(blocked.value);blocked.value=INVALID_HANDLE_VALUE;
    tap(VK_SPACE);require(doc.store->text==L"交" && warning(),"Ordinary commit cleared failure or changed persisted text");
    const auto journal=root/L"user"/L"tiger-words.tcu";
    {const bool emptyJournal=std::filesystem::file_size(journal)==0;
        std::ofstream interrupted(journal,std::ios::binary|std::ios::app);
        if(emptyJournal)interrupted.write("TIGERU01",8);
        interrupted.write("\x0c\0\0",3);require(interrupted.good(),"Cannot inject interrupted journal tail");}
    tap('A');tap('B');SetKeyboardState(ctrl);tap('2');SetKeyboardState(empty);
    require(!warning() && firstCandidate()==L"疒","Explicit successful retry did not clear warning and persist order");
    tap(VK_SPACE);require(doc.store->text==L"交疒","Successful retry committed wrong candidate");
    check(service->Deactivate());
    {std::ofstream config(root/L"config.txt",std::ios::binary);config<<"隐藏候选\t是\n";require(config.good(),"Cannot set private hidden-candidate fixture");}
    ui->nativeUI=true;
    auto nativeWindow=[] {
        HWND window=nullptr;
        while((window=FindWindowExW(nullptr,window,L"NativeTiger.Candidate.v1",nullptr))) {
            DWORD process=0;GetWindowThreadProcessId(window,&process);
            if(process==GetCurrentProcessId() && IsWindowVisible(window))return window;
        }
        return static_cast<HWND>(nullptr);
    };
    check(service->ActivateEx(manager,client,0));check(focus->OnSetFocus(doc.manager.Get(),nullptr));
    tap('A');tap('B');require(firstCandidate()==L"疒" && !nativeWindow(),"Normal reactivation did not load hidden style and user order");
    tap(VK_ESCAPE);check(service->Deactivate());
    check(service->ActivateEx(manager,client,TF_TMAE_SECUREMODE));check(focus->OnSetFocus(doc.manager.Get(),nullptr));
    check(bars->GetItem(tiger::tsf::LanguageBar::ItemId,item.ReleaseAndGetAddressOf()));check(item.As(&button));
    tap('A');tap('B');require(firstCandidate()==L"交","Secure reactivation retained the user-word overlay");
    require(nativeWindow()!=nullptr,"Secure reactivation retained private hidden-candidate style");
    SetKeyboardState(ctrl);tap('2');SetKeyboardState(empty);
    require(!warning() && firstCandidate()==L"交","Secure adjustment published a warning or retained an unpersisted order");
    tap(VK_ESCAPE);
}
}
