#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include "ReminderLedger.h"

namespace {
struct Handle {
    HANDLE value=nullptr;
    ~Handle(){if(value)CloseHandle(value);}
};
struct State {std::uint64_t stamp=0,deadline=0;};
struct Mapping {
    State* value=nullptr;
    ~Mapping(){if(value)UnmapViewOfFile(value);}
};
struct Lock {
    HANDLE mutex;
    explicit Lock(HANDLE value):mutex(value) {
        const auto result=WaitForSingleObject(mutex,5000);
        if(result!=WAIT_OBJECT_0 && result!=WAIT_ABANDONED)throw std::runtime_error("Cannot lock reminder state");
    }
    ~Lock(){ReleaseMutex(mutex);}
};
std::uint64_t number(const wchar_t* text) {
    std::uint64_t value=0;
    if(!*text)throw std::runtime_error("Missing reminder argument");
    for(;*text;++text) {
        if(*text<L'0' || *text>L'9' || value>(std::numeric_limits<std::uint64_t>::max()-(*text-L'0'))/10)
            throw std::runtime_error("Invalid reminder argument");
        value=value*10+(*text-L'0');
    }
    return value;
}
void signal(HANDLE event) {
    if(event && !SetEvent(event))throw std::runtime_error("Cannot signal reminder result");
}
void showPopup() {
    const auto previous=GetThreadDesktop(GetCurrentThreadId());
    const auto target=OpenDesktopW(L"Default",0,FALSE,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS);
    if(!target)throw std::runtime_error("Cannot open reminder desktop");
    if(!SetThreadDesktop(target)){CloseDesktop(target);throw std::runtime_error("Cannot select reminder desktop");}
    MessageBoxW(nullptr,L"时间差不多咯！",L"计时器",MB_OK|MB_ICONINFORMATION);
    SetThreadDesktop(previous);CloseDesktop(target);
}
}

