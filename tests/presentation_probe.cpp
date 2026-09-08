#include "CandidatePresentation.h"
#include "Text.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
static int digit(char c) { return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:throw std::runtime_error("Bad hex"); }
static std::u16string decode(const std::string& s) {
    std::string bytes;
    if(s.size()%2) throw std::runtime_error("Odd hex");
    for(std::size_t i=0;i<s.size();i+=2) bytes.push_back(static_cast<char>(digit(s[i])*16+digit(s[i+1])));
    return tiger::utf16(bytes);
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("presentation_probe <cases.tsv>");
        std::ifstream input(std::filesystem::u8path(argv[1]));
        if(!input) throw std::runtime_error("Cannot open cases");
        for(std::string line;std::getline(input,line);) {
            if(!line.empty() && line.back()=='\r') line.pop_back();
            std::istringstream fields(line); std::string item;
            std::getline(fields,item,'\t'); const auto bits=std::stoi(item);
            tiger::CandidateStyle style; style.vertical=(bits&1)!=0; style.showIndex=(bits&2)!=0;
            style.showCode=(bits&4)!=0; style.hideCandidates=(bits&8)!=0;
            tiger::Snapshot snapshot;
            std::getline(fields,item,'\t'); snapshot.raw=decode(item);
            while(std::getline(fields,item,'\t')) {
                tiger::Candidate candidate; candidate.display=decode(item);
                if(!std::getline(fields,item,'\t')) item.clear();
                candidate.annotation=decode(item); snapshot.candidates.push_back(candidate);
            }
            auto model=tiger::presentCandidates(snapshot,style);
            auto text=model.code;
            if(!text.empty() && !model.items.empty()) {
                if(style.vertical) text+=u'\n';
                else if(text.size()<7) text.append(7-text.size(),u' ');
            }
            for(std::size_t i=0;i<model.items.size();++i) {
                if(i) text+=style.vertical?u"\n":u"  ";
                text+=model.items[i];
            }
            constexpr char hex[]="0123456789abcdef";
            for(unsigned char c:tiger::utf8(text)) std::cout<<hex[c>>4]<<hex[c&15];
            std::cout<<'\n';
        }
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
