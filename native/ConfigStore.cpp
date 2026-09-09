#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include "ConfigStore.h"
#include "Settings.h"
#include "OrdinalCase.h"
#include "SchemaCatalog.h"
#include "Text.h"
#include "SelectionKeys.h"
#include "ReminderLedger.h"
#include <atomic>
#include <algorithm>
#include <functional>
#include <system_error>
#include <limits>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <locale>
namespace tiger {
namespace {
[[noreturn]] void failure(const char* operation) { throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),operation); }
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    ~Handle() { if(value!=INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Temporary {
    std::filesystem::path path;
    ~Temporary() { if(!path.empty()) DeleteFileW(path.c_str()); }
};
}
std::u16string readConfiguration(const std::filesystem::path& path) {
    return decodeUnicodeText(readConfigurationBytes(path));
}
std::string readConfigurationBytes(const std::filesystem::path& path) {
    const auto lockPath=std::filesystem::path(path.native()+L".lock");
    // If the first writer creates the sidecar concurrently, a second lookup
    // distinguishes its replacement gap from a configuration that is absent.
    for(int attempt=0;attempt<2;++attempt) {
        Handle lock{CreateFileW(lockPath.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
        if(lock.value!=INVALID_HANDLE_VALUE) {
            OVERLAPPED offset{};
            if(!LockFileEx(lock.value,0,0,1,0,&offset)) failure("Lock configuration read");
            return std::filesystem::exists(path)?readTextFileBytes(path):std::string{};
        }
        const auto error=GetLastError();
        if(error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND) failure("Open configuration read lock");
        if(std::filesystem::exists(path)) return readTextFileBytes(path);
    }
    return {};
}
static void mutateConfiguration(const std::filesystem::path& path,
    const std::function<std::u16string(std::u16string_view)>& mutate,bool readExisting=true) {
    if(path.empty()) throw std::invalid_argument("No user configuration path");
    if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    const auto lockPath=std::filesystem::path(path.native()+L".lock");
    Handle lock{CreateFileW(lockPath.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
        nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(lock.value==INVALID_HANDLE_VALUE) failure("Open configuration lock");
    OVERLAPPED offset{};
    if(!LockFileEx(lock.value,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&offset)) failure("Lock configuration");
    const auto text=readExisting && std::filesystem::exists(path)?readUnicodeFile(path):std::u16string{};
    const auto updated=mutate(text);
    if(updated==text) return;
    const auto bytes=std::string("\xef\xbb\xbf")+utf8(updated);
    if(bytes.size()>4*1024*1024) throw std::runtime_error("Updated configuration exceeds 4 MiB");
    static std::atomic<unsigned> serial{0};
    Temporary temporary{std::filesystem::path(path.native()+L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+
        std::to_wstring(GetTickCount64())+L"."+std::to_wstring(serial.fetch_add(1)))};
    {
        Handle output{CreateFileW(temporary.path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
        if(output.value==INVALID_HANDLE_VALUE) { temporary.path.clear(); failure("Create temporary configuration"); }
        std::size_t written=0;
        while(written<bytes.size()) {
            DWORD count=0;
            if(!WriteFile(output.value,bytes.data()+written,static_cast<DWORD>(bytes.size()-written),&count,nullptr)) failure("Write configuration");
            if(!count) throw std::runtime_error("Empty configuration write");
            written+=count;
        }
        if(!FlushFileBuffers(output.value)) failure("Flush configuration");
    }
    if(std::filesystem::exists(path)) {
        const auto backup=std::filesystem::path(temporary.path.native()+L".backup");
        if(!ReplaceFileW(path.c_str(),temporary.path.c_str(),backup.c_str(),0,nullptr,nullptr)) {
            const auto error=GetLastError();
            if(!std::filesystem::exists(path) && std::filesystem::exists(backup))
                MoveFileExW(backup.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH);
            // A denied replacement normally leaves the original untouched.
            // Only discard the unpublished file after verifying that state;
            // partial replacement failures must retain recovery artifacts.
            bool unchanged=false;
            try {
                unchanged=!std::filesystem::exists(backup) && std::filesystem::exists(path)
                    && readUnicodeFile(path)==text;
            } catch(...) {}
            if(!unchanged) temporary.path.clear();
            SetLastError(error);
            failure(unchanged?"Replace configuration":"Replace configuration; recovery files retained");
        }
        DeleteFileW(backup.c_str());
    } else if(!MoveFileExW(temporary.path.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH)) failure("Publish configuration");
    temporary.path.clear();
}
namespace {
std::uint64_t reminderNumber(std::u16string_view text) {
    if(text.empty())throw std::runtime_error("Missing reminder counter");
    std::uint64_t value=0;
    for(auto c:text) {
        if(c<u'0' || c>u'9' || value>(std::numeric_limits<std::uint64_t>::max()-(c-u'0'))/10)
            throw std::runtime_error("Invalid reminder counter");
        value=value*10+(c-u'0');
    }
    return value;
}
ReminderRecord reminderRecord(std::u16string_view text) {
    if(configurationValue(text,u"version")!=u"1")throw std::runtime_error("Unsupported reminder ledger");
    ReminderRecord record;
    record.epoch=configurationValue(text,u"epoch");
    if(record.epoch.size()!=38 || record.epoch.front()!=u'{' || record.epoch.back()!=u'}')
        throw std::runtime_error("Invalid reminder epoch");
    for(std::size_t i=1;i<37;++i) {
        const auto c=record.epoch[i];
        if(i==9 || i==14 || i==19 || i==24) {if(c==u'-')continue;}
        else if((c>=u'0' && c<=u'9') || (c>=u'A' && c<=u'F') || (c>=u'a' && c<=u'f'))continue;
        throw std::runtime_error("Invalid reminder epoch");
    }
    record.allocated=reminderNumber(configurationValue(text,u"allocated"));
    record.accepted=reminderNumber(configurationValue(text,u"accepted"));
    record.deadline=reminderNumber(configurationValue(text,u"deadline"));
    if(record.accepted>record.allocated)throw std::runtime_error("Invalid reminder ordering");
    return record;
}
std::u16string reminderText(const ReminderRecord& record) {
    return u"version\t1\r\nepoch\t"+record.epoch+u"\r\nallocated\t"+utf16(std::to_string(record.allocated))+
        u"\r\naccepted\t"+utf16(std::to_string(record.accepted))+u"\r\ndeadline\t"+utf16(std::to_string(record.deadline))+u"\r\n";
}
}
ReminderRecord readReminder(const std::filesystem::path& path) {return reminderRecord(readConfiguration(path));}
ReminderTicket reserveReminder(const std::filesystem::path& path) {
    ReminderTicket ticket;
    mutateConfiguration(path,[&](std::u16string_view text) {
        ReminderRecord record;
        if(std::filesystem::exists(path))record=reminderRecord(text);
        else {
            GUID id{};wchar_t identifier[40]{};
            if(FAILED(CoCreateGuid(&id)) || !StringFromGUID2(id,identifier,40))throw std::runtime_error("Cannot initialize reminder epoch");
            record.epoch=reinterpret_cast<const char16_t*>(identifier);
        }
        if(record.allocated==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("Reminder counter exhausted");
        ticket={record.epoch,++record.allocated};
        return reminderText(record);
    });
    return ticket;
}
bool acceptReminder(const std::filesystem::path& path,const ReminderTicket& ticket,std::uint64_t deadline) {
    bool accepted=false;
    mutateConfiguration(path,[&](std::u16string_view text) {
        auto record=reminderRecord(text);
        if(ticket.epoch!=record.epoch || ticket.sequence<=record.accepted || ticket.sequence>record.allocated)
            return std::u16string(text);
        record.accepted=ticket.sequence;record.deadline=deadline;accepted=true;
        return reminderText(record);
    });
    return accepted;
}
CandidateStyle cycleCandidateMode(const std::filesystem::path& path,bool& horizontalCode,bool& verticalCode) {
    CandidateStyle style;
    bool horizontal=horizontalCode,vertical=verticalCode;
    mutateConfiguration(path,[&](std::u16string_view text) {
        style=parseCandidateStyle(text);
        if(!style.hideCandidates)(style.vertical?vertical:horizontal)=style.showCode;
        if(style.hideCandidates && style.showCode) {style.hideCandidates=false;style.vertical=false;style.showCode=horizontal;}
        else if(style.vertical) {style.hideCandidates=true;style.showCode=true;}
        else {style.hideCandidates=false;style.vertical=true;style.showCode=vertical;}
        auto updated=withConfigurationValue(text,u"隐藏候选",style.hideCandidates?u"是":u"否");
        updated=withConfigurationValue(updated,u"竖排候选",style.vertical?u"是":u"否");
        return withConfigurationValue(updated,u"候选窗显示编码",style.showCode?u"是":u"否");
    });
    horizontalCode=horizontal;verticalCode=vertical;return style;
}
double adjustCandidateFontSize(const std::filesystem::path& path,int wheelDelta) {
    double size=17;
    mutateConfiguration(path,[&](std::u16string_view text) {
        size=parseCandidateStyle(text).fontSize;
        size=std::round(std::clamp(size+wheelDelta/120.0*0.5,3.0,200.0)*100.0)/100.0;
        std::ostringstream value;value.imbue(std::locale::classic());value<<std::fixed<<std::setprecision(2)<<size;
        return withConfigurationValue(text,u"字体大小",utf16(value.str()));
    });
    return size;
}
bool toggleHiddenCandidates(const std::filesystem::path& path) {
    bool hidden=false;
    mutateConfiguration(path,[&](std::u16string_view text) {
        hidden=!parseCandidateStyle(text).hideCandidates;
        return withHiddenCandidateSetting(text,hidden);
    });
    return hidden;
}
static std::u16string selectedSchemaText(std::u16string_view text,std::u16string_view canonicalName) {
        auto equal=[](const std::u16string& a,const std::u16string& b) {
            return ordinalCompareIgnoreCase(a,b)==0;
        };
        // Reuse the configuration parser's Unicode trimming rules for each MRU part.
        std::vector<std::u16string> recent;
        auto raw=configurationValue(text,u"最近码表对");
        std::u16string_view remaining(raw);
        while(!remaining.empty() && recent.size()<2) {
            const auto separator=remaining.find(u'|');
            auto name=configurationValue(u"x\t"+std::u16string(remaining.substr(0,separator)),u"x");
            if(!name.empty() && std::none_of(recent.begin(),recent.end(),[&](const auto& item){return equal(item,name);}))
                recent.push_back(std::move(name));
            if(separator==remaining.npos) break;
            remaining.remove_prefix(separator+1);
        }
        auto record=[&](std::u16string name) {
            if(name.empty()) return;
            recent.erase(std::remove_if(recent.begin(),recent.end(),[&](const auto& item){return equal(item,name);}),recent.end());
            recent.insert(recent.begin(),std::move(name));
            if(recent.size()>2) recent.resize(2);
        };
        auto current=currentSchemaSetting(text);
        record(current.empty()?u"虎码字词":std::move(current));
        record(std::u16string(canonicalName));
        std::u16string pair=recent.front();
        if(recent.size()>1) pair+=u"|"+recent[1];
        auto updated=withConfigurationValue(text,u"当前码表",canonicalName);
        return withConfigurationValue(updated,u"最近码表对",pair);
}
void updateSelectionKeys(const std::filesystem::path& path,const SelectionKeys& before,const SelectionKeys& after) {
    if(before.bindings==after.bindings)return;
    mutateConfiguration(path,[&](std::u16string_view text) {
        SelectionKeys latest;std::u16string error;
        if(!SelectionKeys::parse(text,latest,error))throw std::runtime_error(utf8(error));
        std::u16string updated(text);
        for(int i=0;i<10;++i)if(before.bindings[i]!=after.bindings[i]) {
            latest.bindings[i]=after.bindings[i];
            const auto label=utf16(std::to_string(i+1))+u"选";
            updated=withConfigurationValue(updated,label,configurationValue(latest.serialize(),label));
        }
        return updated;
    });
}
void repairSelectionKeys(const std::filesystem::path& path,std::string_view expectedBytes,const SelectionKeys& after) {
    mutateConfiguration(path,[&](std::u16string_view) {
        // Already under the stable exclusive lock. Read failure is not a
        // malformed-text condition and must never authorize replacement.
        const auto bytes=readTextFileBytes(path);
        if(bytes!=expectedBytes)throw std::runtime_error("选重键文件已在其他位置修改，请重新打开后再保存。");
        SelectionKeys parsed;std::u16string error;
        std::u16string text;bool decoded=false;
        try{text=decodeUnicodeText(bytes);decoded=true;}catch(const std::runtime_error&){}catch(const std::invalid_argument&){}
        if(decoded && SelectionKeys::parse(text,parsed,error))throw std::runtime_error("选重键文件已恢复正常，请重新打开后再编辑。");
        GUID id{};wchar_t identifier[40]{};
        if(FAILED(CoCreateGuid(&id)) || !StringFromGUID2(id,identifier,40))
            throw std::runtime_error("无法创建选重键备份名称。");
        const auto backup=std::filesystem::path(path.native()+L".invalid."+identifier);
        if(!CopyFileW(path.c_str(),backup.c_str(),TRUE))failure("Back up invalid selection keys");
        return after.serialize();
    },false);
}
void updateConfigurationValues(const std::filesystem::path& path,
    const std::vector<std::pair<std::u16string,std::u16string>>& changes,
    const std::function<void(std::u16string_view)>& validate) {
    for(const auto& [key,value]:changes) {
        if(key.empty() || key.find_first_of(u"\r\n\t ,")!=key.npos || value.find_first_of(u"\r\n")!=value.npos)
            throw std::invalid_argument("Invalid configuration field");
    }
    if(changes.empty())return;
    mutateConfiguration(path,[&](std::u16string_view text) {
        std::u16string updated(text);
        for(const auto& [key,value]:changes)updated=withConfigurationValue(updated,key,value);
        if(validate)validate(updated);
        return updated;
    });
}
void saveInputConfiguration(const std::filesystem::path& path,
    const std::vector<std::pair<std::u16string,std::u16string>>& changes,
    const std::function<void(std::u16string_view)>& validate) {
    if(changes.empty())return;
    for(const auto& change:changes)if(change.first==inputSettingsReloadKey)
        throw std::invalid_argument("The settings reload identifier is reserved");
    GUID id{};wchar_t text[40]{};
    if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("Cannot identify settings reload");
    const auto count=StringFromGUID2(id,text,40);
    if(!count)throw std::runtime_error("Cannot format settings reload identifier");
    auto updated=changes;
    updated.emplace_back(inputSettingsReloadKey,std::u16string(reinterpret_cast<const char16_t*>(text),count-1));
    updateConfigurationValues(path,updated,validate);
}
void selectSchemaConfiguration(const std::filesystem::path& path,std::u16string_view canonicalName) {
    if(!validSchemaName(canonicalName)) throw std::invalid_argument("Invalid schema directory name");
    mutateConfiguration(path,[&](std::u16string_view text) {return selectedSchemaText(text,canonicalName);});
}
void selectSchemaGeneration(const std::filesystem::path& directory,std::u16string_view generation) {
    if(generation!=u"legacy" && !validSchemaGeneration(generation)) throw std::invalid_argument("Invalid schema generation");
    mutateConfiguration(directory/L"current.txt",[&](std::u16string_view text) {
        return withConfigurationValue(text,u"generation",generation);
    },false); // Dedicated descriptor: replace malformed old text during recovery.
}
std::u16string switchRecentSchemaConfiguration(const std::filesystem::path& path,
    const std::vector<std::u16string>& schemas,const std::function<void(std::u16string_view)>& prepare) {
    std::u16string selected;
    mutateConfiguration(path,[&](std::u16string_view text) {
        selected=recentSchemaName(text,schemas);
        if(selected.empty()) return std::u16string(text);
        if(!validSchemaName(selected)) throw std::invalid_argument("Invalid schema directory name");
        prepare(selected);
        return selectedSchemaText(text,selected);
    });
    return selected;
}
}
