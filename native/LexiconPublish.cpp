#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include "LexiconPublish.h"
#include "LexiconSerialize.h"
#include "Dictionary.h"
#include <algorithm>
#include <stdexcept>
#include <system_error>
namespace tiger {
namespace {
[[noreturn]] void failed(const char* action) {throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),action);}
struct Temporary {
    std::filesystem::path path;
    HANDLE file=INVALID_HANDLE_VALUE;
    bool owned=false;
    ~Temporary(){if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);if(owned) DeleteFileW(path.c_str());}
};
}
void publishImportedLexicon(const ImportedLexicon& lexicon,const std::filesystem::path& destination) {
    if(!destination.is_absolute() || destination.filename().empty()) throw std::invalid_argument("Absolute dictionary output path required");
    const auto bytes=serializeImportedLexicon(lexicon);
    std::filesystem::create_directories(destination.parent_path());
    GUID guid{};wchar_t name[40]{};
    if(FAILED(CoCreateGuid(&guid)) || !StringFromGUID2(guid,name,40)) throw std::runtime_error("Cannot name temporary dictionary");
    Temporary temporary{destination.parent_path()/(std::wstring(L".tiger-")+name+L".tmp")};
    temporary.file=CreateFileW(temporary.path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(temporary.file==INVALID_HANDLE_VALUE) failed("Create temporary dictionary");
    temporary.owned=true;
    for(std::size_t at=0;at<bytes.size();) {
        const auto count=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-at,1024*1024));
        DWORD written=0;
        if(!WriteFile(temporary.file,bytes.data()+at,count,&written,nullptr)) failed("Write dictionary");
        if(!written) throw std::runtime_error("Dictionary write made no progress");
        at+=written;
    }
    if(!FlushFileBuffers(temporary.file)) failed("Flush dictionary");
    if(!CloseHandle(temporary.file)) failed("Close dictionary");
    temporary.file=INVALID_HANDLE_VALUE;
    // The production mapped reader validates every record before publication.
    auto validated=Dictionary::Open(temporary.path);
    if(!MoveFileExW(temporary.path.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)) failed("Publish dictionary without replacement");
    temporary.owned=false;
}
}
