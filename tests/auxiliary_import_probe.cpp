#include "LexiconImport.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
static std::u16string decode(std::string_view hex) {
    if(hex.size()%4) throw std::runtime_error("Invalid UTF-16 hex");std::u16string result;
    for(std::size_t i=0;i<hex.size();i+=4) result+=static_cast<char16_t>(std::stoul(std::string(hex.substr(i,4)),nullptr,16));return result;
}
static void hex(std::u16string_view text) {
    const char* digits="0123456789abcdef";std::cout<<'"';
    for(auto c:text) std::cout<<digits[(c>>12)&15]<<digits[(c>>8)&15]<<digits[(c>>4)&15]<<digits[c&15];std::cout<<'"';
}
static void lookup(const std::map<std::u16string,std::u16string>& values) {
    std::cout<<'[';bool first=true;for(const auto& entry:values) {
        if(!first) std::cout<<',';first=false;std::cout<<'[';hex(entry.first);std::cout<<',';hex(entry.second);std::cout<<']';
    }std::cout<<']';
}
int main(int argc,char** argv) {
 try {
    if(argc!=2) throw std::runtime_error("auxiliary_import_probe <cases.hex>");
    std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Cannot open cases");
    for(std::string line;std::getline(input,line);) {
        const auto first=line.find(' '),second=first==line.npos?line.npos:line.find(' ',first+1);
        if(second==line.npos) throw std::runtime_error("Invalid case");
        auto text=std::string_view(line);tiger::ImportedLexicon imported;
        imported.pinyin=tiger::mergeCodedLexiconRows(tiger::parseLexiconRows(decode(text.substr(0,first)),false).coded);
        tiger::appendImportedAnnotations(imported.comments,decode(text.substr(first+1,second-first-1)),false);
        tiger::appendImportedAnnotations(imported.splits,decode(text.substr(second+1)),true);
        std::cout<<"{\"pinyin\":[";
        for(std::size_t i=0;i<imported.pinyin.size();++i) {
            if(i) std::cout<<',';const auto& entry=imported.pinyin[i];std::cout<<'[';hex(entry.code);std::cout<<",[";
            for(std::size_t j=0;j<entry.candidates.size();++j) {if(j) std::cout<<',';hex(entry.candidates[j]);}std::cout<<"]]";
        }
        std::cout<<"],\"comments\":";lookup(imported.comments);std::cout<<",\"splits\":";lookup(imported.splits);std::cout<<"}\n";
    }
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
