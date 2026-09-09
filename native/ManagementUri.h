#pragma once
#include <string>
#include <stdexcept>
namespace tiger {
inline std::wstring managementUri(std::wstring_view action,std::wstring_view value) {
    std::wstring result=L"nativetiger://"+std::wstring(action)+L"/";
    constexpr wchar_t hex[]=L"0123456789abcdef";
    for(auto c:value)for(int shift=12;shift>=0;shift-=4)result+=hex[(c>>shift)&15];
    return result;
}
inline std::pair<std::wstring,std::wstring> parseManagementUri(std::wstring_view uri) {
    constexpr std::wstring_view prefix=L"nativetiger://";
    if(uri.substr(0,prefix.size())!=prefix || uri.size()>2048)throw std::runtime_error("Invalid management URI");
    uri.remove_prefix(prefix.size());auto slash=uri.find(L'/');
    if(slash==uri.npos)throw std::runtime_error("Missing management action");
    std::wstring action(uri.substr(0,slash)),value;uri.remove_prefix(slash+1);
    if(uri.size()%4)throw std::runtime_error("Invalid management value");
    while(!uri.empty()) {
        unsigned c=0;for(int i=0;i<4;++i) {auto digit=uri[i];if(!((digit>=L'0' && digit<=L'9') || (digit>=L'a' && digit<=L'f')))throw std::runtime_error("Invalid management encoding");c=c*16+(digit<=L'9'?digit-L'0':digit-L'a'+10);}
        if(c==0)throw std::runtime_error("Invalid management character");
        value+=static_cast<wchar_t>(c);uri.remove_prefix(4);
    }
    return {action,value};
}
}
