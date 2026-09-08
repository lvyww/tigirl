#include "Dictionary.h"
#include "LexiconSerialize.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

int run(const std::filesystem::path& source,const std::filesystem::path& output) {
    const auto dictionary=tiger::Dictionary::Open(source);
    tiger::ImportedLexicon lexicon;
    lexicon.quickSymbols=dictionary->quickSymbols();
    for(unsigned s=1;s<=6;++s) {
        const auto section=static_cast<tiger::Section>(s);
        for(std::uint32_t i=0;i<dictionary->count(section);++i) {
            const auto entry=dictionary->at(section,i);
            tiger::ImportedLexiconEntry row{std::u16string(entry.key),{}};
            for(std::uint32_t v=0;v<entry.count;++v) row.candidates.emplace_back(dictionary->value(entry,v));
            if(s==1) {
                std::size_t index=static_cast<std::size_t>(-1);
                if((entry.flags&8)!=0 || entry.count) {index=lexicon.main.size();lexicon.main.push_back(std::move(row));}
                lexicon.indexedMain.push_back({std::u16string(entry.key),entry.flags,index});
            } else if(s==2) lexicon.pinyin.push_back(std::move(row));
            else {
                if(row.candidates.size()!=1) throw std::runtime_error("Expected singleton map");
                auto& map=s==3?lexicon.comments:s==4?lexicon.splits:s==5?lexicon.fullCodes:lexicon.construct;
                map.emplace(row.code,row.candidates.front());
            }
        }
    }
    // Serializer must independently sort; pipeline encounter order is significant
    // for construction but is not the on-disk record order.
    std::reverse(lexicon.indexedMain.begin(),lexicon.indexedMain.end());
    std::reverse(lexicon.pinyin.begin(),lexicon.pinyin.end());
    const auto bytes=tiger::serializeImportedLexicon(lexicon);
    if(std::filesystem::exists(output)) throw std::runtime_error("Probe output exists");
    std::ofstream stream(output,std::ios::binary);
    stream.exceptions(std::ios::badbit|std::ios::failbit);
    stream.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    stream.close();
    const auto reread=tiger::Dictionary::Open(output);
    for(unsigned s=1;s<=6;++s) {
        const auto section=static_cast<tiger::Section>(s);
        if(dictionary->count(section)!=reread->count(section)) throw std::runtime_error("Count changed");
        for(std::uint32_t i=0;i<dictionary->count(section);++i) {
            auto a=dictionary->at(section,i),b=reread->at(section,i);
            if(a.key!=b.key || a.flags!=b.flags || a.count!=b.count) throw std::runtime_error("Record changed");
            for(std::uint32_t v=0;v<a.count;++v)
                if(dictionary->value(a,v)!=reread->value(b,v)) throw std::runtime_error("Value changed");
        }
    }
    unsigned rejected=0;
    auto reject=[&](const tiger::ImportedLexicon& bad) {
        try {tiger::serializeImportedLexicon(bad);} catch(const std::invalid_argument&) {++rejected;return;}
        throw std::runtime_error("Invalid serializer input accepted");
    };
    reject({});
    lexicon.quickSymbols|=16;reject(lexicon);lexicon.quickSymbols&=15;
    const auto saved=lexicon.indexedMain.front();
    lexicon.indexedMain.front().flags|=16;reject(lexicon);lexicon.indexedMain.front()=saved;
    lexicon.indexedMain.front().sourceIndex=lexicon.main.size();reject(lexicon);lexicon.indexedMain.front()=saved;
    lexicon.indexedMain.push_back(saved);reject(lexicon);
    std::cout<<"{\"bytes\":"<<bytes.size()<<",\"invalid_rejected\":"<<rejected<<"}\n";
    return 0;
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
#else
int main(int argc,char** argv) {
#endif
    try {if(argc!=3) return 2;return run(argv[1],argv[2]);}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
