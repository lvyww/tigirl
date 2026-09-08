#include "SelectionKeys.h"
#include "Text.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <fstream>
#include <stdexcept>

namespace tiger {
namespace {
bool whitespace(char16_t c) {
    return (c>=9 && c<=13) || c==32 || c==0x85 || c==0xa0 || c==0x1680 ||
        (c>=0x2000 && c<=0x200a) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000;
}
std::u16string_view trim(std::u16string_view s) {
    while(!s.empty() && whitespace(s.front())) s.remove_prefix(1);
    while(!s.empty() && whitespace(s.back())) s.remove_suffix(1);
    return s;
}
std::u16string number(int n) { auto s=std::to_string(n); return {s.begin(),s.end()}; }
bool integer(std::u16string_view s,int base,int& value) {
    s=trim(s); bool negative=false;
    if(base==10 && !s.empty() && (s.front()==u'+' || s.front()==u'-')) {
        negative=s.front()==u'-'; s.remove_prefix(1);
    }
    if(s.empty()) return false;
    const std::uint64_t limit=base==16?0xffffffffull:negative?2147483648ull:2147483647ull;
    std::uint64_t n=0;
    for(auto c:s) {
        int d=c>=u'0' && c<=u'9'?c-u'0':c>=u'a' && c<=u'f'?c-u'a'+10:c>=u'A' && c<=u'F'?c-u'A'+10:99;
        if(d>=base || n>(limit-static_cast<unsigned>(d))/static_cast<unsigned>(base)) return false;
        n=n*static_cast<unsigned>(base)+static_cast<unsigned>(d);
    }
    const auto signedValue=base==16 && n>2147483647ull?static_cast<std::int64_t>(n)-4294967296ll:
        negative?-static_cast<std::int64_t>(n):static_cast<std::int64_t>(n);
    value=static_cast<int>(signedValue); return true;
}
const std::map<std::u16string,int>& names() {
    static const auto values=[] {
        std::map<std::u16string,int> v={
            {u"VK_SHIFT",16},{u"VK_LSHIFT",160},{u"VK_RSHIFT",161},
            {u"VK_CONTROL",17},{u"VK_LCONTROL",162},{u"VK_RCONTROL",163},
            {u"VK_MENU",18},{u"VK_LMENU",164},{u"VK_RMENU",165},
            {u"VK_LWIN",91},{u"VK_RWIN",92},{u"VK_CAPITAL",20},{u"VK_SPACE",32},
            {u"VK_BACK",8},{u"VK_RETURN",13},{u"VK_TAB",9},{u"VK_ESCAPE",27},
            {u"VK_OEM_1",186},{u"VK_OEM_2",191},{u"VK_OEM_4",219},{u"VK_OEM_7",222},
            {u"VK_OEM_COMMA",188},{u"VK_OEM_PERIOD",190}};
        for(int i=0;i<=9;++i) v[u"VK_"+number(i)]=48+i;
        for(int i=0;i<26;++i) v[u"VK_"+std::u16string(1,static_cast<char16_t>(u'A'+i))]=65+i;
        for(int i=1;i<=24;++i) v[u"VK_F"+number(i)]=111+i;
        return v;
    }();
    return values;
}
bool key(std::u16string_view s,int& value) {
    if(s.size()>=2 && s[0]==u'0' && (s[1]==u'x' || s[1]==u'X')) return integer(s.substr(2),16,value);
    if(integer(s,10,value)) return true;
    std::u16string name(s);
    for(auto& c:name) if(c>=u'a' && c<=u'z') c-=u'a'-u'A';
    const auto found=names().find(name);
    if(found==names().end()) return false;
    value=found->second; return true;
}
}
SelectionKeys::SelectionKeys() {
    for(int i=0;i<10;++i) bindings[i].push_back(i==9?48:49+i);
}
SelectionKeys SelectionKeys::load(const std::filesystem::path& path) {
    SelectionKeys keys;
    if(!std::filesystem::exists(path)) return keys;
    const auto text=readUnicodeFile(path);
    std::u16string error;
    if(!parse(text,keys,error)) throw std::runtime_error(utf8(error));
    return keys;
}
bool SelectionKeys::parse(std::u16string_view text,SelectionKeys& result,std::u16string& error) {
    SelectionKeys next;
    error.clear();
    while(!text.empty()) {
        const auto end=text.find_first_of(u"\r\n");
        auto line=trim(text.substr(0,end));
        if(end==text.npos) text={};
        else { text.remove_prefix(end+1); if(!text.empty() && text.front()==u'\n') text.remove_prefix(1); }
        if(line.empty() || line.front()==u'#') continue;
        std::vector<std::u16string_view> tokens;
        while(!line.empty()) {
            const auto split=line.find_first_of(u" \t");
            if(split!=0) tokens.push_back(line.substr(0,split));
            if(split==line.npos) break;
            line.remove_prefix(split+1);
        }
        int selection=0;
        const auto label=tokens.front();
        if(label.back()!=u'选' || !integer(label.substr(0,label.size()-1),10,selection) || selection<1 || selection>10) {
            error=u"自定义选重键存在无效标签："+std::u16string(label); return false;
        }
        std::vector<int> keys;
        for(std::size_t i=1;i<tokens.size();++i) {
            int vk;
            if(!key(tokens[i],vk)) { error=u"自定义选重键存在无效按键："+std::u16string(tokens[i]); return false; }
            if(std::find(keys.begin(),keys.end(),vk)==keys.end()) keys.push_back(vk);
        }
        next.bindings[selection-1]=std::move(keys);
    }
    result=std::move(next); return true;
}
std::array<int,256> SelectionKeys::dispatch() const {
    std::array<int,256> result{};
    for(int i=0;i<10;++i) for(auto vk:bindings[i]) if(vk>=0 && vk<256 && !result[vk]) result[vk]=i+1;
    return result;
}
std::u16string SelectionKeys::serialize() const {
    constexpr char16_t hex[]=u"0123456789ABCDEF";
    std::u16string result;
    for(int i=0;i<10;++i) {
        if(i) result+=u"\r\n";
        result+=number(i+1)+u"选";
        for(auto vk:bindings[i]) {
            result+=u" 0x";
            auto n=static_cast<std::uint32_t>(vk);
            std::u16string digits;
            do { digits.push_back(hex[n&15]); n>>=4; } while(n);
            if(digits.size()<2) digits.push_back(u'0');
            result.append(digits.rbegin(),digits.rend());
        }
    }
    return result;
}
}
