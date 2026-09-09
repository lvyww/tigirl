#define NOMINMAX
#include <windows.h>
#include "LexiconDirectory.h"
#include "LexiconPublish.h"
#include "Settings.h"
#include "OrdinalCase.h"
#include "ConfigStore.h"
#include "SchemaCatalog.h"
#include "UserStore.h"
#include "SourceFingerprint.h"
#include "SentenceImport.h"
#include "SentenceRebuild.h"
#include "SentenceRevision.h"
#include "LexiconDecode.h"
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
        const bool revisionOnly=argc==4 && std::wstring_view(argv[1])==L"--sentence-revision";
        const bool sentenceCache=argc==6 && std::wstring_view(argv[1])==L"--ensure-sentence";
        if(revisionOnly || sentenceCache || ((argc==5 || argc==6) && std::wstring_view(argv[1])==L"--rebuild-sentence")) {
            // Explicit immutable output only. Selecting it for a live TSF
            // context requires a separate base/journal revision protocol.
            const std::filesystem::path ordinary(argv[2]),journal(argv[3]);
            auto output=revisionOnly?std::filesystem::path{}:std::filesystem::path(argv[4]);
            if(!ordinary.is_absolute() || !journal.is_absolute() || (!revisionOnly && !output.is_absolute()))
                throw std::runtime_error("Absolute sentence rebuild paths required");
            auto dictionary=tiger::Dictionary::Open(ordinary);
            tiger::SentenceLexicon inventory(tiger::Dictionary::Open(tiger::sentenceLexiconPath(ordinary)));
            tiger::UserStore store(dictionary,journal);
            const auto snapshot=store.refresh();
            const auto revision=tiger::sentenceRevision(inventory,*snapshot);
            std::string revisionText;for(auto c:revision)revisionText.push_back(static_cast<char>(c));
            if(revisionOnly){std::cout<<"{\"revision\":\""<<revisionText<<"\"}\n";return 0;}
            if(argc==6 && std::u16string_view(reinterpret_cast<const char16_t*>(argv[5]))!=revision)
                throw std::runtime_error("Sentence revision changed; discard stale rebuild request");
            SchemaImportLock cacheLock;std::filesystem::path cacheDirectory;std::u16string generation;
            auto cacheResult=[&](bool reused) {
                std::string name;for(auto c:generation)name.push_back(static_cast<char>(c));
                std::cout<<"{\"status\":\"sentence-ready\",\"revision\":\""<<revisionText
                    <<"\",\"generation\":\""<<name<<"\",\"reused\":"<<(reused?"true":"false")<<"}\n";
            };
            if(sentenceCache) {
                cacheDirectory=output/std::filesystem::path(revision);
                cacheLock.acquire(cacheDirectory);
                try {
                    const auto selected=tiger::schemaDictionaryPath(cacheDirectory);
                    auto mapping=tiger::Dictionary::Open(selected);
                    tiger::SentenceLexicon cached(mapping);
                    generation=selected.parent_path().filename().u16string();
                    if(!tiger::validSchemaGeneration(generation) ||
                       mapping->value(mapping->find(tiger::Section::Split,u"_sentence_revision"),0)!=revision)
                        throw std::runtime_error("Sentence cache revision mismatch");
                    cacheResult(true);return 0;
                }catch(const std::exception&){ /* Publish a fresh generation; retain damaged files. */ }
                GUID guid{};if(FAILED(CoCreateGuid(&guid)))throw std::runtime_error("Cannot name sentence generation");
                const auto bytes=reinterpret_cast<const unsigned char*>(&guid);
                constexpr char16_t hex[]=u"0123456789abcdef";generation.clear();
                for(std::size_t i=0;i<sizeof(guid);++i){generation+=hex[bytes[i]>>4];generation+=hex[bytes[i]&15];}
                output=tiger::schemaGenerationPath(cacheDirectory,generation);
            }
            auto rebuilt=tiger::rebuildSentenceLexicon(inventory,*snapshot);
            rebuilt.splits[u"_sentence_revision"]=revision;
            tiger::publishImportedLexicon(rebuilt,output);
            tiger::SentenceLexicon validated(tiger::Dictionary::Open(output));
            if(sentenceCache){tiger::selectSchemaGeneration(cacheDirectory,generation);cacheResult(false);return 0;}
            std::cout<<"{\"status\":\"sentence-rebuilt\",\"edited_codes\":"<<snapshot->editedCodes()
                <<",\"source_codes\":"<<validated.sourceCodeCount()<<",\"revision\":\""<<revisionText<<"\"}\n";
            return 0;
        }
        const bool ensureMode=argc==7 && std::wstring_view(argv[1])==L"--ensure";
        const bool updateMode=argc==7 && std::wstring_view(argv[1])==L"--update";
        const bool schemaMode=ensureMode || updateMode || (argc==7 && std::wstring_view(argv[1])==L"--schema");
        if(argc!=5 && !schemaMode) {std::cerr<<"Usage: lexicon_import <schema-dir> <pinyin-dir> <new-output.tcd> <culture>\n"
            "   or: lexicon_import <--schema|--update> <schema-dir> <pinyin-dir> <user-root> <schema-name> <culture>\n"
            "   or: lexicon_import --rebuild-sentence <base.tcd> <user.tcu> <new-sentence.tcd> [expected-revision]\n"
            "   or: lexicon_import --ensure-sentence <base.tcd> <user.tcu> <cache-root> <expected-revision>\n"
            "   or: lexicon_import --sentence-revision <base.tcd> <user.tcu>\n";return 2;}
        std::string culture;
        for(wchar_t c:std::wstring(argv[schemaMode?6:4])) {if(c>127) throw std::runtime_error("Culture must be an ASCII language tag");culture.push_back(static_cast<char>(c));}
        const std::filesystem::path schema(argv[schemaMode?2:1]),pinyin(argv[schemaMode?3:2]);
        std::filesystem::path output(argv[schemaMode?4:3]);
        SchemaImportLock importLock;
        std::filesystem::path schemaDirectory;
        std::u16string generation,fingerprint;
        std::u16string selectedName;
        if(schemaMode) {
            if(!output.is_absolute()) throw std::runtime_error("Absolute user root required");
            std::u16string name(reinterpret_cast<const char16_t*>(argv[5]));
            if(!tiger::validSchemaName(name) || (!ensureMode && tiger::ordinalCompareIgnoreCase(name,u"虎码字词")==0))
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
                        !entry.is_directory() || (!updateMode && !ensureMode && !std::filesystem::is_empty(entry.path())))
                        throw std::runtime_error("Schema already exists or contains recovery data");
                    name=entry.path().filename().u16string();
                }
            }
            schemaDirectory=schemas/std::filesystem::path(name);
            selectedName=name;
            if(ensureMode) {
                if(!schema.is_absolute() || !pinyin.is_absolute() || !std::filesystem::is_directory(schema))throw std::runtime_error("Missing source schema directory");
                fingerprint=sourceFingerprint(schema,pinyin,culture);
                const auto metadata=tiger::readConfiguration(schemaDirectory/L"source-cache.txt");
                if(tiger::configurationValue(metadata,u"fingerprint")==fingerprint) {
                    try {
                        auto path=tiger::schemaDictionaryPath(schemaDirectory);
                        if(tiger::configurationValue(metadata,u"dictionary")==path.u16string()) {
                            auto dictionary=tiger::Dictionary::Open(path);
                            tiger::validateSentenceSidecars(path);
                            tiger::UserStore store(dictionary,tiger::schemaJournalPath(std::filesystem::path(argv[4]),name));
                            auto validated=store.refresh();
                            std::cout<<"{\"published\":false,\"reused\":true}\n";return 0;
                        }
                    } catch(const std::exception&) { /* Rebuild an invalid or replaced cache. */ }
                }
            }
            if(updateMode || ensureMode) {
                if(!found && !ensureMode) throw std::runtime_error("Schema to update does not exist");
                if(!ensureMode){auto previous=tiger::Dictionary::Open(tiger::schemaDictionaryPath(schemaDirectory));}
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
        const auto supplement=schema/std::filesystem::path(u"补充语料.txt");
        tiger::publishSentenceSidecars(imported,std::filesystem::is_regular_file(supplement)?tiger::readLexiconText(supplement):std::u16string{},output);
        tiger::publishImportedLexicon(imported,output);
        if(ensureMode && sourceFingerprint(schema,pinyin,culture)!=fingerprint)
            throw std::runtime_error("Source changed while loading; please reload again");
        if(updateMode || ensureMode) {
            auto dictionary=tiger::Dictionary::Open(output);
            tiger::UserStore store(dictionary,tiger::schemaJournalPath(std::filesystem::path(argv[4]),selectedName));
            const auto validated=store.refresh();
            // Cache metadata may be ahead after a failed descriptor replacement;
            // reuse also checks the selected path. Publish the descriptor last so
            // a metadata write failure cannot change the active dictionary.
            if(ensureMode)tiger::updateConfigurationValues(schemaDirectory/L"source-cache.txt",
                {{u"fingerprint",fingerprint},{u"dictionary",output.u16string()},{u"source",schema.u16string()},{u"pinyin",pinyin.u16string()}});
            tiger::selectSchemaGeneration(schemaDirectory,generation);
        }
        std::cout<<"{\"published\":true,\"main_records\":"<<imported.indexedMain.size()<<",\"pinyin_records\":"<<imported.pinyin.size()<<"}\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
