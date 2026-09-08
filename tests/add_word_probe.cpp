#include "AddWord.h"
#include "Text.h"
#include <fstream>
#include <iostream>
static int digit(char c) { return c>='0' && c<='9'?c-'0':c-'a'+10; }
static void hex(std::u16string_view value) {
    constexpr char digits[]="0123456789abcdef";
    for(unsigned char c:tiger::utf8(value)) std::cout<<digits[c>>4]<<digits[c&15];
}
int main(int argc,char** argv) {
    try {
        if(argc!=3) return 2;
        auto dictionary=tiger::Dictionary::Open(std::filesystem::u8path(argv[1]));
        std::ifstream input(std::filesystem::u8path(argv[2])); if(!input) return 2;
        for(std::string line;std::getline(input,line);) {
            std::string bytes;
            for(std::size_t i=0;i+1<line.size();i+=2) bytes+=static_cast<char>(digit(line[i])*16+digit(line[i+1]));
            auto text=tiger::utf16(bytes);
            hex(tiger::constructWordCode(*dictionary,text)); std::cout<<'\t';
            hex(tiger::parseAddedWord(text)); std::cout<<'\t';
            hex(tiger::normalizeAddedCode(text)); std::cout<<'\t';
            auto elements=tiger::wordTextElements(text);
            for(std::size_t i=0;i<elements.size();++i) { if(i) std::cout<<','; hex(elements[i]); }
            std::cout<<'\n';
        }
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
