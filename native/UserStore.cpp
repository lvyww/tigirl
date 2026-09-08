#include "UserStore.h"
#include <algorithm>
#include <cstring>
#include <exception>
#include <set>
#include "Text.h"
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
constexpr char magic[]="TIGERU01";
constexpr std::size_t maxJournal=128*1024*1024, maxRecord=16*1024*1024;
using Bytes=std::vector<unsigned char>;
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
void put32(Bytes& bytes,std::uint32_t value) {
    for(unsigned shift=0;shift<32;shift+=8) bytes.push_back(static_cast<unsigned char>(value>>shift));
}
std::uint32_t get32(const Bytes& bytes,std::size_t offset) {
    std::uint32_t value=0;
    for(unsigned i=0;i<4;++i) value|=static_cast<std::uint32_t>(bytes[offset+i])<<(i*8);
    return value;
}
std::uint32_t checksum(const unsigned char* data,std::size_t size) {
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=0;i<size;++i) {
        crc^=data[i];
        for(unsigned bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));
    }
    return ~crc;
}
void putString(Bytes& bytes,std::u16string_view text) {
    for(auto ch:text) { bytes.push_back(static_cast<unsigned char>(ch)); bytes.push_back(static_cast<unsigned char>(ch>>8)); }
}
std::u16string getString(const Bytes& bytes,std::size_t offset,std::uint32_t length) {
    std::u16string text;
    text.reserve(length);
    for(std::uint32_t i=0;i<length;++i,offset+=2) text+=static_cast<char16_t>(bytes[offset]|(bytes[offset+1]<<8));
    return text;
}
void encode(Bytes& records,const UserChange& change) {
    if(change.code.size()>maxRecord/2 || change.text.size()>maxRecord/2 ||
        12+(change.code.size()+change.text.size())*2>maxRecord) throw std::invalid_argument("User entry exceeds record limit");
    Bytes payload;
    put32(payload,static_cast<std::uint32_t>(change.kind));
    put32(payload,static_cast<std::uint32_t>(change.code.size()));
    put32(payload,static_cast<std::uint32_t>(change.text.size()));
    putString(payload,change.code); putString(payload,change.text);
    put32(records,static_cast<std::uint32_t>(payload.size()));
    put32(records,checksum(payload.data(),payload.size()));
    records.insert(records.end(),payload.begin(),payload.end());
}
}
std::shared_ptr<Lexicon> UserStore::decode(const Bytes& bytes,std::shared_ptr<const Dictionary> dictionary,std::size_t& validLength) {
    auto lexicon=std::make_shared<Lexicon>(std::move(dictionary));
    validLength=0;
    if(!bytes.empty() && std::memcmp(bytes.data(),magic,std::min<std::size_t>(bytes.size(),8))) throw std::runtime_error("Invalid user journal header");
    if(bytes.size()<8) return lexicon;
    validLength=8;
    while(bytes.size()-validLength>=8) {
        const auto length=get32(bytes,validLength);
        if(length<12 || length>maxRecord) throw std::runtime_error("Invalid user journal record length");
        if(bytes.size()-validLength-8<length) break; // interrupted final append
        const auto start=validLength+8;
        if(checksum(bytes.data()+start,length)!=get32(bytes,validLength+4)) throw std::runtime_error("User journal checksum mismatch");
        const auto kind=get32(bytes,start), codeLength=get32(bytes,start+4), textLength=get32(bytes,start+8);
        if(kind>3 || 12+(static_cast<std::uint64_t>(codeLength)+textLength)*2!=length)
            throw std::runtime_error("Invalid user journal entry");
        lexicon->applyChange({static_cast<ChangeKind>(kind),getString(bytes,start+12,codeLength),getString(bytes,start+12+static_cast<std::size_t>(codeLength)*2,textLength)});
        validLength=start+length;
    }
    return lexicon;
}
UserStore::UserStore(std::shared_ptr<const Dictionary> dictionary,std::filesystem::path journal)
    :dictionary_(std::move(dictionary)),journal_(std::move(journal)) {
    if(!dictionary_ || journal_.empty()) throw std::invalid_argument("UserStore requires dictionary and journal path");
    if(!journal_.parent_path().empty()) std::filesystem::create_directories(journal_.parent_path());
}
std::shared_ptr<const Lexicon> UserStore::refresh() const { return commit({}); }
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
    Bytes records(magic,magic+8);
    std::set<std::u16string> retainedCodes;
    for(const auto& edit:lexicon->edits_) {
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
    // Operations on distinct codes commute. Preserve original byte records
    // only for codes whose duplicate identities cannot be rebuilt via Add.
    // decode() above has already validated all record lengths and checksums.
    if(!retainedCodes.empty()) {
        for(std::size_t offset=8;offset<validLength;) {
            const auto length=get32(original,offset);
            const auto code=getString(original,offset+20,get32(original,offset+12));
            if(retainedCodes.count(normalizeCode(code)))
                records.insert(records.end(),original.begin()+offset,original.begin()+offset+8+length);
            offset+=8+length;
        }
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
    if(validLength<8 || validLength!=bytes.size())
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
    // Keep this sidecar's identity stable across future journal replacement.
    // Retain the journal lock as well while older installed clients still use
    // that lock alone. All new operations acquire sidecar before journal.
    LockedFile coordination(coordinationPath(journal_));
    rejectMissingPublishedJournal(journal_);
    LockedFile file(journal_);
    const auto bytes=file.read();
    std::size_t validLength=0;
    auto lexicon=decode(bytes,dictionary_,validLength);
    Bytes records;
    if(!changes.empty() && !validLength) records.insert(records.end(),magic,magic+8);
    for(const auto& change:changes) {
        if(lexicon->applyChange(change))encode(records,change);
    }
    if(!records.empty()) file.append(validLength,records);
    return lexicon;
}
}