// Standalone lifecycle prototype. Production launch/acknowledgement and scope
// selection are not wired into the TSF service yet. The explicit test mode uses
// named events instead of opening a reminder on the user's desktop.
int wmain(int argc,wchar_t** argv) {
    try {
        const bool armedTest=argc==11 && std::wstring(argv[1])==L"--armed-test";
        if(armedTest || (argc==10 && std::wstring(argv[1])==L"--armed")) {
            const std::filesystem::path path=argv[2];
            const tiger::ReminderTicket ticket{reinterpret_cast<const char16_t*>(argv[3]),number(argv[4])};
            const auto parentId=number(argv[5]);
            if(parentId>std::numeric_limits<DWORD>::max())return 2;
            Handle parent{OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,static_cast<DWORD>(parentId))};
            Handle ready{OpenEventW(EVENT_MODIFY_STATE,FALSE,argv[7])};
            Handle go{OpenEventW(SYNCHRONIZE,FALSE,argv[8])},cancel{OpenEventW(SYNCHRONIZE,FALSE,argv[9])},fired;
            if(!parent.value || !ready.value || !go.value || !cancel.value)return 2;
            FILETIME created{},exited{},kernel{},user{};
            if(!GetProcessTimes(parent.value,&created,&exited,&kernel,&user))return 2;
            if(((static_cast<std::uint64_t>(created.dwHighDateTime)<<32)|created.dwLowDateTime)!=number(argv[6]))return 2;
            if(armedTest){fired.value=OpenEventW(EVENT_MODIFY_STATE,FALSE,argv[10]);if(!fired.value)return 2;}
            signal(ready.value);
            const HANDLE waiting[]={cancel.value,go.value,parent.value};
            const auto wake=WaitForMultipleObjects(3,waiting,FALSE,INFINITE);
            if(wake==WAIT_OBJECT_0)return 0;
            if(wake!=WAIT_OBJECT_0+1 && wake!=WAIT_OBJECT_0+2)return 1;
            for(;;) {
                const auto current=tiger::readReminder(path);
                if(current.epoch!=ticket.epoch || current.accepted!=ticket.sequence)return 0;
                const auto now=GetTickCount64();
                if(current.deadline<=now)break;
                Sleep(static_cast<DWORD>(std::min<std::uint64_t>(current.deadline-now,100)));
            }
            if(armedTest)signal(fired.value);else showPopup();
            return 0;
        }
        const bool ledgerTest=argc==8 && std::wstring(argv[1])==L"--ledger-test";
        if(ledgerTest || (argc==7 && std::wstring(argv[1])==L"--ledger")) {
            const std::filesystem::path path=argv[2];
            const tiger::ReminderTicket ticket{reinterpret_cast<const char16_t*>(argv[3]),number(argv[4])};
            const auto deadline=number(argv[5]);
            Handle ready{OpenEventW(EVENT_MODIFY_STATE,FALSE,argv[6])},fired;
            if(!ready.value)return 2;
            if(ledgerTest) {
                fired.value=OpenEventW(EVENT_MODIFY_STATE,FALSE,argv[7]);
                if(!fired.value)return 2;
            }
            if(!tiger::acceptReminder(path,ticket,deadline)){signal(ready.value);return 0;}
            signal(ready.value);
            for(;;) {
                const auto current=tiger::readReminder(path);
                if(current.epoch!=ticket.epoch || current.accepted!=ticket.sequence)return 0;
                const auto now=GetTickCount64();
                if(current.deadline<=now)break;
                Sleep(static_cast<DWORD>(std::min<std::uint64_t>(current.deadline-now,100)));
            }
            if(ledgerTest)signal(fired.value);else showPopup();
            return 0;
        }
        const bool test=argc==7 && std::wstring(argv[1])==L"--test";
        const bool acknowledged=argc==6 && std::wstring(argv[1])==L"--timer";
        if(!test && !acknowledged && !(argc==5 && std::wstring(argv[1])==L"--timer"))return 2;
        GUID scope{};
        if(FAILED(CLSIDFromString(argv[2],&scope)))return 2;
        wchar_t canonical[40]{};
        if(!StringFromGUID2(scope,canonical,40))return 2;
        const auto stamp=number(argv[3]),deadline=number(argv[4]);
        if(!stamp)return 2;
        Handle ready,fired;
        if(test || acknowledged) {
            ready.value=OpenEventW(EVENT_MODIFY_STATE,FALSE,argv[5]);
            if(!ready.value)return 2;
        }
        if(test) {
            fired.value=OpenEventW(EVENT_MODIFY_STATE,FALSE,argv[6]);
            if(!fired.value)return 2;
        }
        const std::wstring name=L"Local\\NativeTiger.Reminder."+std::wstring(canonical);
        Handle mutex{CreateMutexW(nullptr,FALSE,(name+L".lock").c_str())};
        if(!mutex.value)throw std::runtime_error("Cannot create reminder mutex");
        Handle file{CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(State),(name+L".state").c_str())};
        if(!file.value)throw std::runtime_error("Cannot create reminder mapping");
        Mapping state{static_cast<State*>(MapViewOfFile(file.value,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(State)))};
        if(!state.value)throw std::runtime_error("Cannot map reminder state");
        {
            Lock lock(mutex.value);
            if(stamp<=state.value->stamp){signal(ready.value);return 0;}
            *state.value={stamp,deadline};
        }
        signal(ready.value);
        for(;;) {
            ULONGLONG remaining=0;
            {
                Lock lock(mutex.value);
                if(state.value->stamp!=stamp)return 0;
                const auto now=GetTickCount64();
                if(state.value->deadline>now)remaining=state.value->deadline-now;
            }
            if(!remaining)break;
            Sleep(static_cast<DWORD>(std::min<ULONGLONG>(remaining,100)));
        }
        if(test)signal(fired.value);
        else showPopup();
        return 0;
    } catch(const std::exception& error) {
        OutputDebugStringA(error.what());return 1;
    }
}
