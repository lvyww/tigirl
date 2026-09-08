// Short-lived native utility. Loading validates the target before publishing it;
// typing continues inside TSF with no helper process or per-key IPC.
#define NOMINMAX
#include <windows.h>
#include "ConfigStore.h"
#include "Settings.h"
#include "SchemaCatalog.h"
#include "OrdinalCase.h"
#include "UserStore.h"
#include "Text.h"
#include <iostream>
#include <stdexcept>
int wmain(int argc,wchar_t** argv) {
    try {
        const bool versions=argc==5 && std::wstring_view(argv[3])==L"--versions";
        const bool restore=argc==6 && std::wstring_view(argv[3])==L"--restore";
        const bool compactUser=argc==5 && std::wstring_view(argv[3])==L"--compact-user";
        const bool recoverUser=argc==6 && std::wstring_view(argv[3])==L"--recover-user";
        const bool maintenance=compactUser || recoverUser;
        if(argc!=4 && !versions && !restore && !maintenance) throw std::runtime_error("schema_select <user-root> <bundled-dictionary> <schema|--recent|--list|--versions schema|--restore schema generation|--compact-user schema|--recover-user schema backup>");
        const std::filesystem::path root(argv[1]);
        const std::filesystem::path bundled(argv[2]);
        std::u16string name(reinterpret_cast<const char16_t*>(argv[3]));
        if(versions || restore || maintenance) name=reinterpret_cast<const char16_t*>(argv[4]);
        if(!root.is_absolute() || !bundled.is_absolute()) throw std::invalid_argument("Absolute paths are required");
        if(!versions && !restore && !maintenance && name==u"--list") {
            for(const auto& item:tiger::schemaNames(root)) std::cout<<tiger::utf8(item)<<'\n';
            return 0;
        }
        std::shared_ptr<const tiger::Lexicon> validated;
        auto prepare=[&](std::u16string_view target) {
            const bool builtin=target==u"虎码字词";
            const auto directory=root/L"schemas"/std::filesystem::path(target);
            auto dictionary=tiger::Dictionary::Open(builtin?bundled:tiger::schemaDictionaryPath(directory));
            tiger::UserStore store(dictionary,builtin?root/L"user"/L"tiger-words.tcu":directory/L"user.tcu");
            validated=store.refresh();
        };
        if(!versions && !restore && !maintenance && name==u"--recent") {
            name=tiger::switchRecentSchemaConfiguration(root/L"config.txt",tiger::schemaNames(root),prepare);
            std::cout<<tiger::utf8(name)<<'\n';return 0;
        }
        if(!tiger::validSchemaName(name)) throw std::invalid_argument("Invalid schema directory name");
        const bool builtin=name==u"虎码字词";
        if(builtin && (versions || restore)) throw std::runtime_error("Builtin schema has no user generations");
        if(!builtin) {
            bool found=false;
            for(const auto& entry:std::filesystem::directory_iterator(root/L"schemas")) {
                const auto actual=entry.path().filename().u16string();
                if(entry.is_directory() && tiger::ordinalCompareIgnoreCase(name,actual)==0) {
                    name=actual;found=true;break;
                }
            }
            if(!found) throw std::runtime_error("Schema directory not found");
        }
        if(maintenance) {
            const auto directory=root/L"schemas"/std::filesystem::path(name);
            auto dictionary=tiger::Dictionary::Open(builtin?bundled:tiger::schemaDictionaryPath(directory));
            tiger::UserStore store(dictionary,builtin?root/L"user"/L"tiger-words.tcu":directory/L"user.tcu");
            bool changed=false;
            if(compactUser)changed=store.compact();
            else {
                const std::filesystem::path backup(argv[5]);
                if(!backup.is_absolute())throw std::invalid_argument("Absolute backup path is required");
                changed=store.restoreCheckpoint(backup);
            }
            std::cout<<"{\"operation\":\""<<(compactUser?"compact-user":"recover-user")<<"\",\"changed\":"<<(changed?"true":"false")<<"}\n";
            return 0;
        }
        if(versions || restore) {
            const auto directory=root/L"schemas"/std::filesystem::path(name);
            if(versions) {
                for(const auto& generation:tiger::schemaGenerations(directory)) std::cout<<tiger::utf8(generation)<<'\n';
                return 0;
            }
            const std::u16string generation(reinterpret_cast<const char16_t*>(argv[5]));
            auto dictionary=tiger::Dictionary::Open(tiger::schemaGenerationPath(directory,generation));
            tiger::UserStore store(dictionary,directory/L"user.tcu");
            validated=store.refresh();
            tiger::selectSchemaGeneration(directory,generation);
            std::cout<<tiger::utf8(generation)<<'\n';return 0;
        }
        prepare(name);
        // Keep the validated mapping alive until configuration publication finishes.
        tiger::selectSchemaConfiguration(root/L"config.txt",name);
        std::cout<<tiger::utf8(name)<<'\n';
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
