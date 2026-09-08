#include "LexiconDecode.h"
#include <cstdint>
#include <array>
#include <fstream>
#include <stdexcept>

namespace tiger {
std::u16string readLexiconText(const std::filesystem::path& path) {
    if(!std::filesystem::is_regular_file(path)) throw std::runtime_error("Table is not a regular file");
    std::ifstream file(path,std::ios::binary);
    if(!file) throw std::runtime_error("Cannot open table file");
    std::string bytes;
    std::array<char,65536> buffer{};
    while(file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
        bytes.append(buffer.data(),static_cast<std::size_t>(file.gcount()));
    }
    if(!file.eof() || file.bad()) throw std::runtime_error("Cannot read complete table file");
    return decodeLexiconBytes(bytes);
}
std::u16string decodeLexiconBytes(std::string_view bytes) {
    const auto size=bytes.size();
    auto b=[&](std::size_t i){return static_cast<unsigned char>(bytes[i]);};
    std::u16string result;
    auto scalar=[&](std::uint32_t value) {
        if(value>0x10ffff || (value>=0xd800 && value<=0xdfff)) result.push_back(0xfffd);
        else if(value<0x10000) result.push_back(static_cast<char16_t>(value));
        else {value-=0x10000;result.push_back(static_cast<char16_t>(0xd800+(value>>10)));result.push_back(static_cast<char16_t>(0xdc00+(value&1023)));}
    };
    std::size_t at=0;
    // DetectTextEncoding selects UTF-16 for FF FE before StreamReader sees it;
    // its matching preamble therefore takes precedence over UTF-32 LE detection.
    if(size>=2 && ((b(0)==0xff && b(1)==0xfe) || (b(0)==0xfe && b(1)==0xff))) {
        const bool little=b(0)==0xff;at=2;
        auto unit=[&](std::size_t i){return static_cast<std::uint32_t>(little?b(i)|(b(i+1)<<8):(b(i)<<8)|b(i+1));};
        while(at+1<size) {
            auto value=unit(at);at+=2;
            if(value>=0xd800 && value<=0xdbff && at+1<size) {
                auto low=unit(at);
                if(low>=0xdc00 && low<=0xdfff) {scalar(0x10000+((value-0xd800)<<10)+(low-0xdc00));at+=2;continue;}
            }
            scalar(value);
        }
        if(at<size) result.push_back(0xfffd);
        return result;
    }
    if(size>=4 && b(0)==0 && b(1)==0 && b(2)==0xfe && b(3)==0xff) {
        at=4;
        while(size-at>=4) {
            scalar((std::uint32_t(b(at))<<24)|(std::uint32_t(b(at+1))<<16)|(std::uint32_t(b(at+2))<<8)|b(at+3));at+=4;
        }
        if(at<size) result.push_back(0xfffd);
        return result;
    }
    if(size>=3 && b(0)==0xef && b(1)==0xbb && b(2)==0xbf) at=3;
    while(at<size) {
        auto lead=b(at++);
        if(lead<0x80) {scalar(lead);continue;}
        unsigned length=lead>=0xc2&&lead<=0xdf?2:lead>=0xe0&&lead<=0xef?3:lead>=0xf0&&lead<=0xf4?4:0;
        if(!length) {result.push_back(0xfffd);continue;}
        std::uint32_t value=lead&((1u<<(7-length))-1);
        unsigned consumed=1;
        while(consumed<length && at<size) {
            auto next=b(at);
            if(next<0x80 || next>0xbf) break;
            if(consumed==1 && ((lead==0xe0 && next<0xa0)||(lead==0xed && next>=0xa0)||
                (lead==0xf0 && next<0x90)||(lead==0xf4 && next>=0x90))) break;
            value=(value<<6)|(next&63);++at;++consumed;
        }
        if(consumed==length) scalar(value);else result.push_back(0xfffd);
    }
    return result;
}
}
