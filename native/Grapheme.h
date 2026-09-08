#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace tiger {
namespace detail {
#include "GraphemeData.inc"
inline int graphemeProperty(std::uint32_t code) {
    const auto end=std::end(graphemeRanges);
    const auto found=std::lower_bound(std::begin(graphemeRanges),end,code,
        [](const GraphemeRange& range,std::uint32_t value) { return range.last<value; });
    return found!=end && found->first<=code?found->value:0;
}
}
inline std::vector<std::u16string> wordTextElements(std::u16string_view text) {
    if(text.empty()) return {};
    if(text.size()>static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) throw std::length_error("Word too long");
    struct Point { std::size_t offset; int32_t property; bool pictographic; };
    std::vector<Point> points;
    for(std::size_t i=0;i<text.size();) {
        const auto offset=i; std::int32_t c=text[i++];
        if(c>=0xd800 && c<=0xdbff && i<text.size() && text[i]>=0xdc00 && text[i]<=0xdfff)
            c=0x10000+((c-0xd800)<<10)+(text[i++]-0xdc00);
        const auto property=detail::graphemeProperty(static_cast<std::uint32_t>(c));
        points.push_back({offset,property & 31,(property & 32)!=0});
    }
    // Match .NET StringInfo's UAX #29 rules, without ICU/CLDR's additional
    // Indic-conjunct tailoring (which differs from the reference engine).
    auto control=[](int p) { return p==detail::GCB_CONTROL || p==detail::GCB_CR || p==detail::GCB_LF; };
    std::vector<std::u16string> result; std::size_t start=0,regional=points[0].property==detail::GCB_REGIONAL_INDICATOR?1:0;
    for(std::size_t i=1;i<points.size();++i) {
        const auto a=points[i-1].property,b=points[i].property;
        bool boundary=true;
        if(a==detail::GCB_CR && b==detail::GCB_LF) boundary=false;
        else if(control(a) || control(b)) boundary=true;
        else if(a==detail::GCB_L && (b==detail::GCB_L || b==detail::GCB_V || b==detail::GCB_LV || b==detail::GCB_LVT)) boundary=false;
        else if((a==detail::GCB_LV || a==detail::GCB_V) && (b==detail::GCB_V || b==detail::GCB_T)) boundary=false;
        else if((a==detail::GCB_LVT || a==detail::GCB_T) && b==detail::GCB_T) boundary=false;
        else if(b==detail::GCB_EXTEND || b==detail::GCB_ZWJ || b==detail::GCB_SPACING_MARK || a==detail::GCB_PREPEND) boundary=false;
        else if(a==detail::GCB_ZWJ && points[i].pictographic) {
            auto j=i-1;
            while(j>0 && points[j-1].property==detail::GCB_EXTEND) --j;
            if(j>0 && points[j-1].pictographic) boundary=false;
        }
        else if(a==detail::GCB_REGIONAL_INDICATOR && b==detail::GCB_REGIONAL_INDICATOR && regional%2) boundary=false;
        if(boundary) { result.emplace_back(text.substr(start,points[i].offset-start)); start=points[i].offset; }
        regional=b==detail::GCB_REGIONAL_INDICATOR?regional+1:0;
    }
    result.emplace_back(text.substr(start));
    return result;
}
}
