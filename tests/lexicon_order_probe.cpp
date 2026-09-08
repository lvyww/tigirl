#include "LexiconOrder.h"
#include <iostream>
#include <iomanip>
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc!=3) return 2;
        std::wstring wide(argv[2]);std::string culture;
        for(wchar_t c:wide) {if(c>127) return 2;culture.push_back(static_cast<char>(c));}
        for(const auto& path:tiger::orderedLexiconFiles(argv[1],culture)) {
            for(auto unit:path.filename().u16string()) std::cout<<std::hex<<std::setfill('0')<<std::setw(4)<<static_cast<unsigned>(unit);
            std::cout<<'\n';
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
