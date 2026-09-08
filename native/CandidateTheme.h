#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>
namespace tiger {
inline constexpr std::array<std::u16string_view,9> candidateThemeNames={u"默认",u"通透",u"一般通透",u"迷雾",u"星夜",u"纸",u"粉",u"赛博朋克",u"清晨"};
struct CandidateTheme {
    std::uint32_t foreground,background,border,selection;
    double borderWidth;
    std::array<double,4> corners; // top-left, top-right, bottom-right, bottom-left
};
const CandidateTheme& candidateTheme(std::u16string_view name);
struct PixelRect { int left,top,right,bottom; };
// Output is premultiplied BGRA for a Windows layered window. Text mask is the
// white-on-black GDI coverage image; no UI/Windows calls occur in this renderer.
std::vector<std::uint32_t> renderCandidateTheme(int width,int height,double scale,
    const CandidateTheme& theme,const std::vector<PixelRect>& selection,
    const std::vector<std::uint32_t>& textMask);
}
