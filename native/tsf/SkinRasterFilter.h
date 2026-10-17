#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tiger::tsf {
// Profile E from the desktop sampling review. Keep the intermediate bitmap
// larger than the physical target so the final D2D pass retains fine detail.
inline constexpr float skinRasterOversample=1.5f;

inline void sharpenSkinRaster(std::vector<std::uint32_t>& pixels,unsigned width,unsigned height){
    if(width<3 || height<3)return;
    constexpr float amount=0.30f,threshold=1.f;
    constexpr int kernel[3]={1,2,1};
    const auto input=pixels;
    for(unsigned y=1;y+1<height;++y)for(unsigned x=1;x+1<width;++x){
        const auto index=static_cast<std::size_t>(y)*width+x;
        bool opaque=true;float blur[3]={};
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
            const auto row=static_cast<std::size_t>(static_cast<int>(y)+dy);
            const auto column=static_cast<unsigned>(static_cast<int>(x)+dx);
            const auto p=input[row*width+column];
            // Authored PNGs often use alpha 253/254 in solid artwork. Include
            // these pixels, but leave translucent silhouettes and their halo alone.
            if((p>>24)<250)opaque=false;
            const float weight=static_cast<float>(kernel[dy+1]*kernel[dx+1])/16.f;
            for(unsigned c=0;c<3;++c)
                blur[c]+=((p>>(c*8))&255)*(255.f/std::max(1u,p>>24))*weight;
        }
        if(!opaque)continue;
        const auto alpha=input[index]>>24;std::uint32_t result=alpha<<24;
        for(unsigned c=0;c<3;++c){
            const float original=((input[index]>>(c*8))&255)*(255.f/alpha);
            const float detail=original-blur[c];
            const float value=original+(std::abs(detail)>threshold?amount*detail:0.f);
            result|=static_cast<unsigned>(std::clamp(std::round(value*alpha/255.f),0.f,static_cast<float>(alpha)))<<(c*8);
        }
        pixels[index]=result;
    }
}
}
