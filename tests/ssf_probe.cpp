#include "../native/SsfArchive.h"
#include "../native/SsfSkin.h"
#include "../native/SsfAnimation.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <cmath>
using namespace tiger::skin;
void hex(std::u16string_view s){for(auto c:s)std::cout<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c);std::cout<<std::dec;}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
#else
int main(int argc,char** argv){
#endif
    try{
        if(argc!=2)throw std::runtime_error("Usage: ssf_probe skin.ssf");std::ifstream input(std::filesystem::path(argv[1]),std::ios::binary);if(!input)throw std::runtime_error("Cannot open test skin");
        Bytes raw{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};auto files=decodeArchive(raw);std::cout<<"archive "<<files.size()<<'\n';
        for(const auto& f:files){std::cout<<"entry ";hex(f.first);std::cout<<' '<<f.second.size()<<' '<<crc32(f.second.data(),f.second.size())<<'\n';}
        const auto skin=parseDefinition(files);std::cout<<"layouts "<<skin.layouts.size()<<" warnings "<<skin.warnings.size()<<'\n';
        std::cout<<"metrics "<<skin.fontSize<<' '<<skin.candidateSpacing<<' '<<skin.lineSpacing<<' '<<skin.characterSpacing<<'\n';
        for(const auto& f:skin.assets){auto animation=splitApng(f.second);std::cout<<"image ";hex(f.first);std::cout<<' '<<animation.width<<' '<<animation.height<<' '<<animation.frames.size()<<' '<<animation.plays;for(const auto& frame:animation.frames)std::cout<<' '<<frame.delayMs;std::cout<<'\n';}
        for(unsigned source:{1u,2u,102u,163u,350u})for(float target:{1.f,25.f,200.f,3000.f})for(Stretch stretch:{Stretch{0,30,128},Stretch{1,78,25},Stretch{0,0,0}}){float length=0;for(const auto& s:sliceAxis(source,target,stretch)){if(!(s.source>=0 && s.sourceLength>0 && s.source+s.sourceLength<=source+.01 && s.destination>=0 && s.destinationLength>0 && s.destination+s.destinationLength<=target+.01))throw std::runtime_error("Invalid nine-slice bounds");length+=s.destinationLength;}if(std::abs(length-target)>.05)throw std::runtime_error("Nine-slice coverage mismatch");}
        if(animationFrame({100,200},0,100).index!=1 || animationFrame({100,200},1,300).remainingMs!=0 || animationFrame({100,200},0,300).index!=0)throw std::runtime_error("Animation clock mismatch");
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
