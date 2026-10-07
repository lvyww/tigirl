#define NOMINMAX
#include "../native/tsf/CandidateRenderer.h"
#include "../native/Text.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <stdexcept>
using namespace tiger;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void bitmap(const std::filesystem::path& path,UINT width,UINT height,const std::vector<std::uint32_t>& pixels){
    BITMAPV4HEADER info{};info.bV4Size=sizeof(info);info.bV4Width=static_cast<LONG>(width);info.bV4Height=-static_cast<LONG>(height);info.bV4Planes=1;info.bV4BitCount=32;info.bV4V4Compression=BI_BITFIELDS;
    info.bV4RedMask=0x00ff0000;info.bV4GreenMask=0x0000ff00;info.bV4BlueMask=255;info.bV4AlphaMask=0xff000000;info.bV4CSType=LCS_sRGB;
    BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+static_cast<DWORD>(pixels.size()*4);
    auto straight=pixels;for(auto& p:straight){unsigned a=p>>24;if(!a){p=0;continue;}std::uint32_t value=a<<24;for(unsigned shift:{0u,8u,16u})value|=std::min(255u,(((p>>shift)&255)*255+a/2)/a)<<shift;p=value;}
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info),sizeof(info));out.write(reinterpret_cast<const char*>(straight.data()),static_cast<std::streamsize>(straight.size()*4));require(bool(out),"Cannot save render capture");
}
void pixelsValid(const std::vector<std::uint32_t>& pixels){
    require(!pixels.empty(),"Empty candidate surface");bool visible=false;
    for(auto pixel:pixels){const auto a=pixel>>24;visible|=a!=0;require((pixel&255)<=a && ((pixel>>8)&255)<=a && ((pixel>>16)&255)<=a,"Invalid premultiplied pixel");}
    require(visible,"Invisible candidate surface");
}
}
int wmain(int argc,wchar_t** argv){
    try{
        require(argc==3,"Usage: ssf_render_probe SKIN_DIRECTORY OUTPUT_DIRECTORY");
        require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"COM unavailable");
        const auto root=std::filesystem::path(argv[1]),output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output);
        if(std::filesystem::exists(root/L"flat-255-0.ssf")){
            unsigned checks=0;
            for(const auto& name:skin::skinFiles(root)){
                if(name==u"默认.ssf")continue;
                CandidateStyle style;style.theme=name;style.skinAnimation=false;
                tsf::CandidateRenderer renderer(style,{},root);CandidatePresentation blank;renderer.layout(blank,640);
                require(renderer.imageSkin(),"Flat fixture failed to load");
                for(UINT dpi:{96u,120u,144u,168u,192u})for(SIZE size:{SIZE{103,67},SIZE{104,68},SIZE{17,11}}){
                    std::vector<std::uint32_t> pixels;renderer.render(dpi,0,pixels,&size);
                    const auto expected=pixels.at(pixels.size()/2);
                    for(const auto pixel:pixels)require(pixel==expected,"Flat skin has a slice seam or transparency overlap");
                    ++checks;
                }
            }
            std::cout<<"Flat skin seam cases passed: "<<checks<<"\n";return 0;
        }
        std::ofstream report(output/L"render-report.json");report<<"{\"cases\":[";bool first=true;unsigned cases=0,animated=0;
        for(const auto& name:skin::skinFiles(root))for(int layout:{2,4,5,6,7})for(int mode=0;mode<3;++mode){
            const bool vertical=layout==3 || layout==4 || layout==6;
            CandidateStyle style;style.theme=name;style.setLayoutMode(layout);style.font=u"Microsoft YaHei UI";style.skinFont=false;style.fontSize=17;style.skinAnimation=false;
            CandidatePresentation presentation;
            if(layout==7)presentation.placeholder=u"abcd";else if(style.showCode)presentation.code=u"abcd";
            if(mode!=2 && !style.hideCandidates){presentation.items={u"1 你好〔注释〕",u"2 世界",u"3 输入法"};presentation.annotationOffsets={4,4,5};}
            if(mode==2 && !style.showCode)presentation.placeholder=u"abcd";
            presentation.codeOnly=style.hideCandidates;
            tsf::CandidateRenderer renderer(style,{},root);renderer.layout(presentation,640);
            require(renderer.imageSkin(),"Sample did not load as an image skin");
            if(name==u"战双帕弥什 · 21号.ssf" && mode==2 && layout==1){
                require(renderer.height()>=164.f*17.f/15.f-.01f,"Authored code canvas was compressed below text insets");
                require(renderer.width()>=359.f*17.f/15.f-.01f,"Authored code canvas width was compressed");
            }
            require(renderer.items().size()==presentation.items.size(),"Candidate hit count mismatch");
            for(const auto& rect:renderer.items())require(rect.left>=0 && rect.top>=0 && rect.right<=renderer.width()+.01 && rect.bottom<=renderer.height()+.01 && rect.right>rect.left && rect.bottom>rect.top,"Candidate hit rectangle outside surface");
            for(UINT dpi:{96u,120u,144u,192u}){
                std::vector<std::uint32_t> pixels;renderer.render(dpi,1,pixels);pixelsValid(pixels);
                require(pixels.size()==static_cast<std::size_t>(renderer.pixelWidth(dpi))*renderer.pixelHeight(dpi),"Surface dimensions mismatch");
                const auto label=std::filesystem::path(name).stem().wstring()+L"-"+std::to_wstring(layout)+L"-"+std::to_wstring(mode)+L"-"+std::to_wstring(dpi);
                bitmap(output/(label+L".bmp"),renderer.pixelWidth(dpi),renderer.pixelHeight(dpi),pixels);
                if(!first)report<<',';first=false;report<<"{\"skin\":\""<<utf8(name)<<"\",\"vertical\":"<<(vertical?"true":"false")<<",\"mode\":"<<mode<<",\"dpi\":"<<dpi<<",\"width\":"<<renderer.pixelWidth(dpi)<<",\"height\":"<<renderer.pixelHeight(dpi)<<'}';++cases;
            }
            if(layout==5 || layout==6){
                CandidatePresentation candidate;candidate.items={u"abcd"};
                CandidatePresentation empty;empty.placeholder=u"abcd";
                tsf::CandidateRenderer normal(style,{},root),placeholder(style,{},root);
                normal.layout(candidate,640);placeholder.layout(empty,640);
                require(normal.width()==placeholder.width() && normal.height()==placeholder.height() && placeholder.items().empty(),"Empty-code substitution moved the layout");
            }
            if(layout>=5){
                auto combinedStyle=style;combinedStyle.setLayoutMode(layout==6?4:2);
                tsf::CandidateRenderer combined(combinedStyle,{},root,2);combined.layout(presentation,640);
                std::vector<std::uint32_t> a,b;renderer.render(96,0,a);combined.render(96,0,b);
                require(renderer.width()==combined.width() && renderer.height()==combined.height() && a==b,"Content visibility changed the combined skin template");
                if(mode==2)require(renderer.items().empty(),"Empty code acquired candidate hit targets");
            }
            if(mode==0 && layout==2){
                auto selectedStyle=style;selectedStyle.skinAnimation=false;selectedStyle.font=u"Arial";
                auto alternateStyle=selectedStyle;alternateStyle.font=u"Courier New";
                CandidatePresentation text;text.code=u"iiiiWWWW";
                tsf::CandidateRenderer selected(selectedStyle,{},root),alternate(alternateStyle,{},root);
                selected.layout(text,640);alternate.layout(text,640);
                std::vector<std::uint32_t> a,b;selected.render(96,0,a);alternate.render(96,0,b);
                require(a!=b,"Settings font did not change rendered text");
            }
            if(mode==0){
                auto baseStyle=style;baseStyle.skinAnimation=false;baseStyle.fontSize=17;baseStyle.font=u"Arial";baseStyle.skinFont=false;
                auto enlargedStyle=baseStyle;enlargedStyle.fontSize=34;enlargedStyle.font=u"Arial";enlargedStyle.skinFont=true;
                tsf::CandidateRenderer base(baseStyle,{},root),enlarged(enlargedStyle,{},root);
                base.layout(presentation,640);enlarged.layout(presentation,1280);
                require(enlarged.width()==base.width()*2 && enlarged.height()==base.height()*2,"Skin outer bounds did not scale uniformly");
                require(base.items().size()==enlarged.items().size(),"Scaled hit item count differs");
                for(std::size_t i=0;i<base.items().size();++i){const auto& a=base.items()[i];const auto& b=enlarged.items()[i];
                    require(b.left==a.left*2 && b.top==a.top*2 && b.right==a.right*2 && b.bottom==a.bottom*2,"Scaled candidate hit positions differ");}
                std::vector<std::uint32_t> a,b;base.render(192,1,a);enlarged.render(96,1,b);
                require(a==b,"Global scale differs from DPI scale with the selected font");
            }
            if(const auto delay=renderer.skinDelay();delay){std::vector<std::uint32_t> before,after;renderer.render(96,0,before);Sleep(delay+25);require(renderer.skinFrameChanged(),"Skin animation clock did not advance");renderer.render(96,0,after);pixelsValid(after);require(before!=after,"Sample animation pixels did not change");++animated;}
        }
        for(const auto& name:{std::u16string(u"missing.ssf"),std::u16string(u"../outside.ssf")}){
            CandidateStyle style;style.theme=name;CandidatePresentation text;text.items={u"1 默认回退"};tsf::CandidateRenderer renderer(style,{},root);renderer.layout(text,640);std::vector<std::uint32_t> pixels;renderer.render(96,0,pixels);require(!renderer.imageSkin(),"Invalid skin did not fall back");pixelsValid(pixels);
        }
        CandidateStyle secure;secure.skinEnabled=false;secure.theme=u"小喵风扇.ssf";tsf::CandidateRenderer protectedRenderer(secure,{},root);CandidatePresentation text;text.items={u"安全模式"};protectedRenderer.layout(text,640);require(!protectedRenderer.imageSkin(),"Secure renderer loaded an external skin");
        Microsoft::WRL::ComPtr<IWICImagingFactory> imaging;require(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&imaging))),"WIC unavailable");
        std::ofstream frames(output/L"frame-report.json");frames<<'[';bool firstFrame=true;unsigned index=0;
        for(const auto& name:skin::skinFiles(root)){
            const auto resources=skin::loadResources(root,name,imaging.Get());
            for(const auto& entry:resources->images)for(std::size_t i=0;i<entry.second.frames.size();++i){
                const auto file="frame-"+std::to_string(index++)+".bgra";std::ofstream raw(output/file,std::ios::binary);const auto& pixels=entry.second.frames[i];pixelsValid(pixels);raw.write(reinterpret_cast<const char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()*4));require(bool(raw),"Cannot save frame pixels");
                if(!firstFrame)frames<<',';firstFrame=false;frames<<"{\"skin\":\""<<utf8(name)<<"\",\"asset\":\""<<utf8(entry.first)<<"\",\"frame\":"<<i<<",\"width\":"<<entry.second.width<<",\"height\":"<<entry.second.height<<",\"file\":\""<<file<<"\"}";
            }
        }
        frames<<"]\n";report<<"],\"passed\":true,\"case_count\":"<<cases<<",\"animated_layouts\":"<<animated<<",\"decoded_frames\":"<<index<<"}\n";
        std::cout<<"{\"passed\":true,\"cases\":"<<cases<<",\"animated_layouts\":"<<animated<<",\"decoded_frames\":"<<index<<"}\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}catch(HRESULT error){std::cerr<<"HRESULT "<<std::hex<<static_cast<unsigned>(error)<<'\n';return 2;}
}
