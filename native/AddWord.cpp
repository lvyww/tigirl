#define NOMINMAX
#include <windows.h>
#include <icu.h>
#include "AddWord.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace tiger {
namespace {
bool whitespace(char16_t c) {
    return (c>=9 && c<=13) || c==32 || c==0x85 || c==0xa0 || c==0x1680 ||
        (c>=0x2000 && c<=0x200a) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000;
}
std::u16string trim(std::u16string_view text) {
    while(!text.empty() && whitespace(text.front())) text.remove_prefix(1);
    while(!text.empty() && whitespace(text.back())) text.remove_suffix(1);
    return std::u16string(text);
}
void replace(std::u16string& text,std::u16string_view from,std::u16string_view to) {
    std::size_t offset=0;
    while((offset=text.find(from,offset))!=text.npos) { text.replace(offset,from.size(),to); offset+=to.size(); }
}
}
std::u16string constructWordCode(const Dictionary& dictionary,std::u16string_view word) {
    return constructWordCode(word,[&](std::u16string_view element) {
        return dictionary.value(dictionary.find(Section::ConstructCode,element),0);
    });
}
std::u16string constructWordCode(std::u16string_view word,
    const std::function<std::u16string_view(std::u16string_view)>& lookup) {
    std::u16string source(word);
    for(auto token:{u"\n",u"\r",u"\t",u" ",u"=",u"，",u"-",u"。",u"·",u"【",u"、",u"】",u"；",u"）",u"！",u"@",u"#",u"￥",u"%",u"……",u"&",u"*",u"（",u"+",u"《",u"——",u"》",u"~",u"{",u"|",u"}",u"？",u"：",u",",u".",u"`",u"[",u"\\",u"]",u"/",u";",u"'",u")",u"!",u"$",u"^",u"<",u"_",u">",u"?",u"\""}) replace(source,token,{});
    std::vector<std::u16string> codes;
    for(const auto& element:wordTextElements(source)) {
        auto code=lookup(element);
        if(!code.empty()) codes.emplace_back(code);
        else if(element.size()==1 && ((element[0]>=u'a' && element[0]<=u'z') || (element[0]>=u'A' && element[0]<=u'Z'))) {
            auto c=element[0]; if(c<=u'Z') c+=u'a'-u'A'; codes.emplace_back(2,c);
        }
    }
    if(codes.empty()) return {};
    if(codes.size()==1) return codes[0];
    if(codes.size()==2) return codes[0].size()>=2 && codes[1].size()>=2?codes[0].substr(0,2)+codes[1].substr(0,2):u"";
    if(codes.size()==3) return codes[2].size()>=2?codes[0].substr(0,1)+codes[1].substr(0,1)+codes[2].substr(0,2):u"";
    return codes[0].substr(0,1)+codes[1].substr(0,1)+codes[2].substr(0,1)+codes.back().substr(0,1);
}
std::u16string decodeLexiconEscapes(std::u16string_view text) {
    std::u16string raw(text);
    replace(raw,u"\\\\",u"Bime20231222BIME");
    replace(raw,u"\\t",u"\t"); replace(raw,u"\\n",u"\r\n"); replace(raw,u"\\s",u" ");
    replace(raw,u"Bime20231222BIME",u"\\");
    return raw;
}
std::u16string parseAddedWord(std::u16string_view text) {
    auto raw=decodeLexiconEscapes(trim(text));
    auto marker=raw.find(u"=>");
    if(marker!=raw.npos && marker>0 && marker+2<raw.size()) {
        auto display=raw.substr(0,marker),commit=raw.substr(marker+2);
        return display==commit?commit:display+u'\x1e'+commit;
    }
    return raw;
}
std::u16string normalizeAddedCode(std::u16string_view code) {
    auto input=trim(code); std::u16string normalized;
    for(std::size_t i=0;i<input.size();) {
        UChar32 c=input[i++];
        if(c>=0xd800 && c<=0xdbff && i<input.size() && input[i]>=0xdc00 && input[i]<=0xdfff)
            c=0x10000+((c-0xd800)<<10)+(input[i++]-0xdc00);
        // .NET invariant casing preserves capital I-with-dot instead of
        // ICU's locale-independent simple mapping to ASCII i.
        if(c!=0x0130) c=u_tolower(c);
        if(c>0xffff) { normalized+=static_cast<char16_t>(0xd800+((c-0x10000)>>10)); normalized+=static_cast<char16_t>(0xdc00+((c-0x10000)&1023)); }
        else normalized+=static_cast<char16_t>(c);
    }
    return normalized;
}
UserChange prepareAddedWord(std::u16string_view code,std::u16string_view text) {
    auto normalized=normalizeAddedCode(code);
    if(normalized.empty()) throw std::invalid_argument("编码为空。");
    auto packed=parseAddedWord(text);
    if(packed.empty()) throw std::invalid_argument("词条为空。");
    return {ChangeKind::Add,std::move(normalized),std::move(packed)};
}
}
