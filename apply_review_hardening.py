from pathlib import Path

ROOT = Path(__file__).resolve().parent


def replace(path: str, old: str, new: str) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one replacement, found {count}: {old[:80]!r}")
    target.write_text(text.replace(old, new), encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")


replace(
    "native/UserStore.h",
    """private:\n    std::vector<unsigned char> makeCheckpoint(std::vector<unsigned char> original) const;\n    static std::shared_ptr<Lexicon> decode(const std::vector<unsigned char>& bytes,\n        std::shared_ptr<const Dictionary> dictionary,std::size_t& validLength);\n    std::shared_ptr<const Dictionary> dictionary_;\n    std::filesystem::path journal_;\n""",
    """private:\n    struct Cache;\n    std::vector<unsigned char> makeCheckpoint(std::vector<unsigned char> original) const;\n    static std::shared_ptr<Lexicon> decode(const std::vector<unsigned char>& bytes,\n        std::shared_ptr<const Dictionary> dictionary,std::size_t& validLength);\n    std::shared_ptr<const Dictionary> dictionary_;\n    std::filesystem::path journal_;\n    std::shared_ptr<Cache> cache_;\n""",
)

replace(
    "native/UserStore.cpp",
    """#include <algorithm>\n#include <cstring>\n#include <exception>\n#include <set>\n""",
    """#include <algorithm>\n#include <array>\n#include <cstring>\n#include <exception>\n#include <mutex>\n#include <set>\n""",
)
replace(
    "native/UserStore.cpp",
    "using Bytes=std::vector<unsigned char>;\n",
    "using Bytes=std::vector<unsigned char>;\nusing FileStamp=std::array<std::uint64_t,4>;\n",
)
replace(
    "native/UserStore.cpp",
    """class LockedFile {\n""",
    """FileStamp fileStamp(const std::filesystem::path& path) {\n#ifdef _WIN32\n    HANDLE handle=CreateFileW(path.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,\n        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);\n    if(handle==INVALID_HANDLE_VALUE) {\n        const auto error=GetLastError();\n        if(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND)return {};\n        SetLastError(error);systemFailure(\"Stat user journal\");\n    }\n    BY_HANDLE_FILE_INFORMATION info{};\n    if(!GetFileInformationByHandle(handle,&info)) {\n        const auto error=GetLastError();CloseHandle(handle);SetLastError(error);systemFailure(\"Stat user journal\");\n    }\n    CloseHandle(handle);\n    return {(static_cast<std::uint64_t>(info.dwVolumeSerialNumber)<<32)|info.nFileIndexHigh,\n        info.nFileIndexLow,(static_cast<std::uint64_t>(info.nFileSizeHigh)<<32)|info.nFileSizeLow,\n        (static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime};\n#else\n    struct stat info{};\n    if(::stat(path.c_str(),&info)<0) {\n        if(errno==ENOENT)return {};\n        systemFailure(\"Stat user journal\");\n    }\n    return {static_cast<std::uint64_t>(info.st_dev),static_cast<std::uint64_t>(info.st_ino),\n        static_cast<std::uint64_t>(info.st_size),\n        static_cast<std::uint64_t>(info.st_mtim.tv_sec)*1000000000ull+static_cast<std::uint64_t>(info.st_mtim.tv_nsec)};\n#endif\n}\nclass LockedFile {\n""",
)
replace(
    "native/UserStore.cpp",
    """    void append(std::size_t validLength,const Bytes& records) {\n""",
    """    FileStamp stamp() const {\n#ifdef _WIN32\n        BY_HANDLE_FILE_INFORMATION info{};\n        if(!GetFileInformationByHandle(handle_,&info))systemFailure(\"Stat locked user journal\");\n        return {(static_cast<std::uint64_t>(info.dwVolumeSerialNumber)<<32)|info.nFileIndexHigh,\n            info.nFileIndexLow,(static_cast<std::uint64_t>(info.nFileSizeHigh)<<32)|info.nFileSizeLow,\n            (static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime};\n#else\n        struct stat info{};\n        if(fstat(handle_,&info)<0)systemFailure(\"Stat locked user journal\");\n        return {static_cast<std::uint64_t>(info.st_dev),static_cast<std::uint64_t>(info.st_ino),\n            static_cast<std::uint64_t>(info.st_size),\n            static_cast<std::uint64_t>(info.st_mtim.tv_sec)*1000000000ull+static_cast<std::uint64_t>(info.st_mtim.tv_nsec)};\n#endif\n    }\n    void append(std::size_t validLength,const Bytes& records) {\n""",
)
replace(
    "native/UserStore.cpp",
    """}\n}\nstd::shared_ptr<Lexicon> UserStore::decode(const Bytes& bytes,std::shared_ptr<const Dictionary> dictionary,std::size_t& validLength) {\n""",
    """}\n}\nstruct UserStore::Cache {\n    std::mutex mutex;\n    std::shared_ptr<const Lexicon> lexicon;\n    FileStamp stamp{};\n};\nstd::shared_ptr<Lexicon> UserStore::decode(const Bytes& bytes,std::shared_ptr<const Dictionary> dictionary,std::size_t& validLength) {\n""",
)
replace(
    "native/UserStore.cpp",
    """UserStore::UserStore(std::shared_ptr<const Dictionary> dictionary,std::filesystem::path journal)\n    :dictionary_(std::move(dictionary)),journal_(std::move(journal)) {\n    if(!dictionary_ || journal_.empty()) throw std::invalid_argument(\"UserStore requires dictionary and journal path\");\n    if(!journal_.parent_path().empty()) std::filesystem::create_directories(journal_.parent_path());\n}\nstd::shared_ptr<const Lexicon> UserStore::refresh() const { return commit({}); }\n""",
    """UserStore::UserStore(std::shared_ptr<const Dictionary> dictionary,std::filesystem::path journal)\n    :dictionary_(std::move(dictionary)),journal_(std::move(journal)),cache_(std::make_shared<Cache>()) {\n    if(!dictionary_ || journal_.empty()) throw std::invalid_argument(\"UserStore requires dictionary and journal path\");\n    if(!journal_.parent_path().empty()) std::filesystem::create_directories(journal_.parent_path());\n}\nstd::shared_ptr<const Lexicon> UserStore::refresh() const {\n    try {\n        const auto observed=fileStamp(journal_);\n        std::lock_guard<std::mutex> lock(cache_->mutex);\n        if(cache_->lexicon && cache_->stamp==observed)return cache_->lexicon;\n    }catch(const std::exception&) {\n        // Metadata is only an optimization. Fall back to the authoritative locked replay.\n    }\n    return commit({});\n}\n""",
)
replace(
    "native/UserStore.cpp",
    """std::shared_ptr<const Lexicon> UserStore::commit(const std::vector<UserChange>& changes) const {\n    // Keep this sidecar's identity stable across future journal replacement.\n""",
    """std::shared_ptr<const Lexicon> UserStore::commit(const std::vector<UserChange>& changes) const {\n    std::lock_guard<std::mutex> local(cache_->mutex);\n    // Keep this sidecar's identity stable across future journal replacement.\n""",
)
replace(
    "native/UserStore.cpp",
    """    if(!records.empty()) file.append(validLength,records);\n    return lexicon;\n}\n""",
    """    if(!records.empty()) file.append(validLength,records);\n    try {\n        cache_->stamp=file.stamp();\n        cache_->lexicon=lexicon;\n    }catch(const std::exception&) {\n        cache_->stamp={};cache_->lexicon.reset();\n    }\n    return lexicon;\n}\n""",
)

replace(
    "native/SentenceNgram.cpp",
    """    if(position!=length_ || !indices_[0].count)invalid();\n    unknown_=lookup(indices_[0],0,0);\n""",
    """    if(position!=length_ || !indices_[0].count)invalid();\n    for(const auto& index:indices_) {\n        std::uint64_t previous=0;const std::uint64_t stride=index.wide?12:8;\n        for(std::uint64_t n=0;n<index.count;++n) {\n            const auto offset=index.offset+n*stride;\n            const auto key=index.wide?read<std::uint64_t>(offset):read<std::uint32_t>(offset);\n            const auto score=read<float>(offset+(index.wide?8:4));\n            // Binary search requires strict ordering; NaN/Inf or negative\n            // probabilities/backoff weights would also poison decoder ordering.\n            if((n && key<=previous) || !std::isfinite(score) || score<0)invalid();\n            previous=key;\n        }\n    }\n    unknown_=lookup(indices_[0],0,0);\n""",
)

replace(
    "native/SentenceCache.h",
    """#include \"SchemaCatalog.h\"\n#include <system_error>\nnamespace tiger {\nstruct PreparedSentenceCache {std::filesystem::path path;std::u16string revision;};\n""",
    """#include \"SchemaCatalog.h\"\n#include <algorithm>\n#include <system_error>\n#include <vector>\nnamespace tiger {\nstruct PreparedSentenceCache {std::filesystem::path path;std::u16string revision;};\nnamespace sentence_cache_detail {\nconstexpr std::size_t maximumRevisions=8;\ninline bool revisionName(std::u16string_view name) {\n    return name.size()==64 && std::all_of(name.begin(),name.end(),[](char16_t c) {\n        return (c>=u'0' && c<=u'9') || (c>=u'a' && c<=u'f');\n    });\n}\nstruct ImportLock {\n    HANDLE handle=INVALID_HANDLE_VALUE;OVERLAPPED offset{};bool locked=false;\n    explicit ImportLock(const std::filesystem::path& path) {\n        handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,\n            FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);\n        if(handle==INVALID_HANDLE_VALUE)return;\n        locked=LockFileEx(handle,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,1,0,&offset)!=FALSE;\n    }\n    ~ImportLock(){if(locked)UnlockFileEx(handle,0,1,0,&offset);if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}\n    explicit operator bool()const{return locked;}\n};\ninline void prune(const std::filesystem::path& root,std::u16string_view current) noexcept {\n    try {\n        const auto rootAttributes=GetFileAttributesW(root.c_str());\n        if(rootAttributes==INVALID_FILE_ATTRIBUTES || (rootAttributes&FILE_ATTRIBUTE_REPARSE_POINT))return;\n        std::error_code error;\n        if(!std::filesystem::is_directory(root,error) || error)return;\n        struct Candidate {std::filesystem::path path;std::filesystem::file_time_type time;};\n        std::vector<Candidate> candidates;\n        std::filesystem::directory_iterator it(root,error),end;\n        for(;!error && it!=end;it.increment(error)) {\n            std::error_code statusError;\n            const auto name=it->path().filename().u16string();\n            if(!revisionName(name) || !it->is_directory(statusError) || statusError)continue;\n            const auto attributes=GetFileAttributesW(it->path().c_str());\n            if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))continue;\n            const auto time=it->last_write_time(statusError);if(statusError)continue;\n            candidates.push_back({it->path(),time});\n        }\n        std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){return a.time>b.time;});\n        std::size_t retained=1; // Always reserve one slot for the active revision.\n        for(const auto& candidate:candidates) {\n            if(candidate.path.filename().u16string()==current)continue;\n            if(retained<maximumRevisions){++retained;continue;}\n            // The producer holds this byte lock while publishing. Readers map\n            // immutable files with FILE_SHARE_DELETE, so old inactive revisions\n            // can disappear from the namespace without invalidating them.\n            ImportLock lock(candidate.path/L\".import.lock\");if(!lock)continue;\n            std::error_code removal;std::filesystem::remove_all(candidate.path,removal);\n        }\n    }catch(...) { /* Cache GC is best effort and never disables sentence input. */ }\n}\n}\n""",
)
replace(
    "native/SentenceCache.h",
    "try{ready();return result;}catch(const std::exception&){}",
    "try{ready();sentence_cache_detail::prune(cacheRoot,result.revision);return result;}catch(const std::exception&){}",
)
replace(
    "native/SentenceCache.h",
    """    const auto wait=WaitForSingleObject(child.value,30000);DWORD code=1;\n    if(wait!=WAIT_OBJECT_0 || !GetExitCodeProcess(child.value,&code) || code)\n        throw std::runtime_error(\"Sentence helper failed or exceeded resource-load timeout\");\n    ready();return result;\n""",
    """    const auto wait=WaitForSingleObject(child.value,30000);\n    if(wait==WAIT_TIMEOUT) {\n        DWORD code=STILL_ACTIVE;\n        if(!GetExitCodeProcess(child.value,&code))\n            throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),\"Query timed-out sentence helper\");\n        if(code==STILL_ACTIVE) {\n            if(!TerminateProcess(child.value,ERROR_TIMEOUT))\n                throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),\"Terminate timed-out sentence helper\");\n            if(WaitForSingleObject(child.value,5000)!=WAIT_OBJECT_0)\n                throw std::runtime_error(\"Timed-out sentence helper could not be reaped\");\n        }\n        throw std::runtime_error(\"Sentence helper exceeded resource-load timeout\");\n    }\n    if(wait==WAIT_FAILED)\n        throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),\"Wait for sentence helper\");\n    DWORD code=1;if(wait!=WAIT_OBJECT_0 || !GetExitCodeProcess(child.value,&code) || code)\n        throw std::runtime_error(\"Sentence helper failed\");\n    ready();sentence_cache_detail::prune(cacheRoot,result.revision);return result;\n""",
)

replace(
    "SampleIME/Register.cpp",
    """    for each(GUID guid in SupportCategories)\n    {\n        hr = pCategoryMgr->RegisterCategory(Global::SampleIMECLSID, guid, Global::SampleIMECLSID);\n    }\n\n    pCategoryMgr->Release();\n\n    return (hr == S_OK);\n""",
    """    for each(GUID guid in SupportCategories)\n    {\n        hr = pCategoryMgr->RegisterCategory(Global::SampleIMECLSID, guid, Global::SampleIMECLSID);\n        if (FAILED(hr))\n        {\n            pCategoryMgr->Release();\n            return FALSE;\n        }\n    }\n\n    pCategoryMgr->Release();\n    return TRUE;\n""",
)
replace(
    "SampleIME/Register.cpp",
    "ITfCategoryMgr* pCategoryMgr = S_OK;",
    "ITfCategoryMgr* pCategoryMgr = nullptr;",
)

replace(
    "tests/run_core_tests.py",
    """COMMON_SOURCES = SELECTION_SOURCES[1:] + [\n    'native/CandidatePresentation.cpp', 'native/Settings.cpp', 'native/SelectionKeys.cpp',\n]\n""",
    """COMMON_SOURCES = SELECTION_SOURCES[1:] + [\n    'native/CandidatePresentation.cpp', 'native/Settings.cpp', 'native/SelectionKeys.cpp',\n    'native/UserStore.cpp', 'native/SentenceNgram.cpp',\n]\n""",
)
replace(
    "tests/run_core_tests.py",
    """    'candidate_reveal': ('tests/candidate_reveal_probe.cpp', False, []),\n    'code_mask': ('tests/code_mask_probe.cpp', False, []),\n}\n""",
    """    'candidate_reveal': ('tests/candidate_reveal_probe.cpp', False, []),\n    'code_mask': ('tests/code_mask_probe.cpp', False, []),\n    'user_store_refresh_cache': ('tests/user_store_refresh_cache_probe.cpp', False, ['fixture.tcd']),\n    'sentence_ngram_validation': ('tests/sentence_ngram_validation_probe.cpp', False, []),\n}\n""",
)

write(
    "tests/user_store_refresh_cache_probe.cpp",
    r'''#include "UserStore.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}

int main(int argc,char** argv) {
    if(argc!=2)return 2;
    using namespace tiger;
    const std::filesystem::path dictionaryPath(argv[1]);
    const auto journal=dictionaryPath.parent_path()/"refresh-cache.tcu";
    std::error_code ignored;std::filesystem::remove(journal,ignored);
    auto sidecar=journal;sidecar+=".lock";std::filesystem::remove(sidecar,ignored);
    auto dictionary=Dictionary::Open(dictionaryPath);
    UserStore store(dictionary,journal);
    auto first=store.refresh();
    auto second=store.refresh();
    check(first==second,"unchanged refresh replayed the journal");

    UserStore copy=store;
    auto committed=copy.commit({{ChangeKind::Add,u"c",u"缓存"}});
    check(store.refresh()==committed,"UserStore copies did not share the committed cache");

    UserStore external(dictionary,journal);
    auto externalView=external.commit({{ChangeKind::Add,u"d",u"外部"}});
    auto refreshed=store.refresh();
    check(refreshed!=committed,"external journal write was hidden by the refresh cache");
    auto match=refreshed->find(Section::Main,u"d");
    check(match.count==1 && refreshed->value(match,0)==u"外部","external write was not replayed");
    check(store.refresh()==refreshed,"post-reload unchanged refresh replayed the journal");

    auto externalMatch=externalView->find(Section::Main,u"d");
    check(externalMatch.count==1,"external writer did not persist its change");
    std::cout<<"PASS: UserStore refresh reuses unchanged snapshots and reloads external writes.\n";
    return 0;
}
''',
)

write(
    "tests/sentence_ngram_validation_probe.cpp",
    r'''#include "SentenceNgram.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using Narrow=std::vector<std::pair<std::uint32_t,float>>;
using Wide=std::vector<std::pair<std::uint64_t,float>>;
template<class T>void put(std::vector<unsigned char>& out,T value) {
    const auto* bytes=reinterpret_cast<const unsigned char*>(&value);out.insert(out.end(),bytes,bytes+sizeof(value));
}
void narrow(std::vector<unsigned char>& out,const Narrow& values) {
    put(out,static_cast<std::uint32_t>(values.size()));for(const auto& item:values){put(out,item.first);put(out,item.second);}
}
void wide(std::vector<unsigned char>& out,const Wide& values) {
    put(out,static_cast<std::uint64_t>(values.size()));for(const auto& item:values){put(out,item.first);put(out,item.second);}
}
void model(const std::filesystem::path& path,const Narrow& unigram,
    const Wide& bigram={},const Narrow& bigramBackoff={},const Wide& trigram={},const Wide& trigramBackoff={}) {
    std::vector<unsigned char> bytes{'T','C','S','K','N','M','0','1'};put(bytes,std::uint32_t{1});
    narrow(bytes,unigram);wide(bytes,bigram);narrow(bytes,bigramBackoff);wide(bytes,trigram);wide(bytes,trigramBackoff);
    std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!output)throw std::runtime_error("write model fixture");
}
void invalid(const std::filesystem::path& path,const Narrow& unigram,
    const Wide& bigram={},const Narrow& bigramBackoff={},const Wide& trigram={},const Wide& trigramBackoff={}) {
    model(path,unigram,bigram,bigramBackoff,trigram,trigramBackoff);
    bool rejected=false;
    try{auto loaded=tiger::SentenceNgram::Open(path);(void)loaded;}catch(const std::runtime_error&){rejected=true;}
    if(!rejected)throw std::runtime_error("invalid model accepted");
}
}

int main() {
    using namespace tiger;
    const auto root=std::filesystem::current_path();
    const auto valid=root/"ngram-valid.bin";
    model(valid,{{0,.1f},{0x4e2d,.2f}});
    auto loaded=SentenceNgram::Open(valid);
    if(!std::isfinite(loaded->logProbability(u"",u"",u"中",true)))throw std::runtime_error("valid model failed");

    invalid(root/"ngram-duplicate.bin",{{0,.1f},{0,.2f}});
    invalid(root/"ngram-unsorted.bin",{{0,.1f},{2,.2f},{1,.3f}});
    invalid(root/"ngram-nan.bin",{{0,.1f},{1,std::numeric_limits<float>::quiet_NaN()}});
    invalid(root/"ngram-negative.bin",{{0,.1f},{1,-.1f}});
    invalid(root/"ngram-infinite-backoff.bin",{{0,.1f}}, {},{{0,std::numeric_limits<float>::infinity()}});
    std::cout<<"PASS: sentence n-gram rejects unsorted and non-finite/negative records.\n";
    return 0;
}
''',
)

write(
    "tests/review_hardening_policy_test.py",
    '''from pathlib import Path\n\nROOT = Path(__file__).resolve().parents[1]\nuser_store = (ROOT / "native/UserStore.cpp").read_text(encoding="utf-8")\nngram = (ROOT / "native/SentenceNgram.cpp").read_text(encoding="utf-8")\ncache = (ROOT / "native/SentenceCache.h").read_text(encoding="utf-8")\nregistration = (ROOT / "SampleIME/Register.cpp").read_text(encoding="utf-8")\n\n\ndef require(value: bool, message: str) -> None:\n    if not value:\n        raise AssertionError(message)\n\n\nrequire("cache_->lexicon && cache_->stamp==observed" in user_store,\n        "unchanged UserStore refresh still replays its journal")\nrequire("cache_->stamp=file.stamp()" in user_store,\n        "successful user-store commits do not refresh the cache identity")\nrequire("!std::isfinite(score) || score<0" in ngram and "key<=previous" in ngram,\n        "n-gram model does not reject non-finite/negative or unsorted records")\nrequire("TerminateProcess(child.value,ERROR_TIMEOUT)" in cache and\n        "WaitForSingleObject(child.value,5000)" in cache,\n        "timed-out sentence helpers can survive the parent wait")\nrequire("maximumRevisions=8" in cache and "LOCKFILE_FAIL_IMMEDIATELY" in cache and\n        "remove_all(candidate.path" in cache,\n        "sentence revision cache does not have bounded best-effort GC")\n\nstart = registration.index("BOOL RegisterCategories()")\nend = registration.index("void UnregisterCategories()")\nblock = registration[start:end]\nrequire("if (FAILED(hr))" in block and "return FALSE" in block,\n        "RegisterCategories can still hide an earlier category failure")\n\nprint("PASS: review hardening policies are present.")\n''',
)

replace(
    ".github/workflows/build-and-test.yml",
    """          python tests/setup_resilience_policy_test.py\n          if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\n          if ($env:TIGIRL_PLATFORM -eq 'x64') {\n""",
    """          python tests/setup_resilience_policy_test.py\n          if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\n          python tests/review_hardening_policy_test.py\n          if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\n          if ($env:TIGIRL_PLATFORM -eq 'x64') {\n""",
)
replace(
    ".github/workflows/build-and-test.yml",
    """      - run: python tests/setup_resilience_policy_test.py\n      - run: python tests/bundled_schemas_test.py --verify-only\n""",
    """      - run: python tests/setup_resilience_policy_test.py\n      - run: python tests/review_hardening_policy_test.py\n      - run: python tests/bundled_schemas_test.py --verify-only\n""",
)

replace(
    "native/USERDATA.md",
    """to 128 MiB. Refresh currently replays the journal; compaction and notifications\nremain future lifecycle work. The TSF DLL now drains actual dispatch\nchanges into this store and refreshes on focus. The current journal is\n""",
    """to 128 MiB. `UserStore` now shares an in-process snapshot cache across its copies\nand keys it by the journal's file identity, size and last-write time. Focus/file\nreconciliation therefore returns the existing immutable `Lexicon` when the journal\nhas not changed, while an external append or checkpoint replacement still forces\na locked replay. Automatic compaction remains future lifecycle work. The TSF DLL\ndrains actual dispatch changes into this store and refreshes on focus. The current journal is\n""",
)

replace(
    "docs/SENTENCE_PERFORMANCE.md",
    """while preserving exact original candidate order, scores and prefix evidence.\n\nThe accepted scoring/state optimization adds a direct-mapped 4,096-entry cache\n""",
    """while preserving exact original candidate order, scores and prefix evidence.\n\nSentence overlay cache lifecycle is now bounded independently of decoder memory.\nAfter a sentence revision is opened or rebuilt, best-effort cleanup keeps the\nactive revision plus the seven newest inactive revision directories. A revision\nwhose `.import.lock` is currently held is skipped, and reparse-point directories\nare never traversed. If `Tigirl.Import.exe --ensure-sentence` exceeds the 30-second\nresource-load wait, the TSF worker terminates and reaps that helper before returning\nan error so abandoned helpers cannot accumulate behind the cache import lock.\n\nThe accepted scoring/state optimization adds a direct-mapped 4,096-entry cache\n""",
)

print("review hardening transform applied")
