#include "UppercaseText.h"
#include <algorithm>
#include <charconv>
#include <system_error>
#include <cmath>
#include <limits>
namespace tiger {
namespace {
void trimZero(std::string& s) { auto first=s.find_first_not_of('0'); s=first==s.npos?"0":s.substr(first); }
void increment(std::string& s) {
    for(auto i=s.size();i>0;--i) { if(s[i-1]!='9') { ++s[i-1]; return; } s[i-1]='0'; }
    s.insert(s.begin(),'1');
}
std::string rounded(std::string digits,std::size_t drop,bool even) {
    if(!drop) return digits;
    if(digits.size()<=drop) digits.insert(0,drop+1-digits.size(),'0');
    const auto cut=digits.size()-drop;
    const bool up=digits[cut]>'5' || (digits[cut]=='5' && (!even || ((digits[cut-1]-'0')%2) || digits.find_first_not_of('0',cut+1)!=digits.npos));
    digits.resize(cut); if(up) increment(digits); trimZero(digits); return digits;
}
bool syntax(std::u16string_view number) {
    bool dot=false,after=false;
    for(auto ch:number) {
        if(ch==u'.' && !dot) { dot=true; after=false; }
        else if((ch>=u'0' && ch<=u'9') || ch==u',') after=true;
        else return false;
    }
    return after;
}
}
ManualTimerSpec parseManualTimer(std::u16string_view code) {
    if(code.size()<3 || code[0]!=u'D' || (code[1]!=u's' && code[1]!=u'S')) return {};
    auto suffix=code.substr(2);
    // .NET Regex '$' also matches immediately before one final LF.
    if(!suffix.empty() && suffix.back()==u'\n') suffix.remove_suffix(1);
    if(!syntax(suffix)) return {};
    ManualTimerSpec result{true,-1};
    std::string number;
    for(auto c:suffix) if(c!=u',') number+=static_cast<char>(c);
    if(number.empty()) return result;
    double minutes=0;
    const auto parsed=std::from_chars(number.data(),number.data()+number.size(),minutes,std::chars_format::general);
    if(parsed.ec==std::errc::result_out_of_range) {
        const auto first=number.find_first_of("123456789"),dot=number.find('.');
        if(first!=number.npos && (dot==number.npos || first<dot))
            result.milliseconds=std::numeric_limits<int>::max();
        return result;
    }
    if(parsed.ec!=std::errc{} || parsed.ptr!=number.data()+number.size() || minutes<=0) return result;
    const double due=minutes*60.0*1000.0;
    result.milliseconds=(!std::isfinite(due) || due>std::numeric_limits<int>::max())
        ?std::numeric_limits<int>::max():static_cast<int>(due);
    return result;
}
bool numericUppercasePrefix(std::u16string_view code) {
    if(code.size()<2 || (code[0]!=u'D' && code[0]!=u'd' && code[0]!=u'S')) return false;
    std::string number;
    for(auto ch:code.substr(1)) { if(ch>127) return false; number+=static_cast<char>(ch); }
    const auto first=number.find_first_not_of(" \t\r\n\v\f");
    if(first==number.npos) return false;
    number=number.substr(first,number.find_last_not_of(" \t\r\n\v\f")-first+1);
    std::string special=number;
    for(auto& c:special) if(c>='A' && c<='Z') c+= 'a'-'A';
    if(special=="nan" || special=="+nan" || special=="-nan" || special=="infinity" || special=="+infinity" || special=="-infinity") return true;
    if(number[0]=='+') number.erase(0,1);
    if(number.empty()) return false;
    // Reject implementation-specific from_chars forms (inf/nan(payload)).
    if(std::any_of(number.begin(),number.end(),[](char c) { return !(c>='0' && c<='9') && c!='.' && c!='e' && c!='E' && c!='+' && c!='-'; })) return false;
    double value=0; const auto result=std::from_chars(number.data(),number.data()+number.size(),value);
    return result.ptr==number.data()+number.size() && (result.ec==std::errc{} || result.ec==std::errc::result_out_of_range);
}
std::u16string uppercaseCurrency(std::u16string_view code) {
    if(code.empty() || code[0]!=u'S' || !syntax(code.substr(1))) return std::u16string(code);
    std::string digits; std::size_t scale=0; bool dot=false;
    for(auto ch:code.substr(1)) {
        if(ch==u'.') dot=true;
        else if(ch!=u',') { digits+=static_cast<char>(ch); if(dot) ++scale; }
    }
    constexpr std::u16string_view error=u"数字格式错误!";
    if(digits.empty()) return std::u16string(error);
    trimZero(digits);
    // Emulate decimal.TryParse's 96-bit coefficient and maximum scale 28,
    // rounding discarded digits to nearest-even before currency formatting.
    constexpr std::string_view maximum="79228162514264337593543950335";
    std::string coefficient;
    auto decimalScale=std::min<std::size_t>(scale,28);
    for(;;) {
        coefficient=rounded(digits,scale-decimalScale,true);
        if(coefficient.size()<maximum.size() || (coefficient.size()==maximum.size() && coefficient<=maximum)) break;
        if(!decimalScale) return std::u16string(error);
        --decimalScale;
    }
    if(decimalScale>2) coefficient=rounded(coefficient,decimalScale-2,false);
    else coefficient.append(2-decimalScale,'0');
    if(coefficient.size()<3) coefficient.insert(0,3-coefficient.size(),'0');
    const char jiao=coefficient[coefficient.size()-2],fen=coefficient.back();
    std::string integer=coefficient.substr(0,coefficient.size()-2); trimZero(integer);
    constexpr std::u16string_view names=u"零壹贰叁肆伍陆柒捌玖",units=u" 拾佰仟",groups=u" 万亿兆京垓秭穰";
    std::u16string output; bool zero=false;
    for(std::size_t i=0;i<integer.size();++i) {
        auto digit=integer[i]-'0'; const auto power=integer.size()-i-1;
        if(digit) { if(zero && !output.empty()) output+=u'零'; output+=names[digit]; if(power%4) output+=units[power%4]; zero=false; }
        else zero=true;
        if(power && power%4==0) {
            // Determine whether this four-digit group is nonzero.
            const auto groupStart=i>=3?i-3:0;
            bool nonzero=false;
            for(auto j=groupStart;j<=i;++j) if(integer[j]!='0') nonzero=true;
            if(nonzero) { output+=groups[power/4]; zero=false; }
        }
    }
    if(!output.empty()) output+=u'元';
    if(jiao!='0') { output+=names[jiao-'0']; output+=u'角'; }
    else if(fen!='0' && !output.empty()) output+=u'零';
    if(fen!='0') { output+=names[fen-'0']; output+=u'分'; }
    if(jiao=='0' && fen=='0' && !output.empty()) output+=u'整';
    return output;
}
}
