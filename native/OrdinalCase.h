#pragma once
#include <icu.h>
#include <string>
#include <string_view>
namespace tiger {
// .NET ICU ordinal casing uses simple uppercase, preserving U+0131 and U+017F.
// Reference: dotnet/runtime v10.0.0, System.Globalization.Native/pal_common.c.
inline std::u16string ordinalCaseKey(std::u16string_view text) {
    std::u16string result;result.reserve(text.size());
    for(std::size_t i=0;i<text.size();) {
        UChar32 c=text[i++];
        if(c>=0xd800 && c<=0xdbff && i<text.size() && text[i]>=0xdc00 && text[i]<=0xdfff)
            c=0x10000+((c-0xd800)<<10)+(text[i++]-0xdc00);
        if(c!=0x0131 && c!=0x017f) c=u_toupper(c);
        if(c>0xffff) {result+=static_cast<char16_t>(0xd800+((c-0x10000)>>10));result+=static_cast<char16_t>(0xdc00+((c-0x10000)&1023));}
        else result+=static_cast<char16_t>(c);
    }
    return result;
}
inline int ordinalCompareIgnoreCase(std::u16string_view a,std::u16string_view b) {
    return ordinalCaseKey(a).compare(ordinalCaseKey(b));
}
}
