#pragma once
#include <cstddef>
#include <cstdint>
namespace tiger::unicode {
namespace detail {
struct Mapping { std::uint32_t code, value; };
struct Range { std::uint32_t first, last; };
#include "UnicodeData.inc"
template<std::size_t N>
inline std::uint32_t map(std::uint32_t code,const Mapping (&entries)[N]) noexcept {
    std::size_t first=0,last=N;
    while(first<last) {const auto mid=first+(last-first)/2;if(entries[mid].code<code)first=mid+1;else last=mid;}
    return first<N && entries[first].code==code?entries[first].value:code;
}
template<std::size_t N>
inline bool contains(std::uint32_t code,const Range (&entries)[N]) noexcept {
    std::size_t first=0,last=N;
    while(first<last) {const auto mid=first+(last-first)/2;if(entries[mid].last<code)first=mid+1;else last=mid;}
    return first<N && entries[first].first<=code;
}
}
inline std::uint32_t toUpper(std::uint32_t c) noexcept {return detail::map(c,detail::upperMappings);}
inline std::uint32_t toLower(std::uint32_t c) noexcept {return detail::map(c,detail::lowerMappings);}
inline bool isLetter(std::uint32_t c) noexcept {return detail::contains(c,detail::letterRanges);}
inline bool isDecimalDigit(std::uint32_t c) noexcept {return detail::contains(c,detail::decimalRanges);}
inline bool isWhitespace(std::uint32_t c) noexcept {return detail::contains(c,detail::whitespaceRanges);}
}
