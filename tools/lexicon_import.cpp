#define NOMINMAX
#include <windows.h>
#include "LexiconDirectory.h"
#include "LexiconPublish.h"
#include "Settings.h"
#include "OrdinalCase.h"
#include "ConfigStore.h"
#include "SchemaCatalog.h"
#include "UserStore.h"
#include <objbase.h>
#include <iostream>
namespace {
struct SchemaImportLock {
    HANDLE handle=INVALID_HANDLE_VALUE;
    ~SchemaImportLock(){if(handle!=INVALID_HANDLE_VALUE) CloseHandle(handle);}
    void acquire(const std::filesystem::path& directory) {
        std::filesystem::create_directories(directory);
        handle=CreateFileW((directory/L".import.lock").c_str(),GENERIC_READ|GENERIC_WRITE,
            FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open schema import lock");
        OVERLAPPED position{};
        if(!LockFileEx(handle,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&position)) throw std::runtime_error("Cannot lock schema imports");
    }
};
}
int wmain(int argc,wchar_t** argv) {
    try {
        const bool updateMode=argc==7 && std::wstring_view(argv[1])==L"--update";
        const bool schemaMode=updateMode || (argc==7 && std::wstring_view(argv[1])==L"--schema");
        if(argc!=5 && !schemaMode) {std::cerr<<"Usage: lexicon_import <schema-dir> <pinyin-dir> <new-output.tcd> <culture>\n"
            "   or: lexicon_import <--schema|--update> <schema-dir> <pinyin-dir> <user-root> <schema-name> <culture>\n";return 2;}
        std::string culture;
        for(wchar_t c:std::wstring(argv[schemaMode?6:4])) {if(c>127) throw std::runtime_error("Culture must be an ASCII language tag");culture.push_back(static_cast<char>(c));}
        const std::filesystem::path schema(argv[schemaMode?2:1]),pinyin(argv[schemaMode?3:2]);
        std::filesystem::path output(argv[schemaMode?4:3]);
        SchemaImportLock importLock;
        std::filesystem::path schemaDirectory;
        std::u16string generation;
        if(schemaMode) {
            if(!output.is_absolute()) throw std::runtime_error("Absolute user root required");
            std::u16string name(reinterpret_cast<const char16_t*>(argv[5]));
            if(!tiger::validSchemaName(name) || tiger::ordinalCompareIgnoreCase(name,u"虎码字词")==0)
                throw std::runtime_error("Invalid or reserved schema name");
            const auto schemas=output/L"schemas";
            importLock.acquire(schemas);
            bool found=false;
            if(std::filesystem::is_directory(schemas)) for(const auto& entry:std::filesystem::directory_iterator(schemas)) {
                if(tiger::ordinalCompareIgnoreCase(name,entry.path().filename().u16string())==0) {
                    found=true;
                    // A failed publish can leave its newly-created directory empty.
                    // Reuse only an empty ordinary directory, never discard user
                    // journals, unknown files, temporary recovery data or links.
                    const auto attributes=GetFileAttributesW(entry.path().c_str());
                    if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT) ||
                        !entry.is_directory() || (!updateMode && !std::filesystem::is_empty(entry.path())))
                        throw std::runtime_error("Schema already exists or contains recovery data");
                    name=entry.path().filename().u16string();
                }
            }
            schemaDirectory=schemas/std::filesystem::path(name);
            if(updateMode) {
                if(!found) throw std::runtime_error("Schema to update does not exist");
                auto previous=tiger::Dictionary::Open(tiger::schemaDictionaryPath(schemaDirectory));
                GUID guid{};
                if(FAILED(CoCreateGuid(&guid))) throw std::runtime_error("Cannot create generation identifier");
                const auto bytes=reinterpret_cast<const unsigned char*>(&guid);
                constexpr char16_t hex[]=u"0123456789abcdef";
                for(std::size_t i=0;i<sizeof(guid);++i) {generation+=hex[bytes[i]>>4];generation+=hex[bytes[i]&15];}
                output=schemaDirectory/L"generations"/std::filesystem::path(generation)/L"tiger-v2.tcd";
            } else output=schemaDirectory/L"tiger-v2.tcd";
        }
        if(!schema.is_absolute() || !pinyin.is_absolute()) throw std::runtime_error("Absolute input directories required");
        auto imported=tiger::importLexiconDirectory(schema,pinyin,culture);
        tiger::publishImportedLexicon(imported,output);
        if(updateMode) {
            auto dictionary=tiger::Dictionary::Open(output);
            tiger::UserStore store(dictionary,schemaDirectory/L"user.tcu");
            const auto validated=store.refresh();
            tiger::selectSchemaGeneration(schemaDirectory,generation);
        }
        std::cout<<"{\"published\":true,\"main_records\":"<<imported.indexedMain.size()<<",\"pinyin_records\":"<<imported.pinyin.size()<<"}\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
