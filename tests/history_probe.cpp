#include "History.h"
#include "Text.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
static std::u16string decode(std::string_view hex) {
    std::string bytes;
    auto digit=[](char c) { return c<='9'?c-'0':c-'a'+10; };
    for(std::size_t i=0;i<hex.size();i+=2) bytes+=static_cast<char>(digit(hex[i])*16+digit(hex[i+1]));
    return tiger::utf16(bytes);
}
static void print(const tiger::History& history) {
    constexpr char hex[]="0123456789abcdef";
    for(const auto& element:history.recent()) {
        for(unsigned char c:tiger::utf8(element)) std::cout<<hex[c>>4]<<hex[c&15];
        std::cout<<' ';
    }
    std::cout<<'\n';
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("history_probe <actions.txt>");
        std::ifstream input(std::filesystem::u8path(argv[1]));
        if(!input) throw std::runtime_error("Cannot read history actions");
        tiger::History history;
        for(std::string line;std::getline(input,line);) {
            const auto before=history.text(); auto preview=history;
            auto apply=[&](tiger::History& target) {
                if(line=="-") target.pop();
                else target.append(decode(line));
            };
            apply(preview);
            if(history.text()!=before) throw std::runtime_error("Preview changed shared history");
            apply(history);
            if(history.recent()!=preview.recent()) throw std::runtime_error("Preview/dispatch history differs");
            print(history);
        }
        // Long sessions must copy cheaply and release without recursive stack overflow.
        tiger::History longHistory;
        for(int i=0;i<100000;++i) longHistory.append(u"字");
        auto copy=longHistory; copy.pop();
        if(longHistory.recent().size()!=20) throw std::runtime_error("Long history corrupt");
        for(int i=0;i<99999;++i)if(copy.pop()!=u"字")throw std::runtime_error("Full history backspace lost an element");
        if(!copy.empty() || !copy.pop().empty() || longHistory.recent().size()!=20)throw std::runtime_error("Full history pop changed its snapshot");
        for(int count:{1,63,64,65,127,128,129,1000}) {
            tiger::History source;
            for(int i=0;i<count;++i)source.append(u"字");
            auto branch=source;branch.pop();branch.append(u"a\u0301");
            if(source.last()!=u"字" || branch.last()!=u"a\u0301" || branch.pop()!=u"a\u0301")
                throw std::runtime_error("History branch corrupted grapheme boundaries");
            for(int i=0;i<count-1;++i)if(branch.pop()!=u"字")throw std::runtime_error("History branch lost its prefix");
            if(!branch.empty() || source.recent(static_cast<std::size_t>(count)+1).size()!=static_cast<std::size_t>(count))
                throw std::runtime_error("History chunk boundary lost a snapshot");
        }
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
