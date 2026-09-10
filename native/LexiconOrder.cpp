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
    // Use Windows NLS collation, available on every supported OS. Primary
    // files and equal-name enumeration order retain their existing precedence.
    std::wstring locale(culture.begin(),culture.end());
    std::replace(locale.begin(),locale.end(),L'_',L'-');
    if(!locale.empty() && !IsValidLocaleName(locale.c_str()))
        throw std::runtime_error("Invalid table collation locale");
    std::stable_sort(txt.begin(),txt.end(),[&](const Row& a,const Row& b) {
        if(a.primary!=b.primary) return a.primary;
        const auto order=CompareStringEx(locale.empty()?LOCALE_NAME_INVARIANT:locale.c_str(),0,
            reinterpret_cast<LPCWCH>(a.name.data()),static_cast<int>(a.name.size()),
            reinterpret_cast<LPCWCH>(b.name.data()),static_cast<int>(b.name.size()),nullptr,nullptr,0);
        if(!order)throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Compare table filenames");
        return order==CSTR_LESS_THAN;
    });
    std::vector<std::filesystem::path> result;result.reserve(txt.size());
    for(auto& row:txt) result.push_back(std::move(row.path));
    return result;
}
}
