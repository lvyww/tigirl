#include "UserStore.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <mutex>
#include <set>
#include "Text.h"
#include "EditableText.h"
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace tiger {
namespace {
constexpr char header[]=u8"# 用户调整：{添加}/{删除}/{置顶}/{前移}编码<Tab>词条，按行顺序执行\n";
constexpr std::size_t maxJournal=128*1024*1024, maxRecord=16*1024*1024;
using Bytes=std::vector<unsigned char>;
using FileStamp=std::array<std::uint64_t,4>;
std::filesystem::path coordinationPath(std::filesystem::path journal) {
    journal+=".lock";
    return journal;
}
void rejectMissingPublishedJournal(const std::filesystem::path& journal) {
    if(std::filesystem::exists(journal))return;
    const auto directory=std::filesystem::absolute(journal).parent_path();
    const auto prefix=journal.filename().native()+std::filesystem::path(".compact-").native();
    for(const auto& item:std::filesystem::directory_iterator(directory)) {
        if(item.path().filename().native().find(prefix)==0)
            throw std::runtime_error("User journal missing while checkpoint recovery files exist: "+journal.u8string());
    }
}
[[noreturn]] void systemFailure(const char* operation) {
#ifdef _WIN32
    throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),operation);
#else
    throw std::system_error(errno,std::generic_category(),operation);
#endif
}
FileStamp fileStamp(const std::filesystem::path& path) {
#ifdef _WIN32
    HANDLE handle=CreateFileW(path.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE) {
        const auto error=GetLastError();
        if(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND)return {};
        SetLastError(error);systemFailure("Stat user journal");
    }
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(handle,&info)) {
        const auto error=GetLastError();CloseHandle(handle);SetLastError(error);systemFailure("Stat user journal");
    }
    CloseHandle(handle);
    return {(static_cast<std::uint64_t>(info.dwVolumeSerialNumber)<<32)|info.nFileIndexHigh,
        info.nFileIndexLow,(static_cast<std::uint64_t>(info.nFileSizeHigh)<<32)|info.nFileSizeLow,
        (static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime};
#else
    struct stat info{};
    if(::stat(path.c_str(),&info)<0) {
        if(errno==ENOENT)return {};
        systemFailure("Stat user journal");
    }
    return {static_cast<std::uint64_t>(info.st_dev),static_cast<std::uint64_t>(info.st_ino),
        static_cast<std::uint64_t>(info.st_size),
        static_cast<std::uint64_t>(info.st_mtim.tv_sec)*1000000000ull+static_cast<std::uint64_t>(info.st_mtim.tv_nsec)};
#endif
}
class LockedFile {
public:
    explicit LockedFile(const std::filesystem::path& path, bool publication=false, bool existingOnly=false) {
#ifdef _WIN32
        handle_=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,
            publication?(FILE_SHARE_READ|FILE_SHARE_DELETE):(FILE_SHARE_READ|FILE_SHARE_WRITE),
            nullptr,(publication||existingOnly)?OPEN_EXISTING:OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle_==INVALID_HANDLE_VALUE) systemFailure("Open user journal");
        OVERLAPPED offset{};
        if(!LockFileEx(handle_,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&offset)) {
            const auto error=GetLastError(); CloseHandle(handle_); SetLastError(error); systemFailure("Lock user journal");
        }
#else
        if(publication)throw std::runtime_error("Exclusive journal publication requires Windows sharing semantics");
        handle_=::open(path.c_str(),O_RDWR|O_CLOEXEC|(existingOnly?0:O_CREAT),0600);
        if(handle_<0) systemFailure("Open user journal");
        int result;
        do { result=flock(handle_,LOCK_EX); } while(result<0 && errno==EINTR);
        if(result<0) { const int error=errno; ::close(handle_); errno=error; systemFailure("Lock user journal"); }
#endif
    }
    ~LockedFile() {
#ifdef _WIN32
        CloseHandle(handle_);
#else
        ::close(handle_);
#endif
    }
    LockedFile(const LockedFile&)=delete;
    LockedFile& operator=(const LockedFile&)=delete;
    Bytes read() {
        std::uint64_t length;
#ifdef _WIN32
        LARGE_INTEGER size{};
        if(!GetFileSizeEx(handle_,&size)) systemFailure("Size user journal");
        length=static_cast<std::uint64_t>(size.QuadPart);
#else
        struct stat size{};
        if(fstat(handle_,&size)<0) systemFailure("Size user journal");
        length=static_cast<std::uint64_t>(size.st_size);
#endif
        if(length>maxJournal) throw std::runtime_error("User journal exceeds size limit");
        Bytes bytes(static_cast<std::size_t>(length));
        std::size_t done=0;
        while(done<bytes.size()) {
#ifdef _WIN32
            DWORD count=0;
            if(!ReadFile(handle_,bytes.data()+done,static_cast<DWORD>(bytes.size()-done),&count,nullptr)) systemFailure("Read user journal");
#else
            const auto count=::read(handle_,bytes.data()+done,bytes.size()-done);
            if(count<0) { if(errno==EINTR) continue; systemFailure("Read user journal"); }
#endif
            if(!count) throw std::runtime_error("Unexpected end of user journal");
            done+=static_cast<std::size_t>(count);
        }
        return bytes;
    }
    FileStamp stamp() const {
#ifdef _WIN32
        BY_HANDLE_FILE_INFORMATION info{};
        if(!GetFileInformationByHandle(handle_,&info))systemFailure("Stat locked user journal");
        return {(static_cast<std::uint64_t>(info.dwVolumeSerialNumber)<<32)|info.nFileIndexHigh,
            info.nFileIndexLow,(static_cast<std::uint64_t>(info.nFileSizeHigh)<<32)|info.nFileSizeLow,
            (static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime};
#else
        struct stat info{};
        if(fstat(handle_,&info)<0)systemFailure("Stat locked user journal");
        return {static_cast<std::uint64_t>(info.st_dev),static_cast<std::uint64_t>(info.st_ino),
            static_cast<std::uint64_t>(info.st_size),
            static_cast<std::uint64_t>(info.st_mtim.tv_sec)*1000000000ull+static_cast<std::uint64_t>(info.st_mtim.tv_nsec)};
#endif
    }
    void append(std::size_t validLength,const Bytes& records) {
        if(validLength+records.size()>maxJournal) throw std::runtime_error("User journal exceeds size limit");
        try {
            truncate(validLength);
            write(records);
            flush();
        } catch(...) {
            // Keep the exclusive journal lock while removing every byte of
            // this failed batch. Replaying a partial Advance could reorder twice.
            const auto failure=std::current_exception();
            try { truncate(validLength); flush(); }
            catch(...) { throw std::runtime_error("User journal write and rollback failed; reload before retrying"); }
            std::rethrow_exception(failure);
        }
    }
private:
    void truncate(std::size_t length) {
#ifdef _WIN32
        LARGE_INTEGER position{}; position.QuadPart=static_cast<LONGLONG>(length);
#ifdef _M_ARM64EC
        // v145 emits a SetFilePointerEx LARGE_INTEGER-by-value exit thunk that
        // conflicts with the static UCRT (LNK4279). Pass the same 64-bit offset
        // as low/high parts to avoid that ABI boundary; retain full error checks.
        SetLastError(ERROR_SUCCESS);
        const auto low=SetFilePointer(handle_,static_cast<LONG>(position.LowPart),&position.HighPart,FILE_BEGIN);
        if((low==INVALID_SET_FILE_POINTER && GetLastError()!=ERROR_SUCCESS) || !SetEndOfFile(handle_)) systemFailure("Repair journal tail");
#else
        if(!SetFilePointerEx(handle_,position,nullptr,FILE_BEGIN) || !SetEndOfFile(handle_)) systemFailure("Repair journal tail");
#endif
#else
        if(ftruncate(handle_,static_cast<off_t>(length))<0 || lseek(handle_,static_cast<off_t>(length),SEEK_SET)<0)
            systemFailure("Repair journal tail");
#endif
    }
    void write(const Bytes& records) {
        std::size_t done=0;
        while(done<records.size()) {
#ifdef _WIN32
            DWORD count=0;
            if(!WriteFile(handle_,records.data()+done,static_cast<DWORD>(records.size()-done),&count,nullptr)) systemFailure("Write user journal");
#else
            const auto count=::write(handle_,records.data()+done,records.size()-done);
            if(count<0) { if(errno==EINTR) continue; systemFailure("Write user journal"); }
#endif
            if(!count) throw std::runtime_error("Zero-length journal write");
            done+=static_cast<std::size_t>(count);
        }
    }
    void flush() {
#ifdef _WIN32
        if(!FlushFileBuffers(handle_)) systemFailure("Flush user journal");
#else
        if(fsync(handle_)<0) systemFailure("Flush user journal");
#endif
    }
#ifdef _WIN32
    HANDLE handle_=INVALID_HANDLE_VALUE;
#else
    int handle_=-1;
#endif
};
std::vector<UserChange> textChanges(const Bytes& bytes) {
    std::string_view data(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    if(data.substr(0,3)=="\xef\xbb\xbf")data.remove_prefix(3);
    std::vector<UserChange> changes;std::size_t row=0;
    while(!data.empty()) {
        ++row;auto end=data.find('\n');auto line=data.substr(0,end);
        if(end==data.npos)data={};else data.remove_prefix(end+1);
        if(!line.empty() && line.back()=='\r')line.remove_suffix(1);
        if(line.empty() || line.front()=='#')continue;
        auto start=line.find_first_not_of(" \t");
        if(start==line.npos)continue;
        line.remove_prefix(start);
        if(line.front()=='#')continue;
        line=line.substr(0,line.find_last_not_of(" \t")+1);
        const char* actions[]={u8"{添加}",u8"{删除}",u8"{置顶}",u8"{前移}"};
        unsigned operation=0;
        for(;operation<4;++operation) {
            const std::string_view prefix=actions[operation];
            if(line.substr(0,prefix.size())==prefix){line.remove_prefix(prefix.size());break;}
        }
        if(operation==4)throw std::runtime_error("Invalid user action at row "+std::to_string(row));
        start=line.find_first_not_of(" \t");
        if(start!=line.npos)line.remove_prefix(start);
        auto first=line.find_first_of(" \t");
        auto second=first==line.npos?line.npos:line.find_first_not_of(" \t",first);
        if(second==line.npos || line.find_first_of(" \t",second)!=line.npos || line.size()>maxRecord)
            throw std::runtime_error("Invalid user text row "+std::to_string(row)+": expected {action}code and escaped text");
        const auto kind=static_cast<ChangeKind>(operation);
        auto code=editableValue(line.substr(0,first),true),text=editableValue(line.substr(second),true);
        if(code.empty() || text.empty())throw std::runtime_error("Empty user code/text at row "+std::to_string(row));
        changes.push_back({kind,std::move(code),std::move(text)});
    }
    return changes;
}
void encode(Bytes& records,const UserChange& change) {
    const char* actions[]={u8"{添加}",u8"{删除}",u8"{置顶}",u8"{前移}"};
    const auto kind=static_cast<unsigned>(change.kind);
    if(kind>=4 || change.code.empty() || change.text.empty())throw std::invalid_argument("Invalid user change");
    auto row=std::string(actions[kind])+editableField(change.code,true)+"\t"+editableField(change.text,true)+"\n";
    if(row.size()>maxRecord)throw std::invalid_argument("User entry exceeds record limit");
    records.insert(records.end(),row.begin(),row.end());
}
}
struct UserStore::Cache {
    std::mutex mutex;
    std::shared_ptr<const Lexicon> lexicon;
    FileStamp stamp{};
};
std::shared_ptr<Lexicon> UserStore::decode(const Bytes& bytes,std::shared_ptr<const Dictionary> dictionary,std::size_t& validLength) {
    auto lexicon=std::make_shared<Lexicon>(std::move(dictionary));
    for(const auto& change:textChanges(bytes))lexicon->applyChange(change);
    validLength=bytes.size();
    return lexicon;
}

