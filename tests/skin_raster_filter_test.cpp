#include "../native/tsf/SkinRasterFilter.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint32_t gray(unsigned value,unsigned alpha){return (alpha<<24)|(value<<16)|(value<<8)|value;}
}
void verifySkinRasterFilter(){
    for(unsigned alpha:{0u,128u,249u,250u,253u,254u,255u}){
        std::vector<std::uint32_t> uniform(81,gray(alpha/2,alpha));
        const auto original=uniform;tiger::tsf::sharpenSkinRaster(uniform,9,9);
        require(uniform==original,"Sharpening changed a flat area");
        auto detail=original;detail[40]=gray(alpha*3/4,alpha);
        const auto before=detail;tiger::tsf::sharpenSkinRaster(detail,9,9);
        require((detail!=before)==(alpha>=250),"Near-opaque detail was not sharpened, or transparency was sharpened");
        for(std::size_t i=0;i<detail.size();++i){
            require((detail[i]>>24)==alpha,"Sharpening changed alpha");
            for(unsigned c=0;c<3;++c)require(((detail[i]>>(c*8))&255)<=alpha,"Invalid premultiplied color");
        }
    }
    // One translucent neighbor protects a whole edge neighborhood, even
    // when the center pixel itself is opaque and has strong RGB detail.
    std::vector<std::uint32_t> edge(81,gray(64,254));edge[40]=gray(192,254);edge[30]=gray(40,128);
    const auto before=edge;tiger::tsf::sharpenSkinRaster(edge,9,9);
    require(edge[40]==before[40] && edge[30]==before[30],"Translucent edge acquired a halo");
    // Exercise clamping at both color extremes and the untouched outer rim.
    std::vector<std::uint32_t> checker(81);
    for(unsigned i=0;i<81;++i)checker[i]=gray(i%2?254:0,254);
    const auto extreme=checker;tiger::tsf::sharpenSkinRaster(checker,9,9);
    require(checker==extreme,"Clipping or border preservation failed");
    auto tiny=std::vector<std::uint32_t>{0,0xffffffff};const auto small=tiny;
    tiger::tsf::sharpenSkinRaster(tiny,1,2);require(tiny==small,"Small raster changed");
    std::cout<<"Skin raster sharpening: alpha, edges, near-opaque detail, flat areas, clipping passed\n";
}
