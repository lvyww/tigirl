#define NOMINMAX
#include "../native/tsf/CandidateRenderer.h"
#include "../native/Text.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tiger;
void verifySkinRasterFilter();
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void valid(const std::vector<std::uint32_t>& pixels){
    for(auto p:pixels){const auto a=p>>24;require((p&255)<=a && ((p>>8)&255)<=a && ((p>>16)&255)<=a,"Invalid premultiplied pixel");}
}
void capture(const std::filesystem::path& path,UINT width,UINT height,const std::vector<std::uint32_t>& pixels){
    BITMAPV4HEADER info{};info.bV4Size=sizeof(info);info.bV4Width=static_cast<LONG>(width);info.bV4Height=-static_cast<LONG>(height);info.bV4Planes=1;info.bV4BitCount=32;info.bV4V4Compression=BI_BITFIELDS;
    info.bV4RedMask=0x00ff0000;info.bV4GreenMask=0x0000ff00;info.bV4BlueMask=255;info.bV4AlphaMask=0xff000000;info.bV4CSType=LCS_sRGB;
    BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size()*4);
    auto straight=pixels;
    for(auto& p:straight){const auto a=p>>24;if(!a){p=0;continue;}std::uint32_t v=a<<24;for(unsigned shift:{0u,8u,16u})v|=std::min(255u,(((p>>shift)&255)*255+a/2)/a)<<shift;p=v;}
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&header),sizeof(header));out.write(reinterpret_cast<const char*>(&info),sizeof(info));out.write(reinterpret_cast<const char*>(straight.data()),static_cast<std::streamsize>(straight.size()*4));require(bool(out),"Cannot save capture");
}
double ms(std::chrono::steady_clock::time_point start){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
}
int wmain(int argc,wchar_t** argv){
    try{
        verifySkinRasterFilter();
        require(argc==3 || (argc==4 && std::wstring_view(argv[3])==L"--capture-only"),"probe <fixtures> <output> [--capture-only]");
        require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"COM unavailable");
        Microsoft::WRL::ComPtr<IWICImagingFactory> imaging;
        require(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&imaging))),"WIC unavailable");
        const std::filesystem::path root=argv[1],output=argv[2];std::filesystem::create_directories(output);
        const bool verify=argc==3;
        std::ofstream report(output/L"report.json");report<<"{\"quality\":[";unsigned cases=0;
        for(bool alpha:{false,true}){
            CandidateStyle style;style.theme=alpha?u"scaling-alpha.ssf":u"scaling-opaque.ssf";style.fontSize=17.5;style.font=u"Segoe UI";style.skinAnimation=false;
            tsf::CandidateRenderer renderer(style,{},root);renderer.layout({},4096);require(renderer.imageSkin(),"Pattern failed to load");
            for(UINT dpi:{96u,120u,144u,192u}){
                std::vector<std::uint32_t> pixels;renderer.render(dpi,0,pixels);valid(pixels);
                const UINT width=renderer.pixelWidth(dpi),height=renderer.pixelHeight(dpi);
                double squared=0;unsigned count=0;
                for(UINT y=8;y+8<height;++y)for(UINT x=8;x+8<width;++x){
                    if(std::abs(static_cast<int>(y)-static_cast<int>(height/2))<8)continue;
                    const auto p=pixels[static_cast<std::size_t>(y)*width+x];
                    const auto value=alpha?p>>24:p&255;
                    squared+=(static_cast<double>(value)-85)*(static_cast<double>(value)-85);++count;
                    if(alpha)require((p&0xffff)==0 && ((p>>16)&255)==(p>>24),"Transparent colors bled into the red edge");
                }
                const double error=std::sqrt(squared/count);
                if(cases++)report<<',';report<<"{\"alpha\":"<<(alpha?"true":"false")<<",\"dpi\":"<<dpi<<",\"rms_alias\":"<<error<<'}';report.flush();
                // Profile E deliberately retains more detail at 1.5x the target
                // resolution. Allow at most 10% of the channel range as residual
                // contrast in this above-Nyquist pattern. Unfiltered D2D bilinear
                // measures about 84/255 here, versus E's worst case 21/255 at 192 DPI.
                if(verify)require(error<25.5,"High frequency pattern exceeds the aliasing budget");
                capture(output/(std::wstring(alpha?L"alpha-":L"opaque-")+std::to_wstring(dpi)+L".bmp"),width,height,pixels);
                const SIZE wider{static_cast<LONG>(width+17),static_cast<LONG>(height)};
                std::vector<std::uint32_t> other;renderer.render(dpi,0,other,&wider);renderer.render(dpi,0,other);require(other==pixels,"Render target recreation changed filtered pixels");
                renderer.render(dpi+24,0,other);renderer.render(dpi,0,other);require(other==pixels,"DPI round trip changed filtered pixels");
            }
            // Evict old scale entries; returning to a DPI must reproduce a fresh renderer.
            std::vector<std::uint32_t> initial,again;renderer.render(96,0,initial);
            for(UINT dpi=48;dpi<=480;dpi+=12){renderer.render(dpi,0,again);valid(again);}
            renderer.render(96,0,again);require(initial==again,"Scale cache eviction corrupted a frame");
            style.fontSize=100;tsf::CandidateRenderer original(style,{},root);original.layout({},4096);original.render(96,0,again);
            const auto decoded=skin::loadResources(root,style.theme,imaging.Get());
            require(again==decoded->images.begin()->second.frames.front(),"Native-size image changed");
        }
        CandidateStyle animatedStyle;animatedStyle.theme=u"scaling-animated.ssf";animatedStyle.fontSize=17.5;animatedStyle.font=u"Segoe UI";
        tsf::CandidateRenderer animated(animatedStyle,{},root);animated.layout({},4096);
        std::vector<std::uint32_t> first,second;animated.render(144,0,first);second=first;
        for(int n=0;n<8 && second==first;++n){Sleep(25);animated.render(144,0,second);}
        require(first!=second,"Scaled APNG did not advance");valid(second);
        report<<"],\"renders\":[";unsigned renders=0;
        for(const auto& name:skin::skinFiles(root)){
            if(name==u"默认.ssf" || name.find(u"scaling-")==0)continue;
            for(int mode:{2,4,5,6,7})for(UINT dpi:{96u,120u,144u,192u}){
                CandidateStyle style;style.theme=name;style.font=u"Microsoft YaHei UI";style.fontSize=17.5;style.skinAnimation=false;style.setLayoutMode(mode);
                tsf::CandidateRenderer renderer(style,{},root);CandidatePresentation text;
                if(style.showCode)text.code=u"d";if(style.hideCandidates)text.placeholder=u"d";else text.items={u"中",u"哪个"};
                renderer.layout(text,640);require(renderer.imageSkin(),"Review skin unavailable");
                std::vector<std::uint32_t> pixels;auto start=std::chrono::steady_clock::now();renderer.render(dpi,0,pixels);const double cold=ms(start);valid(pixels);
                const auto label=std::filesystem::path(name).stem().wstring()+L"-"+std::to_wstring(mode)+L"-"+std::to_wstring(dpi);
                capture(output/(label+L".bmp"),renderer.pixelWidth(dpi),renderer.pixelHeight(dpi),pixels);
                start=std::chrono::steady_clock::now();
                for(int n=0;n<24;++n){const SIZE frame{static_cast<LONG>(renderer.pixelWidth(dpi)+n%12),static_cast<LONG>(renderer.pixelHeight(dpi))};renderer.render(dpi,0,pixels,&frame);}
                const double warm=ms(start)/24;
                if(renders++)report<<',';report<<"{\"skin\":\""<<utf8(name)<<"\",\"mode\":"<<mode<<",\"dpi\":"<<dpi<<",\"width\":"<<renderer.pixelWidth(dpi)<<",\"height\":"<<renderer.pixelHeight(dpi)<<",\"cold_ms\":"<<cold<<",\"animation_ms\":"<<warm<<'}';
            }
        }
        report<<"],\"passed\":"<<(verify?"true":"false")<<"}\n";require(bool(report),"Cannot save report");
        std::cout<<"Scaling quality cases: "<<cases<<"; real-skin captures: "<<renders<<"; DPI/recreation/native-size/APNG checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    catch(HRESULT hr){std::cerr<<"HRESULT "<<std::hex<<static_cast<unsigned>(hr)<<'\n';return 2;}
}
