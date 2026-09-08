#include "CandidateTheme.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace tiger {
const CandidateTheme& candidateTheme(std::u16string_view name) {
    static const std::array<CandidateTheme,9> themes={{
        {0xff000000,0xfffff8f3,0xff1a7b6b,0x48000000,1.25,{5,5,5,5}},
        {0xff2277ee,0x00000000,0x00000000,0x482277ee,1.25,{5,5,5,5}},
        {0xff2277ee,0x1a000000,0x00000000,0x482277ee,1.25,{5,5,5,5}},
        {0xffd9d9d9,0xff2f2f2f,0xff5a5a5a,0x48d9d9d9,1.25,{5,5,5,5}},
        {0xffffdc6a,0xff232b39,0xff3a6b9b,0x48ffdc6a,1.25,{5,5,5,5}},
        {0xff111111,0xfff5f2e8,0xffa8a09d,0x48111111,1.3,{5,5,5,5}},
        {0xff000000,0xfffdf9f5,0xffdeacac,0x48000000,1.25,{5,5,5,5}},
        {0xff71e4fd,0x88001122,0xfff651fc,0x4871e4fd,1.5,{10,0,10,0}},
        {0xff303030,0xfffdfdff,0xff56a1dd,0x48303030,1.25,{5,5,5,5}}
    }};
    for(std::size_t i=0;i<themes.size();++i) if(name==candidateThemeNames[i]) return themes[i];
    return themes[0];
}
namespace {
std::uint32_t over(std::uint32_t target,std::uint32_t source,unsigned coverage=255) {
    const unsigned alpha=((source>>24)*coverage+127)/255;
    std::uint32_t result=(alpha+((target>>24)*(255-alpha)+127)/255)<<24;
    for(unsigned shift=0;shift<24;shift+=8) {
        const auto channel=((((source>>shift)&255)*alpha+((target>>shift)&255)*(255-alpha)+127)/255);
        result|=channel<<shift;
    }
    return result;
}
bool inside(double x,double y,double width,double height,const std::array<double,4>& corners,double inset) {
    if(x<inset || y<inset || x>=width-inset || y>=height-inset) return false;
    const double maxRadius=std::max(0.0,std::min(width-2*inset,height-2*inset)/2);
    for(unsigned i=0;i<4;++i) {
        const double r=std::clamp(corners[i]-inset,0.0,maxRadius);
        const bool right=i==1 || i==2,bottom=i>=2;
        const double cx=right?width-inset-r:inset+r,cy=bottom?height-inset-r:inset+r;
        if((right?x>cx:x<cx) && (bottom?y>cy:y<cy)) return (x-cx)*(x-cx)+(y-cy)*(y-cy)<=r*r;
    }
    return true;
}
}
std::vector<std::uint32_t> renderCandidateTheme(int width,int height,double scale,
    const CandidateTheme& theme,const std::vector<PixelRect>& selection,const std::vector<std::uint32_t>& textMask) {
    if(width<=0 || height<=0 || !std::isfinite(scale) || scale<=0 ||
        static_cast<std::uint64_t>(width)*height!=textMask.size()) throw std::invalid_argument("Invalid candidate surface");
    std::vector<std::uint32_t> pixels(textMask.size());
    auto corners=theme.corners;
    for(auto& r:corners) r*=scale;
    const auto border=theme.borderWidth*scale;
    const double safe=std::ceil(*std::max_element(corners.begin(),corners.end())+border);
    const auto background=over(0,theme.background);
    for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
        const auto i=static_cast<std::size_t>(y)*width+x;
        unsigned outer=0,edge=0;
        const bool interior=x>=border && y>=border && x+1<=width-border && y+1<=height-border &&
            ((x>=safe && x+1<=width-safe) || (y>=safe && y+1<=height-safe));
        if(interior) outer=4;
        else for(double dy:{0.25,0.75}) for(double dx:{0.25,0.75}) {
            if(inside(x+dx,y+dy,width,height,corners,0)) {
                ++outer;
                if(!inside(x+dx,y+dy,width,height,corners,border)) ++edge;
            }
        }
        auto color=background;
        if(edge) color=over(color,theme.border,(edge*255+2)/4);
        for(const auto& rect:selection) if(x>=rect.left && x<rect.right && y>=rect.top && y<rect.bottom) color=over(color,theme.selection);
        const auto mask=textMask[i];
        const unsigned coverage=std::max({mask&255,(mask>>8)&255,(mask>>16)&255});
        if(coverage) color=over(color,theme.foreground,coverage);
        if(outer<4) {
            std::uint32_t clipped=0;
            for(unsigned shift=0;shift<32;shift+=8) clipped|=((((color>>shift)&255)*outer+2)/4)<<shift;
            color=clipped;
        }
        pixels[i]=color;
    }
    return pixels;
}
}
