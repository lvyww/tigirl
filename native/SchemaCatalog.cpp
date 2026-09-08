#define NOMINMAX
#include <windows.h>
#include "SchemaCatalog.h"
#include "Settings.h"
#include "OrdinalCase.h"
#include "ConfigStore.h"
#include "Dictionary.h"
#include <stdexcept>
#include <algorithm>
namespace tiger {
namespace {
int compare(std::u16string_view a,std::u16string_view b) {
    const auto result=ordinalCompareIgnoreCase(a,b);
    return result<0?CSTR_LESS_THAN:result>0?CSTR_GREATER_THAN:CSTR_EQUAL;
}
}
bool validSchemaGeneration(std::u16string_view generation) {
    return generation.size()==32 && std::all_of(generation.begin(),generation.end(),[](char16_t c) {
        return (c>=u'0' && c<=u'9') || (c>=u'a' && c<=u'f');
    });
}
std::filesystem::path schemaDictionaryPath(const std::filesystem::path& directory) {
    const auto descriptor=directory/L"current.txt";
    const auto text=readConfiguration(descriptor);
    if(text.empty() && !std::filesystem::exists(descriptor)) return directory/L"tiger-v2.tcd";
    const auto generation=configurationValue(text,u"generation");
    return schemaGenerationPath(directory,generation);
}
std::filesystem::path schemaGenerationPath(const std::filesystem::path& directory,std::u16string_view generation) {
    if(generation==u"legacy") return directory/L"tiger-v2.tcd";
    if(!validSchemaGeneration(generation)) throw std::runtime_error("Invalid schema generation descriptor");
    return directory/L"generations"/std::filesystem::path(generation)/L"tiger-v2.tcd";
}
std::vector<std::u16string> schemaGenerations(const std::filesystem::path& directory) {
    std::vector<std::u16string> result;
    auto include=[&](std::u16string name) {
        try {auto validated=Dictionary::Open(schemaGenerationPath(directory,name));result.push_back(std::move(name));}
        catch(const std::exception&) { /* Do not offer incomplete or corrupt binaries. */ }
    };
    include(u"legacy");
    if(std::filesystem::is_directory(directory/L"generations"))
        for(const auto& entry:std::filesystem::directory_iterator(directory/L"generations")) {
            auto name=entry.path().filename().u16string();
            if(entry.is_directory() && validSchemaGeneration(name)) include(std::move(name));
        }
    std::sort(result.begin(),result.end());
    return result;
}
std::vector<std::u16string> schemaNames(const std::filesystem::path& userRoot) {
    std::vector<std::u16string> names{u"虎码字词"};
    const auto directory=userRoot/L"schemas";
    if(std::filesystem::exists(directory)) for(const auto& entry:std::filesystem::directory_iterator(directory)) {
        const auto name=entry.path().filename().u16string();
        if(validSchemaName(name) && entry.is_directory()) {
            try {if(std::filesystem::is_regular_file(schemaDictionaryPath(entry.path()))) names.push_back(name);}
            catch(const std::exception&) { /* Incomplete schemes are not selectable. */ }
        }
    }
    std::sort(names.begin(),names.end(),[](const auto& a,const auto& b){return compare(a,b)==CSTR_LESS_THAN;});
    names.erase(std::unique(names.begin(),names.end(),[](const auto& a,const auto& b){return compare(a,b)==CSTR_EQUAL;}),names.end());
    return names;
}
std::u16string recentSchemaName(std::u16string_view configuration,const std::vector<std::u16string>& schemas) {
    if(schemas.size()<2) return {};
    auto current=currentSchemaSetting(configuration);
    if(current.empty()) current=u"虎码字词";
    const auto raw=configurationValue(configuration,u"最近码表对");
    std::u16string_view remaining(raw);
    std::vector<std::u16string> seeded;
    while(!remaining.empty() && seeded.size()<2) {
        const auto end=remaining.find(u'|');
        auto name=configurationValue(u"x\t"+std::u16string(remaining.substr(0,end)),u"x");
        if(!name.empty() && std::none_of(seeded.begin(),seeded.end(),[&](const auto& other){return compare(name,other)==CSTR_EQUAL;}))
            seeded.push_back(std::move(name));
        if(end==remaining.npos) break;
        remaining.remove_prefix(end+1);
    }
    for(const auto& name:seeded) if(compare(current,name)!=CSTR_EQUAL) {
        auto found=std::find_if(schemas.begin(),schemas.end(),[&](const auto& s){return compare(name,s)==CSTR_EQUAL;});
        if(found!=schemas.end()) return *found;
    }
    const auto found=std::find_if(schemas.begin(),schemas.end(),[&](const auto& s){return compare(current,s)==CSTR_EQUAL;});
    return found==schemas.end() || found+1==schemas.end()?schemas.front():*(found+1);
}
}
