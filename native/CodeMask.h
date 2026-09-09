#pragma once
#include "Grapheme.h"
namespace tiger {
inline std::u16string maskInputCode(std::u16string_view input,std::u16string_view mask) {
    if(input.empty() || mask.empty())return std::u16string(input);
    const auto elements=wordTextElements(mask);
    if(elements.empty())return std::u16string(input);
    std::u16string result;
    for(auto c:input) {
        // .NET char.IsWhiteSpace, evaluated per UTF-16 code unit in the original.
        if((c>=9 && c<=13) || c==32 || c==0x85 || c==0xa0 || c==0x1680 ||
           (c>=0x2000 && c<=0x200a) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000) {
            result+=c;continue;
        }
        if(c>=u'A' && c<=u'Z')c+=u'a'-u'A';
        if(c==0x212a)c=u'k'; // Kelvin sign lowercases to ASCII k.
        const auto index=c>=u'a' && c<=u'z'?static_cast<unsigned>(c-u'a'):c==u';'?26u:0u;
        result+=elements[index%elements.size()];
    }
    return result;
}
}
