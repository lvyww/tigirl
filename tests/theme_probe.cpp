#include "CandidateTheme.h"
#include "Text.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <stdexcept>
int main() {
    try {
        const std::u16string_view names[]={u"默认",u"通透",u"一般通透",u"迷雾",u"星夜",u"纸",u"粉",u"赛博朋克",u"清晨",u"unknown"};
        for(auto name:names) {
            const auto& t=tiger::candidateTheme(name);
            std::vector<std::uint32_t> mask(64*32); mask[12*64+24]=0xffffff; mask[12*64+26]=0x808080;
            auto pixels=tiger::renderCandidateTheme(64,32,1,t,{{4,4,20,28}},mask);
            for(auto p:pixels) for(int shift=0;shift<24;shift+=8) if(((p>>shift)&255)>(p>>24)) throw std::runtime_error("Non-premultiplied pixel");
            std::cout<<"{\"name\":\""<<tiger::utf8(name)<<"\",\"colors\":[";
            const std::uint32_t values[]={t.foreground,t.background,t.border,t.selection,pixels[0],pixels[16*64+40],pixels[8*64+8],pixels[12*64+24],pixels[12*64+26],pixels[32]};
            for(unsigned i=0;i<10;++i) { if(i) std::cout<<','; std::cout<<'"'<<std::hex<<std::setw(8)<<std::setfill('0')<<values[i]<<'"'<<std::dec; }
            std::cout<<"],\"border\":"<<t.borderWidth<<",\"corners\":[";
            for(unsigned i=0;i<4;++i) { if(i) std::cout<<','; std::cout<<t.corners[i]; }
            std::cout<<"]}\n";
        }
        std::vector<std::uint32_t> mask(600*200,0);
        const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<20;++i) (void)tiger::renderCandidateTheme(600,200,1,tiger::candidateTheme(u"默认"),{{6,6,250,32}},mask);
        const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/20;
        std::cerr<<"Mean 600x200 theme render: "<<ms<<" ms\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
