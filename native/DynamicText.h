#pragma once
#include <random>
#include <string>
#include <string_view>
namespace tiger {
struct LocalTime {
    int year=1970,month=1,day=1,hour=0,minute=0,second=0,weekday=4;
    std::u16string weekdayName=u"Thursday";
};
LocalTime localTime();
unsigned dynamicSeed();
// Fixed time is used by deterministic differential tests; production reads local
// time only for recognized clock tokens. Random choice state belongs to the engine.
std::u16string expandDynamicText(std::u16string_view text,std::minstd_rand& random,const LocalTime* fixed=nullptr);
}
