namespace sentence_tsf_fixture {
inline void run(ITfTextInputProcessorEx* service,ITfThreadMgr* manager,Document& doc,Document& other,UISink* ui,bool measure=false) {
    ComPtr<ITfKeyEventSink> keys;check(service->QueryInterface(IID_PPV_ARGS(&keys)));
    ComPtr<ITfThreadMgrEventSink> focus;check(service->QueryInterface(IID_PPV_ARGS(&focus)));
    ComPtr<ITfUIElementMgr> elements;check(manager->QueryInterface(IID_PPV_ARGS(&elements)));
    check(focus->OnSetFocus(doc.manager.Get(),nullptr));
    struct Keyboard {BYTE saved[256];Keyboard(){GetKeyboardState(saved);BYTE empty[256]{};SetKeyboardState(empty);}~Keyboard(){SetKeyboardState(saved);}} keyboard;
    auto settle=[](unsigned ms){auto until=GetTickCount64()+ms;while(GetTickCount64()<until){pump();Sleep(5);}};
    auto tapTo=[&](Document& target,WPARAM vk) {
        const auto before=target.store->text;
        BOOL eaten=FALSE;
        for(int i=0;i<2;++i){check(keys->OnTestKeyDown(target.context.Get(),vk,1,&eaten));require(eaten,"Sentence preview did not consume key");require(target.store->text==before,"Sentence preview changed document");}
        check(keys->OnKeyDown(target.context.Get(),vk,1,&eaten));require(eaten,"Sentence dispatch did not consume key");
        check(keys->OnTestKeyUp(target.context.Get(),vk,1,&eaten));if(eaten)check(keys->OnKeyUp(target.context.Get(),vk,1,&eaten));
    };
    auto tap=[&](WPARAM vk){tapTo(doc,vk);};
    auto type=[&]{for(auto vk:{'A','A','B','B'})tap(vk);};
    auto list=[&] {
        ComPtr<ITfUIElement> element;check(elements->GetUIElement(ui->id,&element));
        ComPtr<ITfCandidateListUIElement> result;check(element.As(&result));return result;
    };
    // Resource loading is asynchronous; this wait pumps the actual apartment.
    settle(1000);
    wchar_t rootText[32768]{};GetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",rootText,32768);
    const std::filesystem::path root(rootText);
    if(measure) {
        std::ifstream input(root/L".sentence-measure.tsv");require(static_cast<bool>(input),"Missing sentence measurement input");
        LARGE_INTEGER frequency{};require(QueryPerformanceFrequency(&frequency)!=FALSE,"Performance counter unavailable");
        auto tick=[] {LARGE_INTEGER value{};QueryPerformanceCounter(&value);return value.QuadPart;};
        auto elapsed=[&](LONGLONG start){return 1000.0*(tick()-start)/frequency.QuadPart;};
        auto memory=[] {
            PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);
            require(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters))!=FALSE,"Cannot measure sentence process memory");
            return counters.PrivateUsage;
        };
        unsigned pause=0;std::string raw;
        while(input>>pause>>raw) {
            require(!raw.empty() && raw.size()<=128 && pause<=1000,"Invalid sentence measurement case");
            const auto before=doc.store->text;const auto privateBefore=memory();std::vector<double> times;
            for(char code:raw) {
                require(code>='a' && code<='z',"Measurement code is not ASCII alphabetic");
                const auto start=tick();tap(static_cast<WPARAM>(code-'a'+'A'));times.push_back(elapsed(start));
                if(pause)settle(pause);
            }
            require(compositionCount(doc)==1,"Measurement did not enter sentence composition");
            const auto privateActive=memory();const auto start=tick();tap(VK_SPACE);const auto commitMs=elapsed(start);
            require(compositionCount(doc)==0 && doc.store->text.size()>before.size(),"Measured commit did not finish composition");
            const auto committedText=doc.store->text.substr(before.size());settle(200);
            std::sort(times.begin(),times.end());
            std::cout<<"{\"raw_length\":"<<raw.size()<<",\"pause_ms\":"<<pause<<",\"key_p95_ms\":"<<times[(times.size()-1)*95/100]
                <<",\"key_max_ms\":"<<times.back()<<",\"commit_ms\":"<<commitMs<<",\"private_before\":"<<privateBefore
                <<",\"private_active\":"<<privateActive<<",\"private_after_commit\":"<<memory()<<",\"committed_utf16\":[";
            for(std::size_t i=0;i<committedText.size();++i){if(i)std::cout<<',';std::cout<<static_cast<unsigned>(committedText[i]);}
            std::cout<<"]}\n";
        }
        return;
    }
    type();tap(VK_SPACE);require(doc.store->text==L"中国","Immediate Space did not synchronously complete sentence");
    require(compositionCount(doc)==0,"Sentence commit retained composition");
    type();settle(200);
    BSTR text=nullptr;check(list()->GetString(0,&text));std::wstring first=text;SysFreeString(text);
    require(first==L"中国","Background decoder did not publish candidates");
    tap(VK_TAB);UINT selected=0;check(list()->GetSelection(&selected));require(selected==1,"Sentence selected row not exposed to TSF UI");
    tap(VK_SPACE);require(doc.store->text==L"中国中华","Tab and Space candidate selection");
    type();tap('2');tap(VK_SPACE);require(doc.store->text==L"中国中华中华","Numeric selector should remain in raw sentence");
    type();tap(VK_BACK);tap(VK_ESCAPE);const auto committed=doc.store->text;settle(150);
    require(doc.store->text==committed && compositionCount(doc)==0,"Canceled generation rewrote document");
    type();tap(VK_OEM_PERIOD);require(doc.store->text==committed+L"中国。","Sentence punctuation commit");
    type();check(focus->OnSetFocus(other.manager.Get(),doc.manager.Get()));
    for(auto vk:{'A','A','B','B'})tapTo(other,vk);tapTo(other,VK_SPACE);
    settle(150);require(other.store->text==L"中国","Second context sentence result");
    check(focus->OnSetFocus(doc.manager.Get(),other.manager.Get()));tap(VK_ESCAPE);
    require(doc.store->text==committed+L"中国。","Old focused generation inserted text");
    // Hold the actual TSF edit lock after typing. The worker has finished and
    // pollSentence has captured its completion in a queued edit-session callback.
    // A positive control proves that granting this lock publishes candidates.
    auto holdComputed=[&] {
        type();doc.store->deferLocks=true;
        const auto deadline=GetTickCount64()+3000;
        while(!doc.store->pendingLock && GetTickCount64()<deadline){pump();Sleep(5);}
        require(doc.store->pendingLock!=0,"Computed sentence result did not request a deferred edit");
    };
    auto releaseComputed=[&] {doc.store->deferLocks=false;check(doc.store->grantPendingLock());settle(50);};
    holdComputed();releaseComputed();
    BSTR delivered=nullptr;check(list()->GetString(0,&delivered));const std::wstring deliveredText=delivered;SysFreeString(delivered);
    require(deliveredText==L"中国","Deferred-result positive control did not publish decoded candidates");tap(VK_ESCAPE);
    holdComputed();check(focus->OnSetFocus(other.manager.Get(),doc.manager.Get()));
    const auto oldText=doc.store->text,otherText=other.store->text;
    const auto oldBegins=ui->begins,oldUpdates=ui->updates;
    releaseComputed();
    require(doc.store->text==oldText && other.store->text==otherText && ui->begins==oldBegins && ui->updates==oldUpdates,
        "Computed result published after switching context");
    check(focus->OnSetFocus(doc.manager.Get(),other.manager.Get()));tap(VK_ESCAPE);
    holdComputed();ComPtr<ITfThreadFocusSink> threadFocus;check(service->QueryInterface(IID_PPV_ARGS(&threadFocus)));
    check(threadFocus->OnKillThreadFocus());
    const auto backgroundText=doc.store->text;const auto backgroundBegins=ui->begins,backgroundUpdates=ui->updates;
    releaseComputed();
    require(doc.store->text==backgroundText && ui->begins==backgroundBegins && ui->updates==backgroundUpdates,
        "Computed result published after losing thread focus");
    check(keys->OnSetFocus(TRUE));check(focus->OnSetFocus(doc.manager.Get(),nullptr));tap(VK_ESCAPE);
    require(doc.store->text==committed+L"中国。","Deferred-result fixtures changed committed text");
    if(std::filesystem::exists(root/L".sentence-journal-test")) {
        type();settle(200);const auto livePreedit=doc.store->text;
        require(compositionCount(doc)==1,"Missing active sentence before journal refresh");
        std::ifstream lockName(root/L".sentence-next-cache-lock");std::string lockPath;std::getline(lockName,lockPath);
        const auto pendingLock=std::filesystem::u8path(lockPath);std::filesystem::create_directories(pendingLock.parent_path());
        HANDLE heldCache=CreateFileW(pendingLock.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(heldCache!=INVALID_HANDLE_VALUE,"Cannot hold replacement cache");
        OVERLAPPED cachePosition{};require(LockFileEx(heldCache,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&cachePosition)!=FALSE,"Cannot block replacement cache");
        // Independently append a valid Delete(bb, 国) while the TSF stays active.
        const auto journal=root/L"schemas"/L"测试整句"/L"user.tcu";
        HANDLE coordination=CreateFileW((journal.wstring()+L".lock").c_str(),GENERIC_READ|GENERIC_WRITE,
            FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(coordination!=INVALID_HANDLE_VALUE,"Cannot lock sentence journal fixture");
        OVERLAPPED lock{};require(LockFileEx(coordination,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&lock)!=FALSE,"Cannot acquire sentence journal fixture lock");
        HANDLE file=CreateFileW(journal.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(file!=INVALID_HANDLE_VALUE,"Cannot append sentence journal fixture");
        std::vector<unsigned char> payload;
        auto put=[&](unsigned n){for(int i=0;i<4;++i)payload.push_back(static_cast<unsigned char>(n>>(i*8)));};
        put(1);put(2);put(1);for(wchar_t c:std::wstring(L"bb国")){payload.push_back(static_cast<unsigned char>(c));payload.push_back(static_cast<unsigned char>(c>>8));}
        unsigned crc=0xffffffff;for(auto b:payload){crc^=b;for(int i=0;i<8;++i)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);}crc^=0xffffffff;
        std::vector<unsigned char> record;for(unsigned n:{static_cast<unsigned>(payload.size()),crc})for(int i=0;i<4;++i)record.push_back(static_cast<unsigned char>(n>>(i*8)));
        record.insert(record.end(),payload.begin(),payload.end());LARGE_INTEGER end{};DWORD written=0;
        const bool ok=SetFilePointerEx(file,end,nullptr,FILE_END) && WriteFile(file,record.data(),static_cast<DWORD>(record.size()),&written,nullptr) && written==record.size() && FlushFileBuffers(file);
        CloseHandle(file);CloseHandle(coordination);require(ok,"Cannot write sentence journal fixture");
        settle(600);
        require(doc.store->text==livePreedit && compositionCount(doc)==1,"Pending journal resource reload canceled live sentence");
        tap(VK_BACK);tap('B');tap(VK_SPACE);
        require(doc.store->text==committed+L"中国。中国","Commit while replacement is blocked lost keys or mixed snapshots");
        type();const auto retained=doc.store->text;CloseHandle(heldCache);settle(2500);
        // Pending raw aabb becomes segmented aa bb once decoding completes.
        // Compare codes without those display separators, while requiring the
        // same live composition and checking the committed text below.
        auto codes=[](std::wstring value){value.erase(std::remove(value.begin(),value.end(),L' '),value.end());return value;};
        require(codes(doc.store->text)==codes(retained) && compositionCount(doc)==1,"Journal resource adoption canceled live sentence");
        tap(VK_SPACE);
        require(doc.store->text==committed+L"中国。中国中华","Live journal change did not reach retained sentence decoding");
        auto replace=[&](const wchar_t* from,const std::filesystem::path& to) {
            require(MoveFileExW((root/from).c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE,"Cannot publish sentence reload fixture");
        };
        auto beforeBase=doc.store->text;type();auto baseRaw=doc.store->text;
        replace(L".sentence-next-base",root/L"schemas"/L"测试整句"/L"current.txt");settle(2500);
        require(codes(doc.store->text)==codes(baseRaw) && compositionCount(doc)==1,"New base generation canceled live sentence");
        tap(VK_SPACE);require(doc.store->text==beforeBase+L"中州","New base generation used obsolete sentence data");
        auto beforeSchema=doc.store->text;type();auto schemaRaw=doc.store->text;
        replace(L".sentence-next-config",root/L"config.txt");settle(2500);
        require(codes(doc.store->text)==codes(schemaRaw) && compositionCount(doc)==1,"Sentence schema switch canceled live raw");
        tap(VK_SPACE);require(doc.store->text==beforeSchema+L"中文","Sentence schema switch used old candidates");
        const auto beforeAuto=doc.store->text;
        for(auto vk:{'A','A','B','B','A','A','B','B'}){tap(vk);settle(150);}
        require(doc.store->text.substr(beforeAuto.size(),1)==L"中" && compositionCount(doc)==1,"Automatic sentence prefix was not committed while retaining preedit");
        tap(VK_SPACE);require(doc.store->text==beforeAuto+L"中文中文","Automatic prefix duplicated or lost final sentence text");
        const auto beforeEmpty=doc.store->text;
        tap('C');tap('C');settle(300);
        tap('A');settle(150);
        require(doc.store->text.substr(beforeEmpty.size(),1)==L"明" && compositionCount(doc)==1,"Empty-code prefix not committed before retained tail");
        tap('A');settle(150);tap(VK_SPACE);
        require(doc.store->text==beforeEmpty+L"明中","Empty-code continuation duplicated or lost text");
        require(MoveFileExW((root/L".sentence-timing-config").c_str(),(root/L"config.txt").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE,"Cannot publish timing configuration");
        settle(1500);const auto beforeTiming=doc.store->text;
        for(int i=0;i<6;++i){tap('C');settle(150);}
        require(doc.store->text.substr(beforeTiming.size(),1)==L"c" && compositionCount(doc)==1,"Decoder completion committed before the next appended key");
        settle(400);require(doc.store->text.substr(beforeTiming.size(),1)==L"c","Idle decoder publication committed a prefix");
        tap('C');settle(150);
        require(doc.store->text.substr(beforeTiming.size(),1)==L"明" && compositionCount(doc)==1,"Next appended key did not commit mature evidence");
        tap('C');settle(150);tap(VK_SPACE);
        require(doc.store->text==beforeTiming+L"明明明明","Scheduled prefix commit lost final text");
    }
}
}
