#define NOMINMAX
#include <windows.h>
#include <msctf.h>
#include <richedit.h>
#include <wrl/client.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
static void require(bool value,const char* error){if(!value)throw std::runtime_error(error);}
static void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("HRESULT "+std::to_string(static_cast<unsigned long>(hr)));}
struct KeyObservation { UINT message; WPARAM key; HRESULT test; BOOL tested; HRESULT dispatch; BOOL eaten; };
static KeyObservation observations[128]{};
static unsigned observationCount=0;
static unsigned injected=0;
static void pump(ITfKeystrokeMgr* keys) {
    MSG message;
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
        if(message.message==WM_KEYDOWN || message.message==WM_KEYUP) {
            KeyObservation event{};event.message=message.message;event.key=message.wParam;
            const bool down=message.message==WM_KEYDOWN;
            event.test=down?keys->TestKeyDown(message.wParam,message.lParam,&event.tested):keys->TestKeyUp(message.wParam,message.lParam,&event.tested);
            event.dispatch=S_FALSE;
            if(event.test==S_OK && event.tested)
                event.dispatch=down?keys->KeyDown(message.wParam,message.lParam,&event.eaten):keys->KeyUp(message.wParam,message.lParam,&event.eaten);
            observations[observationCount++%128]=event;
            if(event.dispatch==S_OK && event.eaten)continue;
        }
        TranslateMessage(&message);DispatchMessageW(&message);
    }
}
static std::wstring text(HWND window) {
    std::wstring value(static_cast<std::size_t>(GetWindowTextLengthW(window))+1,L'\0');
    value.resize(GetWindowTextW(window,value.data(),static_cast<int>(value.size())));return value;
}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==2,"rich_edit_probe <installed DLL>");
        wchar_t root[32768]{};
        require(GetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",root,32768)>0 &&
            std::filesystem::exists(std::filesystem::path(root)/L".rich-edit-test"),"Requires marked isolated user root");
        check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
        ComPtr<ITfThreadMgrEx> manager;
        check(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager)));
        TfClientId client;check(manager->ActivateEx(&client,TF_TMAE_NOACTIVATEKEYBOARDLAYOUT));
        ComPtr<ITfKeystrokeMgr> keys;check(manager.As(&keys));
        require(LoadLibraryW(L"msftedit.dll")!=nullptr,"Cannot load system Rich Edit");
        HWND parent=CreateWindowExW(0,L"STATIC",L"NativeTiger Rich Edit acceptance",WS_OVERLAPPEDWINDOW,
            100,100,700,420,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        require(parent!=nullptr,"Cannot create test window");
        HWND edits[2]{};
        for(int i=0;i<2;++i) {
            edits[i]=CreateWindowExW(WS_EX_CLIENTEDGE,MSFTEDIT_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL,
                20,30+i*150,640,120,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
            require(edits[i]!=nullptr,"Cannot create Rich Edit control");
            require((SendMessageW(edits[i],EM_SETEDITSTYLE,SES_USECTF,SES_USECTF)&SES_USECTF)!=0,"Rich Edit TSF enablement failed");
        }
        ShowWindow(parent,SW_SHOW);SetForegroundWindow(parent);SetFocus(edits[0]);pump(keys.Get());
        ComPtr<ITfInputProcessorProfileMgr> profiles;
        check(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&profiles)));
        CLSID clsid;GUID profile;
        check(CLSIDFromString(L"{D2291A80-84D8-4641-9AB2-BDD1472C846B}",&clsid));
        check(CLSIDFromString(L"{83955C0E-2C09-47A5-BCF3-F2B98E11EE8B}",&profile));
        check(profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0804,clsid,profile,nullptr,TF_IPPMF_FORPROCESS));
        ComPtr<ITfCompartmentMgr> compartments;check(manager.As(&compartments));
        ComPtr<ITfCompartment> open;check(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,&open));
        VARIANT chinese;VariantInit(&chinese);chinese.vt=VT_I4;chinese.lVal=1;check(open->SetValue(client,&chinese));
        pump(keys.Get());
        auto waitText=[&](HWND edit,const std::wstring& expected) {
            const auto deadline=GetTickCount64()+2000;
            do {pump(keys.Get());if(text(edit)==expected)return;MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);}while(GetTickCount64()<deadline);
            std::string actual;for(auto ch:text(edit))actual+=std::to_string(static_cast<unsigned>(ch))+",";
            std::string wanted;for(auto ch:expected)wanted+=std::to_string(static_cast<unsigned>(ch))+",";
            throw std::runtime_error("Rich Edit text mismatch after tap="+std::to_string(injected)+", expected UTF16="+wanted+", actual UTF16="+actual);
        };

        auto tap=[&](HWND edit,WORD vk) {
            require(GetForegroundWindow()==parent && GetFocus()==edit,"Rich Edit lost input focus");
            for(int modifier:{VK_SHIFT,VK_CONTROL,VK_MENU,VK_LWIN,VK_RWIN})
                require((GetAsyncKeyState(modifier)&0x8000)==0,"External modifier held during Rich Edit test");
            INPUT input[2]{};input[0].type=input[1].type=INPUT_KEYBOARD;input[0].ki.wVk=input[1].ki.wVk=vk;input[1].ki.dwFlags=KEYEVENTF_KEYUP;
            require(SendInput(2,input,sizeof(INPUT))==2,"Cannot inject Rich Edit test key");++injected;
            const auto deadline=GetTickCount64()+40;
            do {pump(keys.Get());MsgWaitForMultipleObjects(0,nullptr,FALSE,5,QS_ALLINPUT);}while(GetTickCount64()<deadline);
        };
        tap(edits[0],'A');waitText(edits[0],L"a");tap(edits[0],'B');waitText(edits[0],L"ab");
        tap(edits[0],VK_SPACE);waitText(edits[0],L"交");
        tap(edits[0],'A');tap(edits[0],'B');tap(edits[0],VK_ESCAPE);waitText(edits[0],L"交");
        SetFocus(edits[1]);pump(keys.Get());tap(edits[1],'D');tap(edits[1],'K');tap(edits[1],VK_SPACE);waitText(edits[1],L"口");
        SetFocus(edits[0]);pump(keys.Get());tap(edits[0],'A');tap(edits[0],'B');tap(edits[0],VK_SPACE);waitText(edits[0],L"交交");
        require(text(edits[1])==L"口","Second edit content changed");
        tap(edits[0],'A');tap(edits[0],'B');tap(edits[0],'2');waitText(edits[0],L"交交疒");
        tap(edits[0],'A');tap(edits[0],'B');tap(edits[0],VK_BACK);waitText(edits[0],L"交交疒a");
        tap(edits[0],'B');tap(edits[0],VK_SPACE);waitText(edits[0],L"交交疒交");
        tap(edits[0],VK_RETURN);
        const auto linePrefix=text(edits[0]);
        require(linePrefix==L"交交疒交\r" || linePrefix==L"交交疒交\r\n", "Idle Enter did not insert exactly one line break");
        require(SendMessageW(edits[0],EM_GETLINECOUNT,0,0)==2,"Unexpected Rich Edit line count");
        tap(edits[0],'D');tap(edits[0],'K');tap(edits[0],VK_SPACE);waitText(edits[0],linePrefix+L"口");
        // Replace one committed character through a genuine Rich Edit selection.
        SendMessageW(edits[0],EM_SETSEL,0,1);
        tap(edits[0],'D');tap(edits[0],'K');tap(edits[0],VK_SPACE);
        waitText(edits[0],L"口"+linePrefix.substr(1)+L"口");
        require(text(edits[1])==L"口","Extended input changed the other edit");
        HMODULE module=GetModuleHandleW(L"SampleIME.dll");wchar_t path[32768]{};
        require(module && GetModuleFileNameW(module,path,32768) && std::filesystem::equivalent(path,argv[1]),"Unexpected registered DLL loaded");
        DestroyWindow(parent);check(manager->Deactivate());
        std::cout<<"{\"status\":\"passed\",\"standard_rich_edit\":true,\"controls\":2,\"injected_taps\":"<<injected
            <<",\"cases\":[\"preedit_commit\",\"escape\",\"context_return\",\"numeric_selection\",\"backspace\",\"new_line\",\"selection_replacement\"]"
            <<",\"module_verified\":true,\"physical_hardware_keys\":false,\"third_party_application\":false}\n";
    }catch(const std::exception& e){
        std::cerr<<e.what()<<'\n';
        for(unsigned i=observationCount>128?observationCount-128:0;i<observationCount;++i){
            const auto& event=observations[i%128];
            std::cerr<<"key message="<<event.message<<" vk="<<event.key<<" test_hr="<<event.test
                <<" tested="<<event.tested<<" dispatch_hr="<<event.dispatch<<" eaten="<<event.eaten<<'\n';
        }
        return 1;
    }
}
