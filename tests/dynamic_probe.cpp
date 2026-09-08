#include "DynamicText.h"
#include "Text.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
static int digit(char c) { return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:throw std::runtime_error("Bad hex"); }
static std::u16string decode(std::string s) {
    std::string bytes;
    if(s.size()%2) throw std::runtime_error("Odd hex");
    for(std::size_t i=0;i<s.size();i+=2) bytes.push_back(static_cast<char>(digit(s[i])*16+digit(s[i+1])));
    return tiger::utf16(bytes);
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("dynamic_probe <cases.tsv>");
        std::ifstream input(std::filesystem::u8path(argv[1]));
        if(!input) throw std::runtime_error("Cannot read cases");
        std::minstd_rand random(12345);
        for(std::string line;std::getline(input,line);) {
            std::istringstream fields(line); tiger::LocalTime time; std::string name,text;
            if(!(fields>>time.year>>time.month>>time.day>>time.hour>>time.minute>>time.second>>time.weekday>>name)) throw std::runtime_error("Bad clock");
            fields>>text; time.weekdayName=decode(name);
            auto copy=random;
            const auto output=tiger::expandDynamicText(decode(text),random,&time);
            if(tiger::expandDynamicText(decode(text),copy,&time)!=output) throw std::runtime_error("Copied preview changed random sequence");
            constexpr char hex[]="0123456789abcdef";
            for(unsigned char c:tiger::utf8(output)) std::cout<<hex[c>>4]<<hex[c&15];
            std::cout<<'\n';
        }
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
