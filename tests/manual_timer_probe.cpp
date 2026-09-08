#include "UppercaseText.h"
#include "Text.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("manual_timer_probe <hex inputs>");
        std::ifstream input(std::filesystem::u8path(argv[1]));
        if(!input) throw std::runtime_error("Cannot open timer cases");
        for(std::string hex;std::getline(input,hex);) {
            std::string bytes;
            for(std::size_t i=0;i<hex.size();i+=2) bytes+=static_cast<char>(std::stoi(hex.substr(i,2),nullptr,16));
            const auto result=tiger::parseManualTimer(tiger::utf16(bytes));
            std::cout<<"{\"matched\":"<<(result.matched?"true":"false")<<",\"milliseconds\":"<<result.milliseconds<<"}\n";
        }
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
