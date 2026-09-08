#include "UppercaseText.h"
#include "Text.h"
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    std::ifstream input(std::filesystem::u8path(argv[1])); if(!input) return 2;
    for(std::string line;std::getline(input,line);) {
        auto text=tiger::utf16(line);
        std::cout<<tiger::numericUppercasePrefix(text)<<'\t';
        constexpr char hex[]="0123456789abcdef";
        for(unsigned char c:tiger::utf8(tiger::uppercaseCurrency(text))) std::cout<<hex[c>>4]<<hex[c&15];
        std::cout<<'\n';
    }
}
