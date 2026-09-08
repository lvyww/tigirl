#include "SelectionKeys.h"
#include "Text.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
static int digit(char c) { return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:throw std::runtime_error("Bad hex"); }
static void output(const tiger::SelectionKeys& keys,bool valid) {
    std::cout<<(valid?1:0)<<'\t';
    constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:tiger::utf8(keys.serialize())) std::cout<<hex[c>>4]<<hex[c&15];
    std::cout<<'\n';
}
int main(int argc,char** argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--file") {
            output(tiger::SelectionKeys::load(std::filesystem::u8path(argv[2])),true); return 0;
        }
        std::ifstream file;
        if(argc==2) file.open(std::filesystem::u8path(argv[1]));
        std::istream& input=argc==2?file:std::cin;
        std::string line;
        while(std::getline(input,line)) {
            if(!line.empty() && line.back()=='\r') line.pop_back();
            std::string bytes;
            if(line.size()%2) throw std::runtime_error("Odd hex");
            for(std::size_t i=0;i<line.size();i+=2) bytes.push_back(static_cast<char>(digit(line[i])*16+digit(line[i+1])));
            tiger::SelectionKeys keys; std::u16string error;
            bool valid=tiger::SelectionKeys::parse(tiger::utf16(bytes),keys,error);
            output(keys,valid);
        }
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
