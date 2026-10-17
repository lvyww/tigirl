#define NOMINMAX
#include "../native/tsf/CandidateRenderer.h"
#include "../native/tsf/FrameTransition.h"
#include "../native/Text.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace tiger;
using namespace tiger::tsf;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int wmain(int argc,wchar_t** argv){
    try{
        require(argc==2 || argc==3,"probe <skin-directory> [private-font.ttf]");
        require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"COM unavailable");
        const std::filesystem::path root=argv[1];
        const std::vector<std::filesystem::path> fonts=argc==3?std::vector<std::filesystem::path>{argv[2]}:std::vector<std::filesystem::path>{};
        auto names=skin::skinFiles(root);names.push_back(u"missing.ssf"); // Built-in fallback too.
        const auto rain=std::find(names.begin(),names.end(),u"雨竹听风.ssf");
        if(rain!=names.end())std::rotate(names.begin(),rain,rain+1);
        unsigned cases=0,failures=0,animationFrames=0;
        for(const auto& name:names)for(int mode:{2,4,5,6,7})for(double size:{17.,17.5,24.}){
            CandidateStyle style;style.theme=name;style.setLayoutMode(mode);style.fontSize=size;style.skinAnimation=false;
            if(fonts.empty())style.font=u"Segoe UI"; // Clean CI has no downloaded font data.
            CandidateRenderer renderer(style,fonts,root);
            if(!fonts.empty())require(renderer.privateFamily(),"User font fell back to a system font");
            CandidatePresentation text;
            if(style.showCode)text.code=u"d";
            if(style.hideCandidates)text.placeholder=u"d";
            else text.items={u"1 中〔zhōng 基本〕",u"2 哪个"};
            renderer.layout(text,640);
            const float initialHeight=renderer.height();
            if(style.showCode)text.code=u"ds";
            if(style.hideCandidates)text.placeholder=u"ds";
            else text.items={u"1 输入后的候选文字〔shū rù 基本〕",u"2 另一个候选"};
            renderer.layout(text,640);
            require(renderer.height()==initialHeight,"d -> ds changed the row count or logical height");
            for(UINT dpi:{96u,120u,144u,192u}){
                const LONG width=static_cast<LONG>(renderer.pixelWidth(dpi)),height=static_cast<LONG>(renderer.pixelHeight(dpi));
                std::vector<std::uint32_t> final,animated;
                renderer.render(dpi,0,final);
                const SIZE same{width,height};renderer.render(dpi,0,animated,&same);
                // Publishing the last animation frame must not move any border,
                // ornament or text pixel when switching to the cached final frame.
                if(final!=animated){
                    if(failures++<5)std::cerr<<"Final-frame mismatch: "<<utf8(name)<<" mode="<<mode<<" font="<<size<<" dpi="<<dpi<<" surface="<<width<<'x'<<height<<'\n';
                }
                // A width-only transition must not resize the left fixed slice
                // vertically; exercise both directions through the real clock.
                for(bool growing:{false,true}){
                    FrameTransition transition;transition.Start({0,0,width+(growing?0:37),height},{0,0,width+(growing?37:0),height},1000,60,70);
                    for(unsigned elapsed:{0u,10u,30u,50u,60u,70u}){
                        const auto frame=transition.Sample(1000+elapsed);
                        require(frame.height==height,"Width transition changed the window height");
                        const SIZE extent{frame.width,frame.height};renderer.render(dpi,0,animated,&extent);
                        bool stable=true;
                        for(LONG y=0;y<height && stable;++y)for(LONG x=0;x<4;++x)
                            if(final[static_cast<std::size_t>(y)*width+x]!=animated[static_cast<std::size_t>(y)*frame.width+x]){stable=false;break;}
                        if(!stable){
                            if(failures++<5)std::cerr<<"Width-only frame moved fixed left edge: "<<utf8(name)<<" mode="<<mode<<" font="<<size<<" dpi="<<dpi<<" elapsed="<<elapsed<<'\n';
                        }
                        ++animationFrames;
                    }
                }
                ++cases;
            }
        }
        std::cout<<"{\"cases\":"<<cases<<",\"animation_frames\":"<<animationFrames<<",\"failures\":"<<failures<<"}\n";
        return failures?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    catch(HRESULT hr){std::cerr<<"HRESULT "<<std::hex<<static_cast<unsigned>(hr)<<'\n';return 2;}
}
