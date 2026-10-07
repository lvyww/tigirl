#pragma once
#include "SsfArchive.h"
#include <array>
namespace tiger::skin {
struct Insets { int top=0,bottom=0,left=0,right=0; };
struct Stretch { int mode=0,before=0,after=0; }; // 0 stretch, 1 tile
struct Decoration {
    std::u16string image;
    std::array<int,10> align{};
    bool explicitAlignment=false;
};
struct Layout {
    std::u16string image;
    Stretch horizontal,vertical;
    Insets code,candidates;
    std::vector<Decoration> decorations;
};
enum class Variant { Horizontal,Vertical,HorizontalCode,HorizontalCandidates,VerticalCode,VerticalCandidates };
struct Definition {
    std::u16string name=u"默认",author,font,englishFont,preview;
    float fontSize=17;
    float candidateSpacing=0,lineSpacing=0,characterSpacing=0;
    std::uint32_t codeColor=0xff333333,firstColor=0xff2869b0,textColor=0xff333333,annotationColor=0xff666666;
    std::map<Variant,Layout> layouts;
    Files assets;
    std::vector<std::u16string> warnings;
    const Layout* choose(bool vertical,bool code,bool candidates) const;
};
Definition parseDefinition(Files);
struct AxisSlice { float source=0,sourceLength=0,destination=0,destinationLength=0; };
// Resolves malformed author margins and tiny targets without zero/negative
// source rectangles. Tiling has a fixed drawing-work budget.
std::vector<AxisSlice> sliceAxis(unsigned source,float destination,Stretch);
}
