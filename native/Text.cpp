#include "Text.h"
#include <stdexcept>
#include <fstream>
#include <cstdint>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace tiger {
std::string readTextFileBytes(const std::filesystem::path& path) {
#ifdef _WIN32
    struct File { HANDLE value=INVALID_HANDLE_VALUE; ~File() { if(value!=INVALID_HANDLE_VALUE) CloseHandle(value); } } input;
    const auto started=GetTickCount64(); DWORD error=0;
    for(;;) {
        input.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(input.value!=INVALID_HANDLE_VALUE) break;
        error=GetLastError();
        if((error!=ERROR_FILE_NOT_FOUND && error!=ERROR_SHARING_VIOLATION && error!=ERROR_ACCESS_DENIED) || GetTickCount64()-started>=100) break;
        Sleep(1);
    }
    if(input.value==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot read text file (Windows error "+std::to_string(error)+")");
    LARGE_INTEGER length{};
    if(!GetFileSizeEx(input.value,&length) || length.QuadPart<0 || length.QuadPart>4*1024*1024) throw std::runtime_error("Text file exceeds 4 MiB");
    std::string bytes(static_cast<std::size_t>(length.QuadPart),'\0');
    std::size_t done=0;
    while(done<bytes.size()) {
        DWORD count=0;
        if(!ReadFile(input.value,bytes.data()+done,static_cast<DWORD>(bytes.size()-done),&count,nullptr) || !count) throw std::runtime_error("Cannot read text file");
        done+=count;
    }
#else
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input) throw std::runtime_error("Cannot read text file");
    const auto length=input.tellg();
    if(length<0 || length>4*1024*1024) throw std::runtime_error("Text file exceeds 4 MiB");
    std::string bytes(static_cast<std::size_t>(length),'\0');
    input.seekg(0);
    if(length && !input.read(bytes.data(),length)) throw std::runtime_error("Cannot read text file");
#endif
    return bytes;
}
std::u16string readUnicodeFile(const std::filesystem::path& path) {
    return decodeUnicodeText(readTextFileBytes(path));
}
std::u16string decodeUnicodeText(std::string_view bytes) {
    std::u16string text;
    const bool utf32le=bytes.compare(0,4,std::string("\xff\xfe\0\0",4))==0;
    const bool utf32be=bytes.compare(0,4,std::string("\0\0\xfe\xff",4))==0;
    const bool utf16le=bytes.compare(0,2,"\xff\xfe")==0;
    const bool utf16be=bytes.compare(0,2,"\xfe\xff")==0;
    if(utf32le || utf32be || utf16le || utf16be) {
        const std::size_t width=utf32le || utf32be?4:2;
        if(bytes.size()%width) throw std::runtime_error("Truncated Unicode text file");
        for(std::size_t i=width;i<bytes.size();i+=width) {
            std::uint32_t value=0;
            for(std::size_t j=0;j<width;++j) {
                const auto index=utf32le || utf16le?width-1-j:j;
                value=(value<<8)|static_cast<unsigned char>(bytes[i+index]);
            }
            if(width==4 && (value>0x10ffff || (value>=0xd800 && value<=0xdfff)))
                throw std::runtime_error("Invalid Unicode text file");
            if(value>0xffff) { value-=0x10000; text.push_back(static_cast<char16_t>(0xd800+(value>>10))); value=0xdc00+(value&1023); }
            text.push_back(static_cast<char16_t>(value));
        }
        (void)utf8(text); // Validate surrogate pairs in UTF-16 input.
    } else {
        if(bytes.compare(0,3,"\xef\xbb\xbf")==0) bytes.remove_prefix(3);
        text=utf16(bytes);
    }
    return text;
}

