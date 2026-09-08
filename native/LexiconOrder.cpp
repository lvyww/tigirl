#define NOMINMAX
#include <windows.h>
#include "LexiconOrder.h"
#include "OrdinalCase.h"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <system_error>
namespace tiger {
std::vector<std::filesystem::path> enumerateLexiconFiles(const std::filesystem::path& directory) {
    if(!std::filesystem::is_directory(directory)) throw std::runtime_error("Missing table directory");
    const auto root=std::filesystem::absolute(directory).lexically_normal();
    std::vector<std::filesystem::path> result;
    WIN32_FIND_DATAW data{};
    HANDLE handle=FindFirstFileExW((root/L"*").c_str(),FindExInfoBasic,&data,FindExSearchNameMatch,nullptr,0);
    if(handle==INVALID_HANDLE_VALUE) {
        auto error=GetLastError();
        if(error==ERROR_FILE_NOT_FOUND) return {};
        throw std::system_error(static_cast<int>(error),std::system_category(),"Enumerate table directory");
    }
    struct FindGuard {HANDLE value;~FindGuard(){FindClose(value);}} guard{handle};
    do {
        if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue;
        result.push_back(root/std::filesystem::path(data.cFileName));
    } while(FindNextFileW(handle,&data));
    auto error=GetLastError();
    if(error!=ERROR_NO_MORE_FILES) throw std::system_error(static_cast<int>(error),std::system_category(),"Enumerate table directory");
    return result;
}
std::vector<std::filesystem::path> orderedLexiconFiles(
    const std::filesystem::path& directory,const std::string& culture) {
    if(!std::filesystem::is_directory(directory)) throw std::runtime_error("Missing table directory");
    struct Row {std::filesystem::path path;std::u16string name;bool primary;};
    std::vector<Row> txt,yaml;
    const auto root=std::filesystem::absolute(directory).lexically_normal();
    auto schema=root.filename().u16string();
    if(schema.empty()) schema=root.parent_path().filename().u16string();
    auto primaryTxt=ordinalCaseKey(schema+u".txt"),primaryYaml=ordinalCaseKey(schema+u".dict.yaml");
    for(const auto& file:enumerateLexiconFiles(root)) {
        auto name=file.filename().u16string();auto folded=ordinalCaseKey(name);
        auto ends=[&](std::u16string_view suffix) {return folded.size()>=suffix.size() &&
            std::u16string_view(folded).substr(folded.size()-suffix.size())==suffix;};
        const bool primary=folded==primaryTxt || folded==primaryYaml;
        if(ends(u".TXT")) txt.push_back({file,std::move(name),primary});
        else if(ends(u".DICT.YAML")) yaml.push_back({file,std::move(name),primary});
    }
    txt.insert(txt.end(),std::make_move_iterator(yaml.begin()),std::make_move_iterator(yaml.end()));
    UErrorCode status=U_ZERO_ERROR;
    std::unique_ptr<UCollator,decltype(&ucol_close)> collator(ucol_open(culture.empty()?"root":culture.c_str(),&status),ucol_close);
    if(U_FAILURE(status) || !collator) throw std::runtime_error("Cannot create table collation");
    std::stable_sort(txt.begin(),txt.end(),[&](const Row& a,const Row& b) {
        if(a.primary!=b.primary) return a.primary;
        return ucol_strcoll(collator.get(),reinterpret_cast<const UChar*>(a.name.data()),static_cast<int32_t>(a.name.size()),
            reinterpret_cast<const UChar*>(b.name.data()),static_cast<int32_t>(b.name.size()))==UCOL_LESS;
    });
    std::vector<std::filesystem::path> result;result.reserve(txt.size());
    for(auto& row:txt) result.push_back(std::move(row.path));
    return result;
}
}
