#define NOMINMAX
#include "ReminderLaunch.h"
#include <objbase.h>
#include <stdexcept>
#include <string_view>
#include <system_error>
namespace tiger {
namespace {
struct Handle {HANDLE value=nullptr;~Handle(){if(value)CloseHandle(value);}};
struct Cancel {
    HANDLE event;bool complete=false;
    ~Cancel(){if(!complete)SetEvent(event);}
};
[[noreturn]] void failure(const char* message) {throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),message);}
std::wstring quote(std::wstring_view argument) {
    std::wstring result=L"\"";std::size_t slashes=0;
    for(auto c:argument) {
        if(c==L'\\'){++slashes;continue;}
        result.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;result+=c;
    }
    result.append(slashes*2,L'\\');result+=L'"';return result;
}
}
LaunchedReminder launchReminder(const std::filesystem::path& executable,const std::filesystem::path& ledger,
    int milliseconds,const std::wstring& testEvent) {
    if(milliseconds<0)return {};
    const auto deadline=GetTickCount64()+static_cast<ULONGLONG>(milliseconds);
    if(!std::filesystem::is_regular_file(executable))throw std::runtime_error("Reminder executable is missing");
    GUID id{};wchar_t identifier[40]{};
    if(FAILED(CoCreateGuid(&id)) || !StringFromGUID2(id,identifier,40))throw std::runtime_error("Cannot identify reminder launch");
    const auto prefix=L"Local\\Tigirl.ReminderLaunch."+std::wstring(identifier);
    const auto readyName=prefix+L".ready",goName=prefix+L".go",cancelName=prefix+L".cancel";
    Handle ready{CreateEventW(nullptr,TRUE,FALSE,readyName.c_str())};
    Handle go{CreateEventW(nullptr,TRUE,FALSE,goName.c_str())};
    Handle cancel{CreateEventW(nullptr,TRUE,FALSE,cancelName.c_str())};
    if(!ready.value || !go.value || !cancel.value)failure("Create reminder startup events");
    Cancel cleanup{cancel.value};
    FILETIME created{},exited{},kernel{},user{};
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))failure("Identify reminder parent");
    const auto creation=(static_cast<std::uint64_t>(created.dwHighDateTime)<<32)|created.dwLowDateTime;
    const auto ticket=reserveReminder(ledger);
    std::wstring command=quote(executable.wstring());
    const std::wstring arguments[]={testEvent.empty()?L"--armed":L"--armed-test",ledger.wstring(),
        std::wstring(ticket.epoch.begin(),ticket.epoch.end()),std::to_wstring(ticket.sequence),
        std::to_wstring(GetCurrentProcessId()),std::to_wstring(creation),readyName,goName,cancelName};
    for(const auto& argument:arguments)command+=L" "+quote(argument);
    if(!testEvent.empty())command+=L" "+quote(testEvent);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,
        executable.parent_path().c_str(),&startup,&process))failure("Launch reminder");
    Handle child{process.hProcess},thread{process.hThread};
    const HANDLE waiting[]={ready.value,child.value};
    const auto result=WaitForMultipleObjects(2,waiting,FALSE,5000);
    if(result!=WAIT_OBJECT_0 || WaitForSingleObject(child.value,0)==WAIT_OBJECT_0)
        throw std::runtime_error("Reminder failed to acknowledge startup");
    if(!acceptReminder(ledger,ticket,deadline))return {ticket,process.dwProcessId};
    if(!SetEvent(go.value))failure("Release reminder startup gate");
    cleanup.complete=true;
    return {ticket,process.dwProcessId};
}
}