UserStore::UserStore(std::shared_ptr<const Dictionary> dictionary,std::filesystem::path journal)
    :dictionary_(std::move(dictionary)),journal_(std::move(journal)),cache_(std::make_shared<Cache>()) {
    if(!dictionary_ || journal_.empty()) throw std::invalid_argument("UserStore requires dictionary and journal path");
    if(!journal_.parent_path().empty()) std::filesystem::create_directories(journal_.parent_path());
}
std::shared_ptr<const Lexicon> UserStore::refresh() const {
    try {
        const auto observed=fileStamp(journal_);
        std::lock_guard<std::mutex> lock(cache_->mutex);
        if(cache_->lexicon && cache_->stamp==observed)return cache_->lexicon;
    }catch(const std::exception&) {
        // Metadata is only an optimization. Fall back to the authoritative locked replay.
    }
    return commit({});
}
std::vector<unsigned char> UserStore::checkpoint() const {
    LockedFile coordination(coordinationPath(journal_));
    rejectMissingPublishedJournal(journal_);
    LockedFile file(journal_);
    return makeCheckpoint(file.read());
}
std::vector<unsigned char> UserStore::makeCheckpoint(Bytes original) const {
    std::size_t validLength=0;
    const auto lexicon=decode(original,dictionary_,validLength);
    original.resize(validLength);
    Bytes records(header,header+sizeof(header)-1);
    std::set<std::u16string> retainedCodes;
    // Base keys already have their place in the source inventory. New keys
    // must be replayed in insertion order: sentence shortest-code ties depend
    // on it, even when a key temporarily has no candidates.
    std::set<std::u16string> added(lexicon->addedCodes_.begin(),lexicon->addedCodes_.end());
    std::vector<Lexicon::Edits::const_iterator> ordered;
    for(auto it=lexicon->edits_.begin();it!=lexicon->edits_.end();++it)
        if(!added.count(it->first))ordered.push_back(it);
    for(const auto& code:lexicon->addedCodes_)ordered.push_back(lexicon->edits_.find(code));
    for(const auto it:ordered) {
        const auto& edit=*it;
        // Add deduplicates by commit identity. A retained duplicate inherited
        // from the base cannot be reconstructed using Add, so preserve the
        // original history in that case instead of silently dropping it.
        std::set<std::u16string> identities;
        for(const auto& value:*edit.second)
            if(!identities.emplace(commitText(value)).second)retainedCodes.insert(edit.first);
        if(retainedCodes.count(edit.first))continue;
        const auto base=dictionary_->find(Section::Main,edit.first);
        for(std::uint32_t i=0;i<base.count;++i)
            encode(records,{ChangeKind::Delete,edit.first,std::u16string(dictionary_->value(base,i))});
        for(const auto& value:*edit.second)encode(records,{ChangeKind::Add,edit.first,value});
        if(base.count==0 && edit.second->empty()) {
            // Exact keys survive deletion of their final word in Core. Keep
            // that inventory entry, including its quick-symbol consequences.
            encode(records,{ChangeKind::Add,edit.first,u"checkpoint"});
            encode(records,{ChangeKind::Delete,edit.first,u"checkpoint"});
        }
        if(records.size()>maxJournal)return original;
    }
    // Duplicate identities can only be inherited from base keys (Add itself
    // deduplicates); replaying their history here cannot reorder new codes.
    // Preserve records whose duplicate identities cannot be rebuilt via Add.
    // decode() above has already validated all text rows.
    if(!retainedCodes.empty()) {
        for(const auto& change:textChanges(original))
            if(retainedCodes.count(normalizeCode(change.code)))encode(records,change);
        if(records.size()>maxJournal)return original;
    }
    std::size_t checkedLength=0;
    const auto checked=decode(records,dictionary_,checkedLength);
    if(checkedLength!=records.size() || !checked->equivalent(*lexicon) ||
        checked->quickSymbols()!=lexicon->quickSymbols())
        throw std::runtime_error("User checkpoint changed lexicon semantics");
    return records.size()<original.size()?records:original;
}
#ifdef _WIN32
bool UserStore::restoreCheckpoint(const std::filesystem::path& backup) const {
    LockedFile coordination(coordinationPath(journal_));
    if(std::filesystem::exists(journal_))return false;
    const auto directory=std::filesystem::absolute(journal_).parent_path();
    const auto prefix=journal_.filename().native()+L".compact-";
    if(!std::filesystem::exists(backup) || backup.extension()!=L".old" ||
        backup.filename().native().find(prefix)!=0 ||
        !std::filesystem::equivalent(std::filesystem::absolute(backup).parent_path(),directory))
        throw std::invalid_argument("Checkpoint backup does not belong to this journal");
    LockedFile source(backup,false,true);
    const auto bytes=source.read();
    std::size_t validLength=0;
    decode(bytes,dictionary_,validLength);
    if(validLength!=bytes.size())
        throw std::runtime_error("Checkpoint backup is incomplete");
    wchar_t temporary[MAX_PATH]{};
    if(!GetTempFileNameW(directory.c_str(),L"tcu",0,temporary))systemFailure("Create checkpoint recovery staging");
    try {
        {LockedFile output(temporary);output.append(0,bytes);}
        // No REPLACE_EXISTING: a legacy writer can recreate the path without
        // observing the sidecar. In that race, preserve its file and the backup.
        if(!MoveFileExW(temporary,journal_.c_str(),MOVEFILE_WRITE_THROUGH)) {
            const auto error=GetLastError();
            if(error==ERROR_ALREADY_EXISTS || error==ERROR_FILE_EXISTS) {DeleteFileW(temporary);return false;}
            SetLastError(error);systemFailure("Restore user checkpoint");
        }
    }catch(...) {DeleteFileW(temporary);throw;}
    return true;
}
bool UserStore::compact() const {
    LockedFile coordination(coordinationPath(journal_));
    std::unique_ptr<LockedFile> guard;
    try {guard=std::make_unique<LockedFile>(journal_,true);}
    catch(const std::system_error& error) {
        if(error.code().value()==ERROR_SHARING_VIOLATION)return false;
        throw;
    }
    auto original=guard->read();
    const auto replacement=makeCheckpoint(original);
    if(replacement.size()>=original.size())return false;
    const auto directory=std::filesystem::absolute(journal_).parent_path();
    wchar_t temporary[MAX_PATH]{};
    if(!GetTempFileNameW(directory.c_str(),L"tcu",0,temporary))systemFailure("Create checkpoint staging file");
    const std::filesystem::path reserved(temporary);
    // Keep recovery artifacts attributable to their journal even when several
    // schemas share a user directory. Reserve without replacing any old file.
    auto name=journal_.filename().native()+L".compact-"+reserved.filename().native();
    const auto staging=directory/name;
    if(!MoveFileW(reserved.c_str(),staging.c_str())) {
        const auto error=GetLastError();DeleteFileW(reserved.c_str());SetLastError(error);
        systemFailure("Name checkpoint staging file");
    }
    auto backup=staging;backup+=L".old";
    // Never delete a pre-existing recovery artifact, even on a name collision.
    if(std::filesystem::exists(backup))throw std::runtime_error("Checkpoint backup collision");
    {
        LockedFile output(staging);
        output.append(0,replacement);
    }
    if(!ReplaceFileW(journal_.c_str(),staging.c_str(),backup.c_str(),0,nullptr,nullptr)) {
        const auto error=GetLastError();
        // ReplaceFile has documented partial-failure states. Keep both names
        // available for recovery rather than blindly deleting the staging file.
        throw std::system_error(static_cast<int>(error),std::system_category(),
            "Publish user checkpoint; preserve recovery artifacts at "+staging.u8string());
    }
    return true;
}
#endif
std::shared_ptr<const Lexicon> UserStore::commit(const std::vector<UserChange>& changes) const {
    std::lock_guard<std::mutex> local(cache_->mutex);
    // Keep this sidecar's identity stable across future journal replacement.
    // All operations acquire the coordination sidecar before the journal.
    LockedFile coordination(coordinationPath(journal_));
    rejectMissingPublishedJournal(journal_);
    LockedFile file(journal_);
    const auto bytes=file.read();
    std::size_t validLength=0;
    auto lexicon=decode(bytes,dictionary_,validLength);
    Bytes records;
    if(!changes.empty() && !validLength) records.insert(records.end(),header,header+sizeof(header)-1);
    if(!changes.empty() && !bytes.empty() && bytes.back()!='\n')records.push_back('\n');
    for(const auto& change:changes) {
        if(lexicon->applyChange(change))encode(records,change);
    }
    if(!records.empty()) file.append(validLength,records);
    try {
        cache_->stamp=file.stamp();
        cache_->lexicon=lexicon;
    }catch(const std::exception&) {
        cache_->stamp={};cache_->lexicon.reset();
    }
    return lexicon;
}
}
