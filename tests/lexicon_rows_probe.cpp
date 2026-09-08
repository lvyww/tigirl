#include "LexiconImport.h"
#include "LexiconFile.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
static void json(std::u16string_view text) {
    const char* hex="0123456789abcdef";std::cout<<'"';
    for(auto c:text) std::cout<<"\\u"<<hex[(c>>12)&15]<<hex[(c>>8)&15]<<hex[(c>>4)&15]<<hex[c&15];
    std::cout<<'"';
}
int main(int argc,char** argv) {
 try {
    if(argc<2 || argc>3) throw std::runtime_error("lexicon_rows_probe <hex-input>");
    std::ifstream source(argv[1]);if(!source) throw std::runtime_error("Cannot open cases");
    const bool files=argc==3 && std::string_view(argv[2])=="--files";
    for(std::string line;std::getline(source,line);) {
        tiger::ParsedLexiconRows rows;
        if(files) rows=tiger::parseLexiconFile(std::filesystem::u8path(line));
        else {
        if(line.size()<2 || (line.size()-2)%4) throw std::runtime_error("Invalid case encoding");
        std::u16string text;
        for(std::size_t i=2;i<line.size();i+=4) text+=static_cast<char16_t>(std::stoul(line.substr(i,4),nullptr,16));
        rows=tiger::parseLexiconRows(text,line[0]=='1');
        }
        if(argc==3 && !files) {
            auto entries=tiger::mergeCodedLexiconRows(rows.coded);std::cout<<'[';
            for(std::size_t i=0;i<entries.size();++i) {
                if(i) std::cout<<',';
                std::cout<<'[';json(entries[i].code);std::cout<<",[";
                for(std::size_t j=0;j<entries[i].candidates.size();++j) {if(j) std::cout<<',';json(entries[i].candidates[j]);}
                std::cout<<"]]";
            }
            std::cout<<"]\n";continue;
        }
        std::cout<<"{\"coded\":[";
        for(std::size_t i=0;i<rows.coded.size();++i) {
            if(i) std::cout<<',';const auto& row=rows.coded[i];
            std::cout<<'[';json(row.code);std::cout<<',';json(row.text);std::cout<<','<<row.frequency<<']';
        }
        std::cout<<"],\"uncoded\":[";
        for(std::size_t i=0;i<rows.uncoded.size();++i) {
            if(i) std::cout<<',';const auto& row=rows.uncoded[i];
            std::cout<<'[';json(row.text);std::cout<<','<<row.frequency<<']';
        }
        std::cout<<"]}\n";
    }
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
