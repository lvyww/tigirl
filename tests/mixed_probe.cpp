#include "MixedInput.h"
#include "Text.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
static void json(std::u16string_view text) {
    constexpr char hex[]="0123456789abcdef"; std::cout<<'"';
    for(auto c:text) std::cout<<"\\u"<<hex[(c>>12)&15]<<hex[(c>>8)&15]<<hex[(c>>4)&15]<<hex[c&15];
    std::cout<<'"';
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("mixed_probe <cases.tsv>");
        std::ifstream input(std::filesystem::u8path(argv[1]));
        if(!input) throw std::runtime_error("Cannot open mixed cases");
        tiger::MixedDecoder decoder; int calls=0;
        auto resolver=[&](std::u16string_view code) {
            ++calls; std::u16string key(code);
            for(auto& c:key) if(c>=u'A' && c<=u'Z') c+=u'a'-u'A';
            return key.front()==u'z'?std::u16string{}:key+u":"+tiger::utf16(std::to_string(calls));
        };
        for(std::string line;std::getline(input,line);) {
            std::istringstream fields(line); bool clear; int maximum; std::uint64_t version; std::size_t count; std::string raw;
            if(!(fields>>clear>>maximum>>version>>std::quoted(raw)>>count)) throw std::runtime_error("Bad mixed case");
            tiger::MixedDecoder::Preferred preferred;
            for(std::size_t i=0;i<count;++i) {
                std::size_t start; std::string text;
                if(!(fields>>start>>std::quoted(text))) throw std::runtime_error("Bad preferred candidate");
                preferred[start]=tiger::utf16(text);
            }
            if(clear) decoder.clear();
            // Preview uses its own cache and does not change the live decoder.
            auto preview=decoder; const int before=calls;
            auto expected=preview.decode(tiger::utf16(raw),maximum,version,resolver,preferred);
            calls=before;
            auto result=decoder.decode(tiger::utf16(raw),maximum,version,resolver,preferred);
            if(expected.surface!=result.surface) throw std::runtime_error("Preview mutated live cache");
            std::cout<<"{\"raw\":";json(result.raw);std::cout<<",\"prefix\":";json(result.prefix);
            std::cout<<",\"active\":";json(result.active);std::cout<<",\"surface\":";json(result.surface);
            std::cout<<",\"segments\":[";
            for(std::size_t i=0;i<result.segments.size();++i) {
                if(i) std::cout<<',';
                std::cout<<"{\"code\":";json(result.segments[i].code);std::cout<<",\"candidate\":";json(result.segments[i].candidate);std::cout<<'}';
            }
            std::cout<<"],\"calls\":"<<calls<<",\"chinese\":";json(result.compose(u"尾",u"。"));
            std::cout<<",\"english\":";json(result.raw);std::cout<<"}\n";
        }
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
