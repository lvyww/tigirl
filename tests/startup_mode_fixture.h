// Isolated startup observation: no registration, no input injection, no mode fixup.
#pragma once
namespace startup_mode_fixture {
struct Value {
    HRESULT hr=E_FAIL;VARTYPE vt=VT_EMPTY;LONG number=0;
};
static Value read(ITfCompartment* compartment) {
    VARIANT raw;VariantInit(&raw);Value result;result.hr=compartment->GetValue(&raw);
    result.vt=raw.vt;if(raw.vt==VT_I4)result.number=raw.lVal;VariantClear(&raw);return result;
}
struct Row {
    std::string phase,event;ULONGLONG ms=0;Value open,conversion;bool foreground=false;
    std::vector<std::string> stack;
};
class Trace final:public ITfCompartmentEventSink, public ITfActiveLanguageProfileNotifySink {
    LONG refs_=1;ULONGLONG started_=GetTickCount64();
public:
    ComPtr<ITfCompartment> open,conversion;HWND window=nullptr;
    std::string phase="setup";std::vector<Row> rows;
    STDMETHODIMP QueryInterface(REFIID iid,void** result) override {
        if(!result)return E_POINTER;*result=nullptr;
        if(iid==IID_IUnknown || iid==IID_ITfCompartmentEventSink)*result=static_cast<ITfCompartmentEventSink*>(this);
        else if(iid==IID_ITfActiveLanguageProfileNotifySink)*result=static_cast<ITfActiveLanguageProfileNotifySink*>(this);
        else return E_NOINTERFACE;AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override{return ++refs_;}
    STDMETHODIMP_(ULONG) Release() override{const auto left=--refs_;if(!left)delete this;return left;}
    void record(const char* event,bool stack=false) {
        Row row;row.phase=phase;row.event=event;row.ms=GetTickCount64()-started_;
        row.open=read(open.Get());row.conversion=read(conversion.Get());
        row.foreground=window && GetForegroundWindow()==window;
        if(stack) {
            void* frames[24]{};const auto count=CaptureStackBackTrace(1,24,frames,nullptr);
            for(USHORT i=0;i<count;++i) {
                HMODULE module=nullptr;wchar_t path[MAX_PATH]{};
                if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(frames[i]),&module) && GetModuleFileNameW(module,path,MAX_PATH)) {
                    std::ostringstream value;value<<std::filesystem::path(path).filename().string()<<"+0x"<<std::hex
                        <<(reinterpret_cast<std::uintptr_t>(frames[i])-reinterpret_cast<std::uintptr_t>(module));
                    row.stack.push_back(value.str());
                }
            }
        }
        rows.push_back(std::move(row));
    }
    STDMETHODIMP OnChange(REFGUID guid) override {
        try{record(guid==GUID_COMPARTMENT_KEYBOARD_OPENCLOSE?"open-change":"conversion-change",true);}
        catch(...){return E_FAIL;}return S_OK;
    }
    STDMETHODIMP OnActivated(REFCLSID clsid,REFGUID,BOOL activated) override {
        CLSID target{};CLSIDFromString(L"{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}",&target);
        try{record(clsid==target?(activated?"target-profile-activated":"target-profile-deactivated"):(activated?"other-profile-activated":"other-profile-deactivated"),true);}
        catch(...){return E_FAIL;}return S_OK;
    }
    void print(bool foregroundRequested,bool keyEaten) const {
        auto value=[](const Value& item){std::cout<<"{\"hr\":"<<item.hr<<",\"vt\":"<<item.vt<<",\"value\":"<<item.number<<"}";};
        std::cout<<"{\"status\":\"observed\",\"private_activation\":true,\"registered\":false,\"foreground_requested\":"
            <<(foregroundRequested?"true":"false")<<",\"first_letter_preview_eaten\":"<<(keyEaten?"true":"false")<<",\"events\":[";
        bool first=true;for(const auto& row:rows) {
            if(!first)std::cout<<",";first=false;
            std::cout<<"{\"phase\":\""<<row.phase<<"\",\"event\":\""<<row.event<<"\",\"ms\":"<<row.ms
                <<",\"foreground\":"<<(row.foreground?"true":"false")<<",\"open\":";value(row.open);
            std::cout<<",\"conversion\":";value(row.conversion);std::cout<<",\"stack\":[";
            for(std::size_t i=0;i<row.stack.size();++i){if(i)std::cout<<",";std::cout<<"\""<<row.stack[i]<<"\"";}
            std::cout<<"]}";
        }std::cout<<"]}\n";
    }
};
static int run(int argc,wchar_t** argv) {
    require(argc==8,"--startup-mode-probe <DLL> <manifest> <isolated-root> <hidden|foreground> <seed-zero|unseeded> <natural|neutral|activate-before-foreground>");
    const std::filesystem::path root=argv[4];
    require(root.is_absolute() && std::filesystem::exists(root/L".tsf-startup-test"),"Startup probe requires marked isolated root");
    require(SetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",root.c_str())!=FALSE,"Cannot isolate startup probe");
    const bool foreground=wcscmp(argv[5],L"foreground")==0;
    require(foreground || wcscmp(argv[5],L"hidden")==0,"Unknown visibility");
    const bool seed=wcscmp(argv[6],L"seed-zero")==0;
    require(seed || wcscmp(argv[6],L"unseeded")==0,"Unknown seed");
    const bool early=wcscmp(argv[7],L"activate-before-foreground")==0;
    const bool neutral=wcscmp(argv[7],L"neutral")==0;
    require(neutral || early || wcscmp(argv[7],L"natural")==0,"Unknown activation order");
    require(!early || !seed,"Early activation fixture must not seed after activation");
    CLSID clsid;GUID profile;check(CLSIDFromString(L"{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}",&clsid));
    check(CLSIDFromString(L"{43201C7B-F615-469D-9D54-906D9270975E}",&profile));
    LocalActivation activation(argv[3]);check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
    auto module=LoadLibraryW(argv[2]);require(module!=nullptr,"Cannot load startup DLL");
    ComPtr<ITfThreadMgrEx> manager;check(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager)));
    ComPtr<ITfInputProcessorProfileMgr> profiles;check(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&profiles)));
    TfClientId client;check(manager->ActivateEx(&client,TF_TMAE_NOACTIVATEKEYBOARDLAYOUT));
    ComPtr<ITfCompartmentMgr> compartments;check(manager.As(&compartments));
    ComPtr<Trace> trace;trace.Attach(new Trace);
    check(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,&trace->open));
    check(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,&trace->conversion));
    ComPtr<ITfSource> openSource,conversionSource;check(trace->open.As(&openSource));check(trace->conversion.As(&conversionSource));
    DWORD openCookie=TF_INVALID_COOKIE,conversionCookie=TF_INVALID_COOKIE;
    check(openSource->AdviseSink(IID_ITfCompartmentEventSink,static_cast<ITfCompartmentEventSink*>(trace.Get()),&openCookie));
    check(conversionSource->AdviseSink(IID_ITfCompartmentEventSink,static_cast<ITfCompartmentEventSink*>(trace.Get()),&conversionCookie));
    ComPtr<ITfSource> managerSource;check(manager.As(&managerSource));DWORD profileCookie=TF_INVALID_COOKIE;
    check(managerSource->AdviseSink(IID_ITfActiveLanguageProfileNotifySink,static_cast<ITfActiveLanguageProfileNotifySink*>(trace.Get()),&profileCookie));
    if(neutral) {
        HKL layouts[64]{};const int count=GetKeyboardLayoutList(64,layouts);HKL plain=nullptr;
        for(int i=0;i<count;++i)if(LOWORD(reinterpret_cast<ULONG_PTR>(layouts[i]))==0x0409){plain=layouts[i];break;}
        require(plain!=nullptr,"Startup control requires loaded English layout");trace->phase="neutral-profile";
        check(profiles->ActivateProfile(TF_PROFILETYPE_KEYBOARDLAYOUT,0x0409,CLSID_NULL,GUID_NULL,plain,TF_IPPMF_FORPROCESS|TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE));pump();trace->record("sample");
    }
    HWND previous=GetForegroundWindow();
    HWND window=CreateWindowExW(0,L"STATIC",L"Tigirl isolated startup mode probe",WS_OVERLAPPEDWINDOW,100,100,600,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    require(window!=nullptr,"Cannot create startup window");trace->window=window;
    auto doc=document(manager.Get(),client,window);ComPtr<ITfDocumentMgr> previousDoc;
    check(manager->AssociateFocus(window,doc.manager.Get(),&previousDoc));
    if(early) {
        check(manager->SetFocus(doc.manager.Get()));trace->phase="activate-before-foreground";
        check(profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,clsid,profile,nullptr,TF_IPPMF_FORPROCESS|TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE));pump();trace->record("returned");
    }
    trace->phase="before-window-focus";trace->record("sample");
    if(foreground){ShowWindow(window,SW_SHOWNORMAL);SetForegroundWindow(window);SetFocus(window);}
    check(manager->SetFocus(doc.manager.Get()));pump();
    trace->phase="before-profile";trace->record("sample");
    if(seed) {
        trace->phase="seed-english";VARIANT zero;VariantInit(&zero);zero.vt=VT_I4;zero.lVal=0;
        check(trace->open->SetValue(client,&zero));check(trace->conversion->SetValue(client,&zero));pump();
        trace->record("seed-complete");
    }
    trace->phase="ActivateProfile";
    check(profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,clsid,profile,nullptr,TF_IPPMF_FORPROCESS|TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE));
    trace->record("returned");verifyModule(module);
    auto settle=[&](const char* phase,ULONGLONG duration) {
        trace->phase=phase;const auto until=GetTickCount64()+duration;do{pump();Sleep(10);}while(GetTickCount64()<until);trace->record("sample");
    };
    settle("after-activation-pump",300);
    TF_INPUTPROCESSORPROFILE active{};check(profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&active));
    require(active.clsid==clsid,"Startup profile mismatch");
    trace->phase="focus-out";check(manager->SetFocus(nullptr));pump();trace->record("sample");
    trace->phase="focus-back";check(manager->SetFocus(doc.manager.Get()));pump();trace->record("sample");
    settle("after-focus-pump",400);
    BOOL eaten=FALSE;ComPtr<ITfKeystrokeMgr> keys;check(manager.As(&keys));
    trace->phase="first-letter-preview";
    check(keys->TestKeyDown('A',1|(static_cast<LPARAM>(MapVirtualKeyW('A',MAPVK_VK_TO_VSC))<<16),&eaten));trace->record("sample");
    trace->print(foreground,eaten!=FALSE);
    check(managerSource->UnadviseSink(profileCookie));
    check(openSource->UnadviseSink(openCookie));check(conversionSource->UnadviseSink(conversionCookie));
    check(profiles->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,clsid,profile,nullptr,TF_IPPMF_FORPROCESS));pump();
    check(doc.manager->Pop(TF_POPF_ALL));check(manager->Deactivate());DestroyWindow(window);
    if(foreground && previous && IsWindow(previous))SetForegroundWindow(previous);
    return 0;
}
}
