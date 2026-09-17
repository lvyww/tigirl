#pragma once
#include "SentenceRevision.h"
#include "SentenceImport.h"
#include "SchemaCatalog.h"
#include <algorithm>
#include <system_error>
#include <vector>
namespace tiger {
struct PreparedSentenceCache {std::filesystem::path path;std::u16string revision;};
namespace sentence_cache_detail {
constexpr std::size_t maximumRevisions=8;
inline bool revisionName(std::u16string_view name) {
    return name.size()==64 && std::all_of(name.begin(),name.end(),[](char16_t c) {
        return (c>=u'0' && c<=u'9') || (c>=u'a' && c<=u'f');
    });
}
struct ImportLock {
    HANDLE handle=INVALID_HANDLE_VALUE;OVERLAPPED offset{};bool locked=false;
    explicit ImportLock(const std::filesystem::path& path) {
        handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,
            FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle==INVALID_HANDLE_VALUE)return;
        locked=LockFileEx(handle,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,1,0,&offset)!=FALSE;
    }
    ~ImportLock(){if(locked)UnlockFileEx(handle,0,1,0,&offset);if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
    explicit operator bool()const{return locked;}
};
inline void prune(const std::filesystem::path& root,std::u16string_view current) noexcept {
    try {
        const auto rootAttributes=GetFileAttributesW(root.c_str());
        if(rootAttributes==INVALID_FILE_ATTRIBUTES || (rootAttributes&FILE_ATTRIBUTE_REPARSE_POINT))return;
        std::error_code error;
        if(!std::filesystem::is_directory(root,error) || error)return;
        struct Candidate {std::filesystem::path path;std::filesystem::file_time_type time;};
        std::vector<Candidate> candidates;
        std::filesystem::directory_iterator it(root,error),end;
        for(;!error && it!=end;it.increment(error)) {
            std::error_code statusError;
            const auto name=it->path().filename().u16string();
            if(!revisionName(name) || !it->is_directory(statusError) || statusError)continue;
            const auto attributes=GetFileAttributesW(it->path().c_str());
            if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))continue;
            const auto time=it->last_write_time(statusError);if(statusError)continue;
            candidates.push_back({it->path(),time});
        }
        std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){return a.time>b.time;});
        std::size_t retained=1; // Always reserve one slot for the active revision.
        for(const auto& candidate:candidates) {
            if(candidate.path.filename().u16string()==current)continue;
            if(retained<maximumRevisions){++retained;continue;}
            // The producer holds this byte lock while publishing. Readers map
            // immutable files with FILE_SHARE_DELETE, so old inactive revisions
            // can disappear from the namespace without invalidating them.
            ImportLock lock(candidate.path/L".import.lock");if(!lock)continue;
            std::error_code removal;std::filesystem::remove_all(candidate.path,removal);
        }
    }catch(...) { /* Cache GC is best effort and never disables sentence input. */ }
}
}
// Resource-worker operation. Full metadata rebuilding occurs in the helper.
inline PreparedSentenceCache prepareSentenceCache(const std::filesystem::path& helper,
    const std::filesystem::path& ordinary,const std::filesystem::path& journal,
    const std::filesystem::path& cacheRoot,const Lexicon& snapshot) {
    SentenceLexicon inventory(Dictionary::Open(sentenceLexiconPath(ordinary)));
    PreparedSentenceCache result{{},sentenceRevision(inventory,snapshot)};
    const auto directory=cacheRoot/std::filesystem::path(result.revision);
    auto ready=[&] {
        result.path=schemaDictionaryPath(directory);auto mapped=Dictionary::Open(result.path);
        SentenceLexicon checked(mapped);
        if(mapped->value(mapped->find(Section::Split,u"_sentence_revision"),0)!=result.revision)
            throw std::runtime_error("Sentence cache revision mismatch");
    };
    try{ready();sentence_cache_detail::prune(cacheRoot,result.revision);return result;}catch(const std::exception&){}
    auto quote=[](std::wstring_view argument) {
        std::wstring text=L"\"";std::size_t slashes=0;
        for(auto c:argument){if(c==L'\\'){++slashes;continue;}
            text.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;text+=c;}
        text.append(slashes*2,L'\\');text+=L'"';return text;
    };
    auto command=quote(helper.wstring());
    for(const auto& arg:std::vector<std::wstring>{L"--ensure-sentence",ordinary.wstring(),journal.wstring(),
        cacheRoot.wstring(),std::filesystem::path(result.revision).wstring()})command+=L" "+quote(arg);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,
        helper.parent_path().c_str(),&startup,&process))
        throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Launch sentence helper");
    struct Handle {HANDLE value;~Handle(){CloseHandle(value);}} child{process.hProcess},thread{process.hThread};
    const auto wait=WaitForSingleObject(child.value,30000);
    if(wait==WAIT_TIMEOUT) {
        DWORD code=STILL_ACTIVE;
        if(!GetExitCodeProcess(child.value,&code))
            throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Query timed-out sentence helper");
        if(code==STILL_ACTIVE) {
            if(!TerminateProcess(child.value,ERROR_TIMEOUT))
                throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Terminate timed-out sentence helper");
            if(WaitForSingleObject(child.value,5000)!=WAIT_OBJECT_0)
                throw std::runtime_error("Timed-out sentence helper could not be reaped");
        }
        throw std::runtime_error("Sentence helper exceeded resource-load timeout");
    }
    if(wait==WAIT_FAILED)
        throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Wait for sentence helper");
    DWORD code=1;if(wait!=WAIT_OBJECT_0 || !GetExitCodeProcess(child.value,&code) || code)
        throw std::runtime_error("Sentence helper failed");
    ready();sentence_cache_detail::prune(cacheRoot,result.revision);return result;
}
}
