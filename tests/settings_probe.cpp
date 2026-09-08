#include "Settings.h"
#include "Text.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
static int digit(char c) { return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:throw std::runtime_error("Bad hex"); }
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("settings_probe <hex-cases.txt>");
        std::ifstream input(std::filesystem::u8path(argv[1]));
        if(!input) throw std::runtime_error("Cannot open cases");
        std::cout<<std::boolalpha;
        std::string line;
        while(std::getline(input,line)) {
            if(!line.empty() && line.back()=='\r') line.pop_back();
            if(line.size()%2) throw std::runtime_error("Odd hex");
            std::string bytes;
            for(std::size_t i=0;i<line.size();i+=2) bytes.push_back(static_cast<char>(digit(line[i])*16+digit(line[i+1])));
            auto config=tiger::parseEngineSettings(tiger::utf16(bytes));
            const auto style=tiger::parseCandidateStyle(tiger::utf16(bytes));
            std::cout<<'{';
#define FIELD(name) std::cout<<"\"" #name "\":"<<config.name
#define NEXT(name) std::cout<<','; FIELD(name)
            FIELD(defaultChinese); NEXT(shiftToggle); NEXT(ctrlSpaceToggle); NEXT(englishPunctuation); NEXT(slashDunhao);
            NEXT(enterClear); NEXT(tabClear); NEXT(clearOnNoCode); NEXT(maxCodeAutoCommit); NEXT(reverseLookup);
            NEXT(semicolonSecond); NEXT(quoteThird); NEXT(showComment); NEXT(showSplit); NEXT(maxCodeLength); NEXT(pageSize); NEXT(pageKeys);
            std::cout<<",\"vertical\":"<<style.vertical<<",\"showIndex\":"<<style.showIndex
                <<",\"showCode\":"<<style.showCode<<",\"hideCandidates\":"<<style.hideCandidates<<",\"fontSize\":"<<style.fontSize<<",\"font\":\"";
            constexpr char hex[]="0123456789abcdef";
            for(auto c:style.font) std::cout<<"\\u"<<hex[(c>>12)&15]<<hex[(c>>8)&15]<<hex[(c>>4)&15]<<hex[c&15];
            std::cout<<'"';
            std::cout<<",\"theme\":\"";
            for(auto c:style.theme) std::cout<<"\\u"<<hex[(c>>12)&15]<<hex[(c>>8)&15]<<hex[(c>>4)&15]<<hex[c&15];
            std::cout<<'"';
            std::cout<<"}\n";
        }
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
