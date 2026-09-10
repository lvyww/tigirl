#define NOMINMAX
#include <windows.h>
#include <icu.h>
#include "../native/Unicode.h"
#include "../native/OrdinalCase.h"
#include <iostream>
int main() {
    // ICU is an independent test oracle only, loaded explicitly on the test host.
    auto library=LoadLibraryW(L"icu.dll");if(!library)return 2;
    auto upper=reinterpret_cast<decltype(&u_toupper)>(GetProcAddress(library,"u_toupper"));
    auto lower=reinterpret_cast<decltype(&u_tolower)>(GetProcAddress(library,"u_tolower"));
    auto letter=reinterpret_cast<decltype(&u_isalpha)>(GetProcAddress(library,"u_isalpha"));
    auto space=reinterpret_cast<decltype(&u_isUWhiteSpace)>(GetProcAddress(library,"u_isUWhiteSpace"));
    auto category=reinterpret_cast<decltype(&u_charType)>(GetProcAddress(library,"u_charType"));
    auto version=reinterpret_cast<decltype(&u_getUnicodeVersion)>(GetProcAddress(library,"u_getUnicodeVersion"));
    if(!upper||!lower||!letter||!space||!category||!version)return 3;
    UVersionInfo v{};version(v);
    if(v[0]!=15||v[1]!=1||v[2]!=0||v[3]!=0){std::cerr<<"Unicode oracle version differs from 15.1.0\n";return 4;}
    for(std::uint32_t c=0;c<=0x10ffff;++c) {
        if(tiger::unicode::toUpper(c)!=static_cast<std::uint32_t>(upper(c)) ||
           tiger::unicode::toLower(c)!=static_cast<std::uint32_t>(lower(c)) ||
           tiger::unicode::isLetter(c)!=(letter(c)!=0) ||
           tiger::unicode::isWhitespace(c)!=(space(c)!=0) ||
           tiger::unicode::isDecimalDigit(c)!=(category(c)==U_DECIMAL_DIGIT_NUMBER)) {
            std::cerr<<"Mismatch U+"<<std::hex<<c<<'\n';return 5;
        }
    }
    if(tiger::ordinalCaseKey(u"\u0131\u017f\U00010428")!=u"\u0131\u017f\U00010400")return 6;
    if(tiger::ordinalCaseKey(std::u16string(1,0xd800))!=std::u16string(1,0xd800))return 7;
    FreeLibrary(library);
    std::cout<<"PASS: all 1,114,112 code points, five operations, Unicode 15.1; ordinal exceptions and surrogate handling.\n";
}
