// Included after the TSF host's pump/capture helpers. Only this process's popup
// receives synthesized input, and only its own child windows are closed.
namespace management_fixture {
struct MenuInput {
    UINT selection=0;
    ULONGLONG started=0;
    bool sent=false,failed=false;
};
inline MenuInput* pending=nullptr;
inline bool addWordSeen=false;
inline void CALLBACK closeAddWord(HWND,UINT,UINT_PTR timer,DWORD) {
    HWND dialog=nullptr;
    while((dialog=FindWindowExW(nullptr,dialog,L"#32770",L"虎娘加词"))) {
        DWORD process=0;GetWindowThreadProcessId(dialog,&process);
        if(process!=GetCurrentProcessId())continue;
        addWordSeen=true;PostMessageW(dialog,WM_COMMAND,IDCANCEL,0);KillTimer(nullptr,timer);return;
    }
}
inline void CALLBACK selectMenu(HWND,UINT,UINT_PTR timer,DWORD) {
    auto& state=*pending;
    DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    HWND menu=nullptr;
    while((menu=FindWindowExW(nullptr,menu,L"#32768",nullptr))) {
        DWORD owner=0;GetWindowThreadProcessId(menu,&owner);
        if(owner!=GetCurrentProcessId() || !IsWindowVisible(menu))continue;
        if(foreground!=owner)break;
        INPUT input[24]{};UINT count=0;
        for(UINT i=0;i<state.selection;++i) {
            input[count].type=INPUT_KEYBOARD;input[count++].ki.wVk=VK_DOWN;
            input[count].type=INPUT_KEYBOARD;input[count].ki.wVk=VK_DOWN;input[count++].ki.dwFlags=KEYEVENTF_KEYUP;
        }
        input[count].type=INPUT_KEYBOARD;input[count++].ki.wVk=VK_RETURN;
        input[count].type=INPUT_KEYBOARD;input[count].ki.wVk=VK_RETURN;input[count++].ki.dwFlags=KEYEVENTF_KEYUP;
        state.sent=SendInput(count,input,sizeof(INPUT))==count;state.failed=!state.sent;
        KillTimer(nullptr,timer);if(state.failed)EndMenu();return;
    }
    if(GetTickCount64()-state.started>5000){state.failed=true;KillTimer(nullptr,timer);EndMenu();}
}
inline std::vector<DWORD> children() {
    std::vector<DWORD> result;HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    require(snapshot!=INVALID_HANDLE_VALUE,"Cannot inspect manager child processes");
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(Process32FirstW(snapshot,&entry))do {
        if(entry.th32ParentProcessID==GetCurrentProcessId() && !_wcsicmp(entry.szExeFile,L"Tigirl.exe"))result.push_back(entry.th32ProcessID);
    }while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);return result;
}
inline HWND childWindow(DWORD process,const wchar_t* type) {
    HWND window=nullptr;
    while((window=FindWindowExW(nullptr,window,type,nullptr))) {
        DWORD owner=0;GetWindowThreadProcessId(window,&owner);
        if(owner==process && IsWindowVisible(window))return window;
    }
    return nullptr;
}
inline void run(ITfThreadMgr* manager,HWND host,const std::filesystem::path& output,bool popup) {
    ComPtr<ITfLangBarItemMgr> bars;check(manager->QueryInterface(IID_PPV_ARGS(&bars)));
    ComPtr<ITfLangBarItem> item;check(bars->GetItem(tiger::tsf::LanguageBar::ItemId,&item));
    ComPtr<ITfLangBarItemButton> button;check(item.As(&button));
    ComPtr<MenuCapture> captured;captured.Attach(new MenuCapture);check(button->InitMenu(captured.Get()));
    require(captured->items.size()==12 && captured->children.count(10) && captured->children.count(11),"Missing Tiger menu groups");
    require(captured->children[11]->items.size()==9,"Missing themes");
    unsigned checkmarks=0;for(const auto& entry:captured->children[11]->flags)if(entry.second&TF_LBMENUF_CHECKED)++checkmarks;
    require(checkmarks==1,"Theme selection has no unique checkmark");
    for(UINT selection:{1u,2u}) {
        require(children().empty(),"Unexpected preexisting manager child");
        HRESULT result=S_FALSE;
        if(popup) {
        // The WSL-launched controller is not necessarily foreground-eligible.
        // Arrange the fixture's initial focus explicitly; restoration after the
        // child closes is checked without this assistance.
        const auto foregroundThread=GetWindowThreadProcessId(GetForegroundWindow(),nullptr);
        const auto thread=GetCurrentThreadId();
        const bool attached=foregroundThread!=thread && AttachThreadInput(thread,foregroundThread,TRUE)!=FALSE;
        BringWindowToTop(host);SetForegroundWindow(host);SetFocus(host);
        if(attached)AttachThreadInput(thread,foregroundThread,FALSE);
        pump();
        require(GetForegroundWindow()==host,"Management test host lacks foreground");
        MenuInput input{selection==1?8u:9u,GetTickCount64()};pending=&input;
        const auto timer=SetTimer(nullptr,0,100,selectMenu);require(timer!=0,"Cannot schedule popup input");
        RECT area{110,110,140,140};result=button->OnClick(TF_LBI_CLK_RIGHT,{140,140},&area);
        KillTimer(nullptr,timer);pending=nullptr;
        check(result);require(result==S_OK && input.sent && !input.failed,"Popup input failed to select management action");
        } else {result=button->OnMenuSelect(selection);check(result);require(result==S_OK,"Management action refused launch");}
        const auto processes=children();require(processes.size()==1,"Menu did not launch exactly one manager child");
        OwnedReminder child{OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,processes[0])};
        require(child.process!=nullptr,"Cannot retain manager process handle");
        HWND window=nullptr;const auto start=GetTickCount64();
        while(!(window=childWindow(processes[0],selection==1?L"NativeTigerSchemaManager":L"NativeTigerInputSettings")) && GetTickCount64()-start<10000) {
            pump();Sleep(10);require(WaitForSingleObject(child.process,0)==WAIT_TIMEOUT,"Manager exited before showing its window");
        }
        require(window!=nullptr,"Management window was not displayed");
        for(int i=0;i<30;++i){pump();Sleep(10);}
        if(popup) {
            require(GetForegroundWindow()==window,"Management window did not receive foreground focus");
            capture(window,(output/(selection==1?L"management-menu-schema.bmp":L"management-menu-settings.bmp")).c_str());
        }
        require(PostMessageW(window,WM_CLOSE,0,0)!=FALSE,"Cannot close fixture manager");
        const auto closeStart=GetTickCount64();
        while(WaitForSingleObject(child.process,0)==WAIT_TIMEOUT && GetTickCount64()-closeStart<10000){pump();Sleep(10);}
        require(WaitForSingleObject(child.process,0)==WAIT_OBJECT_0,"Manager did not exit after close");
        DWORD code=1;require(GetExitCodeProcess(child.process,&code)!=FALSE && code==0,"Manager exited with failure");
        pump();if(popup)require(GetForegroundWindow()==host,"Closing management did not restore host foreground");
    }
    addWordSeen=false;const auto timer=SetTimer(nullptr,0,100,closeAddWord);require(timer!=0,"Cannot schedule add-word close");
    check(button->OnMenuSelect(5));pump();KillTimer(nullptr,timer);
    require(addWordSeen,"Menu add-word dialog did not open");
    check(button->OnMenuSelect(8));pump();
    ComPtr<ITfCompartmentMgr> modes;check(manager->QueryInterface(IID_PPV_ARGS(&modes)));
    ComPtr<ITfCompartment> open;check(modes->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,&open));
    VARIANT value;VariantInit(&value);check(open->GetValue(&value));
    require(value.vt==VT_I4 && value.lVal==0,"English menu action did not update system mode");VariantClear(&value);
}
}
