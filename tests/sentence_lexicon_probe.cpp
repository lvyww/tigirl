#include "../native/SentenceLexicon.h"
#include "../native/LexiconSerialize.h"
#include "../native/AddWord.h"
#include "../native/OrdinalCase.h"
#include <fstream>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string>
std::u16string token(const std::string& h){if(h=="-")return {};std::u16string t;for(std::size_t i=0;i<h.size();i+=4)t+=static_cast<char16_t>(std::stoul(h.substr(i,4),nullptr,16));return t;}
void hex(std::u16string_view t){constexpr char h[]="0123456789abcdef";std::cout<<'"';for(auto c:t)std::cout<<h[(c>>12)&15]<<h[(c>>8)&15]<<h[(c>>4)&15]<<h[c&15];std::cout<<'"';}
int run(const std::filesystem::path& input,const std::filesystem::path& output) {
 try {
    std::ifstream f(input);if(!f)throw std::runtime_error("Missing fixture");
    std::vector<tiger::ImportedLexiconEntry> source;std::vector<std::u16string> queries;
    tiger::SentenceLexicon::Characters common,white;std::string line;int id=0;std::cout<<std::setprecision(17);
    while(std::getline(f,line)) {
        std::istringstream row(line);std::string type,v;row>>type;
        if(type=="B"){row>>id;source.clear();queries.clear();common.clear();white.clear();}
        if(type=="C" || type=="W")while(row>>v)(type=="C"?common:white).insert(token(v));
        if(type=="E"){row>>v;tiger::ImportedLexiconEntry entry{token(v),{}};while(row>>v)entry.candidates.push_back(token(v));
            auto old=std::find_if(source.begin(),source.end(),[&](const auto& e){return e.code==entry.code;});if(old==source.end())source.push_back(std::move(entry));else old->candidates=std::move(entry.candidates);}
        if(type=="Q"){row>>v;queries.push_back(token(v));}
        if(type!="X")continue;
        auto prepared=tiger::prepareSentenceLexicon(source);auto bytes=tiger::serializeImportedLexicon(prepared);
        const auto path=output/(std::to_string(id)+".tcs");{std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!out)throw std::runtime_error("Cannot save fixture index");}
        auto mapping=tiger::Dictionary::Open(path);tiger::SentenceLexicon index(mapping,common,white);
        std::vector<std::u16string> inventory;
        std::set<std::u16string> identities;
        for(const auto& entry:source) {
            auto code=tiger::normalizeAddedCode(entry.code);
            if(!code.empty() && identities.insert(tiger::ordinalCaseKey(code)).second)inventory.push_back(code);
        }
        if(index.sourceCodeCount()!=inventory.size())throw std::runtime_error("Source inventory lost empty or duplicate keys");
        for(std::uint32_t i=0;i<index.sourceCodeCount();++i)
            if(index.sourceCode(i)!=inventory[i])throw std::runtime_error("Serialized source order changed");
        if(!index.sourceCode(index.sourceCodeCount()).empty())throw std::runtime_error("Source inventory bounds failure");
        if(mapping!=tiger::Dictionary::Open(path))throw std::runtime_error("Sentence mapping not reused");
        std::cout<<"{\"id\":"<<id<<",\"lengths\":[";bool first=true;
        for(auto n:index.codeLengths()){if(!first)std::cout<<',';first=false;std::cout<<n;}
        std::cout<<"],\"queries\":[";first=true;
        for(const auto& q:queries){if(!first)std::cout<<',';first=false;std::cout<<"{\"code\":";hex(q);
            std::cout<<",\"prefix\":"<<(index.isProperCodePrefix(q)?"true":"false")<<",\"candidates\":[";bool fc=true;
            for(const auto& c:index.candidates(q)){if(!fc)std::cout<<',';fc=false;std::cout<<"{\"text\":";hex(c.text);
                std::cout<<",\"rank\":"<<c.rank<<",\"log\":"<<c.logRank<<",\"optimal\":"<<(c.optimalSingleCharacterCode?"true":"false")<<",\"elements\":[";bool fe=true;
                for(const auto& e:c.textElements){if(!fe)std::cout<<',';fe=false;hex(e);}std::cout<<"]}";}
            std::cout<<"]}";}
        std::cout<<"]}\n";
    }
    return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
int wmain(int argc,wchar_t** argv){return argc==3?run(argv[1],argv[2]):2;}