std::u16string normalizeCode(std::u16string_view text) {
    const auto start = text.find_first_not_of(u" \t\r\n");
    if (start == std::u16string_view::npos) return {};
    text = text.substr(start, text.find_last_not_of(u" \t\r\n") - start + 1);
    std::u16string result(text);
    for (auto& ch : result) if (ch >= u'A' && ch <= u'Z') ch += u'a' - u'A';
    return result;
}
std::u16string unpackText(std::u16string_view text) {
    std::u16string result;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == u'\\' && i + 1 < text.size()) {
            switch (text[i + 1]) {
            case u'\\': result += u'\\'; ++i; continue;
            case u't': result += u'\t'; ++i; continue;
            case u'n': result += u"\r\n"; ++i; continue;
            case u's': result += u' '; ++i; continue;
            default: break;
            }
        }
        result += text[i];
    }
    return result;
}
std::u16string packText(std::u16string_view text) {
    std::u16string result;
    for (std::size_t i = 0; i < text.size(); ++i) {
        switch (text[i]) {
        case u'\\': result += u"\\\\"; break;
        case u'\t': result += u"\\t"; break;
        case u' ': result += u"\\s"; break;
        case u'\r': if (i + 1 < text.size() && text[i+1] == u'\n') { result += u"\\n"; ++i; } else result += text[i]; break;
        case u'\n': result += u"\\n"; break;
        default: result += text[i]; break;
        }
    }
    return result;
}
std::u16string parseEntry(std::u16string_view text) {
    const auto first = text.find_first_not_of(u" \t\r\n");
    if (first == std::u16string_view::npos) return {};
    auto result = unpackText(text.substr(first, text.find_last_not_of(u" \t\r\n") - first + 1));
    const auto marker = result.find(u"=>");
    if (marker != std::u16string::npos && marker && marker + 2 < result.size()) {
        auto display = result.substr(0, marker), commit = result.substr(marker + 2);
        return display == commit ? commit : display + u'\x1e' + commit;
    }
    return result;
}
std::u16string_view commitText(std::u16string_view entry) {
    const auto marker = entry.find(u'\x1e');
    if (marker == std::u16string_view::npos) return entry;
    const auto suffix = entry.substr(marker + 1);
    return suffix.empty() ? entry.substr(0, marker) : suffix;
}
std::u16string_view displayText(std::u16string_view entry) {
    const auto marker = entry.find(u'\x1e');
    if (marker == std::u16string_view::npos) return entry;
    return marker ? entry.substr(0, marker) : entry.substr(1);
}
std::string utf8(std::u16string_view text) {
    std::string result;
    for (std::size_t i=0; i<text.size(); ++i) {
        unsigned cp=text[i];
        if (cp>=0xd800 && cp<=0xdbff) {
            if (++i==text.size() || text[i]<0xdc00 || text[i]>0xdfff)
                throw std::invalid_argument("Unpaired UTF-16 surrogate");
            cp=0x10000+((cp-0xd800)<<10)+(text[i]-0xdc00);
        } else if (cp>=0xdc00 && cp<=0xdfff) throw std::invalid_argument("Unpaired UTF-16 surrogate");
        if (cp<0x80) result+=static_cast<char>(cp);
        else {
            if (cp<0x800) result+=static_cast<char>(0xc0|(cp>>6));
            else {
                if (cp<0x10000) result+=static_cast<char>(0xe0|(cp>>12));
                else { result+=static_cast<char>(0xf0|(cp>>18)); result+=static_cast<char>(0x80|((cp>>12)&63)); }
                result+=static_cast<char>(0x80|((cp>>6)&63));
            }
            result+=static_cast<char>(0x80|(cp&63));
        }
    }
    return result;
}
std::u16string utf16(std::string_view text) {
    std::u16string result;
    for (std::size_t i=0; i<text.size();) {
        unsigned cp=static_cast<unsigned char>(text[i++]);
        unsigned count=0, minimum=0;
        if (cp<0x80) {}
        else if (cp>=0xc2 && cp<=0xdf) { cp&=31; count=1; minimum=0x80; }
        else if (cp>=0xe0 && cp<=0xef) { cp&=15; count=2; minimum=0x800; }
        else if (cp>=0xf0 && cp<=0xf4) { cp&=7; count=3; minimum=0x10000; }
        else throw std::invalid_argument("Invalid UTF-8 lead byte");
        while (count--) {
            if (i==text.size()) throw std::invalid_argument("Truncated UTF-8");
            const auto next=static_cast<unsigned char>(text[i++]);
            if ((next&0xc0)!=0x80) throw std::invalid_argument("Invalid UTF-8 continuation");
            cp=(cp<<6)|(next&63);
        }
        if (cp<minimum || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))
            throw std::invalid_argument("Invalid UTF-8 code point");
        if (cp>=0x10000) { cp-=0x10000; result+=static_cast<char16_t>(0xd800+(cp>>10)); result+=static_cast<char16_t>(0xdc00+(cp&1023)); }
        else result+=static_cast<char16_t>(cp);
    }
    return result;
}
}
