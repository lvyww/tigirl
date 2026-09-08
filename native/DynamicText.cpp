#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include "DynamicText.h"
#include "Text.h"
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <locale>
#include <sstream>
#include <vector>
namespace tiger {
unsigned dynamicSeed() {
    static std::atomic<unsigned> counter{1};
    const auto ticks=static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
    return static_cast<unsigned>(ticks^(ticks>>32))^counter.fetch_add(0x9e3779b9u,std::memory_order_relaxed);
}
LocalTime localTime() {
    LocalTime result;
#ifdef _WIN32
    SYSTEMTIME time{}; GetLocalTime(&time);
    result.year=time.wYear; result.month=time.wMonth; result.day=time.wDay;
    result.hour=time.wHour; result.minute=time.wMinute; result.second=time.wSecond; result.weekday=time.wDayOfWeek;
    wchar_t name[128];
    const auto type=static_cast<LCTYPE>(LOCALE_SDAYNAME1+(result.weekday+6)%7);
    if(GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT,type,name,128)) result.weekdayName.assign(reinterpret_cast<const char16_t*>(name));
#else
    const auto now=std::time(nullptr); std::tm time{};
    if(localtime_r(&now,&time)) {
        result.year=time.tm_year+1900; result.month=time.tm_mon+1; result.day=time.tm_mday;
        result.hour=time.tm_hour; result.minute=time.tm_min; result.second=time.tm_sec; result.weekday=time.tm_wday;
        std::ostringstream text;
        try { text.imbue(std::locale("")); } catch(...) {}
        text<<std::put_time(&time,"%A"); result.weekdayName=utf16(text.str());
    }
#endif
    return result;
}
std::u16string expandDynamicText(std::u16string_view text,std::minstd_rand& random,const LocalTime* fixed) {
    if(text.size()<2 || text.front()!=u'{' || text.back()!=u'}') return std::u16string(text);
    if(text.size()>3 && text.find(u'|')!=text.npos) {
        auto body=text.substr(1,text.size()-2);
        std::vector<std::u16string_view> choices;
        while(!body.empty()) {
            const auto split=body.find(u'|');
            if(split!=0) choices.push_back(body.substr(0,split));
            if(split==body.npos) break;
            body.remove_prefix(split+1);
        }
        if(!choices.empty()) return std::u16string(choices[std::uniform_int_distribution<std::size_t>(0,choices.size()-1)(random)]);
    }
    const bool date=text==u"{日期}" || text==u"{日期.}" || text==u"{日期-}" || text==u"{日期/}";
    const bool time=text==u"{时分秒}" || text==u"{时分}";
    if(!date && !time && text!=u"{星期}" && text!=u"{周}") return std::u16string(text);
    const auto now=fixed?*fixed:localTime();
    if(text==u"{星期}") return now.weekdayName;
    if(text==u"{周}") {
        constexpr std::u16string_view names[]={u"周日",u"周一",u"周二",u"周三",u"周四",u"周五",u"周六"};
        return now.weekday>=0 && now.weekday<7?std::u16string(names[now.weekday]):std::u16string(text);
    }
    auto digits=[](int value,std::size_t count) {
        const auto s=std::to_string(value); std::u16string result(s.begin(),s.end());
        if(result.size()<count) result.insert(0,count-result.size(),u'0');
        return result;
    };
    if(date) {
        const char16_t separator=text==u"{日期}"?u'年':text[3];
        return digits(now.year,4)+separator+digits(now.month,2)+(separator==u'年'?u'月':separator)+digits(now.day,2)+(separator==u'年'?u"日":u"");
    }
    return digits(now.hour,2)+u":"+digits(now.minute,2)+(text==u"{时分秒}"?u":"+digits(now.second,2):u"");
}
}
