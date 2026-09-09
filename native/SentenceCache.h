#pragma once
#include "SentenceRevision.h"
#include "SentenceImport.h"
#include "SchemaCatalog.h"
#include <system_error>
namespace tiger {
struct PreparedSentenceCache {std::filesystem::path path;std::u16string revision;};
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
    try{ready();return result;}catch(const std::exception&){}
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
    const auto wait=WaitForSingleObject(child.value,30000);DWORD code=1;
    if(wait!=WAIT_OBJECT_0 || !GetExitCodeProcess(child.value,&code) || code)
        throw std::runtime_error("Sentence helper failed or exceeded resource-load timeout");
    ready();return result;
}
}
