#define NOMINMAX
#include "../native/tsf/CandidateRenderer.h"
#include "../native/Text.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
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
void compactReview(const std::filesystem::path& root,const std::filesystem::path& output,const std::filesystem::path& baseline,const std::filesystem::path& font){
    std::ofstream report(output/L"compact-report.json");report<<"{\"cases\":[";unsigned cases=0;
    const std::vector<std::filesystem::path> fonts={font};
    struct CompactSkin {const char16_t* name;float minimum;};
    for(const auto& skin: {CompactSkin{u"雨竹听风.ssf",1455.f},CompactSkin{u"雪夜铃兰.ssf",1575.f},CompactSkin{u"霓虹电玩.ssf",1175.f}}){
    if(!std::filesystem::exists(root/skin.name))continue;
    const auto stem=std::filesystem::path(skin.name).stem();
    for(int mode:{2,4,5,6,7})for(double size:{17.,17.5,24.})for(bool longText:{false,true}){
        CandidateStyle style;style.theme=skin.name;style.setLayoutMode(mode);style.fontSize=size;style.skinAnimation=false;
        CandidatePresentation text;
        if(style.showCode)text.code=longText?u"ds":u"d";
        if(style.hideCandidates)text.placeholder=longText?u"abcdefghijklmnopqrstuvwx":u"d";
        else text.items=longText?std::vector<std::u16string>{u"输入候选内容增多之后自动撑开〔shū rù〕",u"第二个候选",u"第三个候选"}:std::vector<std::u16string>{u"中",u"哪个"};
        tsf::CandidateRenderer before(style,fonts,baseline),after(style,fonts,root);
        before.layout(text,1000);after.layout(text,1000);
        require(before.imageSkin() && after.imageSkin() && after.privateFamily(),"Compact review skin/font unavailable");
        require(after.height()==before.height(),"Horizontal compaction changed height");
        require(after.width()<=before.width() && after.items().size()==before.items().size(),"Compact skin lost content");
        if(!style.vertical){
            if(!longText)require(std::abs(after.width()-skin.minimum*static_cast<float>(size/100))<.02f,"Short layout did not contract to its fixed-end minimum");
            else require(after.width()>skin.minimum*static_cast<float>(size/100),"Long content did not expand compact layout");
        }
        require(after.items().size()==text.items.size(),"Compact skin clipped candidate count");
        for(std::size_t i=0;i<after.items().size();++i){
            const auto& a=after.items()[i];const auto& b=before.items()[i];
            require(a.left==b.left && a.top==b.top && a.right==b.right && a.bottom==b.bottom && a.right<=after.width(),"Compaction moved or clipped text");
        }
        for(UINT dpi:{96u,120u,144u,192u}){
            std::vector<std::uint32_t> a,b;before.render(dpi,0,a);after.render(dpi,0,b);pixelsValid(a);pixelsValid(b);
            if(style.vertical)require(a==b,"Compaction changed a vertical layout");
            const UINT width=after.pixelWidth(dpi),height=after.pixelHeight(dpi);
            const SIZE last{static_cast<LONG>(width),static_cast<LONG>(height)};
            std::vector<std::uint32_t> frame;after.render(dpi,0,frame,&last);
            require(frame==b,"Compact animation changed its final frame");
            const SIZE wider{static_cast<LONG>(width+13),static_cast<LONG>(height)};after.render(dpi,0,frame,&wider);
            for(UINT y=0;y<height;++y)for(UINT x=0;x<4;++x)
                require(frame[static_cast<std::size_t>(y)*(width+13)+x]==b[static_cast<std::size_t>(y)*width+x],"Compact width animation moved its bottom edge");
            if(size==17.5 && dpi==144){
                const auto label=stem.wstring()+L"-mode-"+std::to_wstring(mode)+(longText?L"-long":L"-short");
                bitmap(output/(label+L"-before.bmp"),before.pixelWidth(dpi),before.pixelHeight(dpi),a);
                bitmap(output/(label+L"-after.bmp"),after.pixelWidth(dpi),after.pixelHeight(dpi),b);
            }
            if(cases++)report<<',';
            report<<"{\"skin\":\""<<utf8(skin.name)<<"\",\"mode\":"<<mode<<",\"font\":"<<size<<",\"dpi\":"<<dpi<<",\"long\":"<<(longText?"true":"false")<<",\"before_width\":"<<before.pixelWidth(dpi)<<",\"after_width\":"<<after.pixelWidth(dpi)<<",\"height\":"<<after.pixelHeight(dpi)<<'}';
        }
        // A mistakenly tiny author minimum must still preserve both fixed ends.
        auto floorStyle=style;floorStyle.theme=stem.u16string()+u"-floor-test.ssf";
        tsf::CandidateRenderer floor(floorStyle,fonts,root);floor.layout(text,1000);
        require(floor.imageSkin() && floor.width()==after.width() && floor.height()==after.height(),"Tiny minimum squeezed fixed artwork");
    }
    }
    require(cases>0,"No compact skins available to test");
    report<<"],\"passed\":true,\"case_count\":"<<cases<<"}\n";
    require(bool(report),"Cannot save compact review report");
    std::cout<<"Compact skin cases passed: "<<cases<<'\n';
}
// Real-size art review, including the user's single/pair/code-only examples.
// This is opt-in so ordinary SSF regression runs keep their existing workload.
void verticalReview(const std::filesystem::path& root,const std::filesystem::path& output,const std::filesystem::path& baseline,const std::filesystem::path& font){
    std::ofstream report(output/L"vertical-report.json");report<<"{\"cases\":[";unsigned cases=0;
    const std::vector<std::filesystem::path> fonts={font};
    struct Spec{const char16_t* name;float width,height;};
    for(const auto& spec:{Spec{u"雨竹听风.ssf",1235,1185},Spec{u"雪夜铃兰.ssf",1230,1065},Spec{u"霓虹电玩.ssf",1225,1205}}){
        const auto stem=std::filesystem::path(spec.name).stem();
        for(int mode:{2,4,5,6,7})for(double size:{17.,17.5,24.})for(int scene=0;scene<5;++scene){
            CandidateStyle style;style.theme=spec.name;style.setLayoutMode(mode);style.fontSize=size;style.skinAnimation=false;
            CandidatePresentation text;
            if(style.showCode)text.code=u"d";
            if(style.hideCandidates)text.placeholder=u"d";
            else if(scene==0)text.items={u"中",u"哪个"};
            else if(scene==1)text.items={u"中"};
            else if(scene==4)text.items={u"这是较长的候选词组〔shì jiào cháng de hòu xuǎn cí zǔ〕",u"第二个候选"};
            else {const unsigned count=scene==2?5u:10u;for(unsigned i=0;i<count;++i)text.items.push_back(u"候选词");}
            tsf::CandidateRenderer before(style,fonts,baseline),after(style,fonts,root);
            before.layout(text,1000);after.layout(text,1000);
            require(before.imageSkin() && after.imageSkin() && after.privateFamily(),"Vertical review skin/font unavailable");
            require(after.width()<=before.width() && after.height()<=before.height(),"Vertical optimization enlarged the canvas");
            require(after.items().size()==text.items.size() && before.items().size()==text.items.size(),"Vertical optimization lost candidates");
            if(style.vertical){
                const float scale=static_cast<float>(size/100);
                require(after.width()+.02f>=spec.width*scale && after.height()+.02f>=spec.height*scale,"Fixed artwork was compressed");
                if(scene<2)require(std::abs(after.width()-spec.width*scale)<.02f && after.height()<before.height(),"Short vertical window did not contract");
                if(scene==3)require(after.height()>spec.height*scale,"Ten candidates did not expand the window");
            }
            for(std::size_t i=0;i<after.items().size();++i){
                const auto& a=after.items()[i];const auto& b=before.items()[i];
                require(a.left==b.left && a.top==b.top && a.right==b.right && a.bottom==b.bottom && a.right<=after.width() && a.bottom<=after.height(),"Vertical optimization moved or clipped text");
            }
            auto floorStyle=style;floorStyle.theme=stem.u16string()+u"-floor-test.ssf";
            tsf::CandidateRenderer floor(floorStyle,fonts,root);floor.layout(text,1000);
            require(floor.imageSkin() && floor.width()==after.width() && floor.height()==after.height(),"Tiny minimum squeezed fixed artwork");
            for(UINT dpi:{96u,120u,144u,192u}){
                std::vector<std::uint32_t> a,b,frame;before.render(dpi,0,a);after.render(dpi,0,b);pixelsValid(a);pixelsValid(b);
                if(!style.vertical)require(a==b,"Vertical optimization changed a horizontal layout");
                const UINT width=after.pixelWidth(dpi),height=after.pixelHeight(dpi);
                const SIZE last{static_cast<LONG>(width),static_cast<LONG>(height)};after.render(dpi,0,frame,&last);
                require(frame==b,"Vertical animation changed its final frame");
                const SIZE taller{static_cast<LONG>(width),static_cast<LONG>(height+13)};after.render(dpi,0,frame,&taller);
                for(UINT y=0;y<4;++y)for(UINT x=0;x<width;++x)
                    require(frame[static_cast<std::size_t>(y)*width+x]==b[static_cast<std::size_t>(y)*width+x],"Height animation moved the top edge");
                if(style.vertical && size==17.5 && dpi==144){
                    const auto label=stem.wstring()+L"-mode-"+std::to_wstring(mode)+L"-scene-"+std::to_wstring(scene);
                    bitmap(output/(label+L"-before.bmp"),before.pixelWidth(dpi),before.pixelHeight(dpi),a);
                    bitmap(output/(label+L"-after.bmp"),after.pixelWidth(dpi),after.pixelHeight(dpi),b);
                }
                if(cases++)report<<',';
                report<<"{\"skin\":\""<<utf8(spec.name)<<"\",\"mode\":"<<mode<<",\"font\":"<<size<<",\"dpi\":"<<dpi<<",\"scene\":"<<scene<<",\"before_width\":"<<before.pixelWidth(dpi)<<",\"after_width\":"<<width<<",\"before_height\":"<<before.pixelHeight(dpi)<<",\"after_height\":"<<height<<'}';
            }
        }
    }
    report<<"],\"passed\":true,\"case_count\":"<<cases<<"}\n";require(bool(report),"Cannot save vertical review report");
    std::cout<<"Vertical skin cases passed: "<<cases<<'\n';
}
void reviewSkins(const std::filesystem::path& root,const std::filesystem::path& output,const std::filesystem::path& baseline={}){
    const auto folder=output/L"review";std::filesystem::create_directories(folder);
    std::ofstream report(folder/L"review-report.json");report<<"{\"cases\":[";unsigned cases=0;
    for(const auto& name:skin::skinFiles(root)){
        if(name==u"默认.ssf")continue;
        for(int layout:{2,4,5,6,7})for(int scenario=0;scenario<6;++scenario)for(int font:{14,17,24}){
            CandidateStyle style;style.theme=name;style.setLayoutMode(layout);style.font=u"Microsoft YaHei UI";
            style.fontSize=font;style.skinFont=false;style.skinAnimation=false;
            CandidatePresentation text;
            if(style.showCode)text.code=u"d";
            if(!style.hideCandidates){
                switch(scenario){
                case 0:text.items={u"1 别〔bié 基本〕"};text.annotationOffsets={3};break;
                case 1:text.items={u"1 中〔zhōng 基本〕",u"2 哪个"};text.annotationOffsets={3,4};break;
                case 2:text.items={u"1 你好〔nǐ hǎo 基本〕",u"2 世界",u"3 输入法",u"4 虎娘",u"5 测试"};text.annotationOffsets={4,4,5,4,4};break;
                case 3:text.items={u"1 一",u"2 二",u"3 三",u"4 四",u"5 五",u"6 六",u"7 七",u"8 八",u"9 九",u"0 十"};break;
                case 4:break;
                case 5:text.items={u"1 长句与注释的显示边界检查〔cháng jù yǔ zhù shì，完整拼音注释〕"};text.annotationOffsets={15};break;
                }
            }
            if(layout==7 || (!style.showCode && text.items.empty()))text.placeholder=u"d";
            text.codeOnly=style.hideCandidates;
            tsf::CandidateRenderer renderer(style,{},root);renderer.layout(text,640);
            require(renderer.imageSkin(),"Review skin did not load");
            require(renderer.width()<=640.01f,"Review exceeded the available width");
            std::size_t baselineVisible=text.items.size();
            if(baseline.empty())require(renderer.items().size()==text.items.size(),"Review candidate count changed");
            else {
                tsf::CandidateRenderer previous(style,{},baseline);previous.layout(text,640);
                require(previous.imageSkin(),"Review baseline skin did not load");
                baselineVisible=previous.items().size();
                require(renderer.items().size()>=baselineVisible,"Recut skin reduced visible candidate count");
                require(renderer.width()<=previous.width()+.01f && renderer.height()<=previous.height()+.01f,"Recut skin exceeded baseline outer dimensions");
            }
            for(const auto& rect:renderer.items())require(rect.left>=0 && rect.top>=0 && rect.right<=renderer.width()+.01f && rect.bottom<=renderer.height()+.01f && rect.right>rect.left && rect.bottom>rect.top,"Review hit rectangle outside surface");
            for(UINT dpi:{96u,120u,144u,192u}){
                std::vector<std::uint32_t> pixels;renderer.render(dpi,0,pixels);pixelsValid(pixels);
                const auto label=std::filesystem::path(name).stem().wstring()+L"-"+std::to_wstring(layout)+L"-"+std::to_wstring(scenario)+L"-"+std::to_wstring(font)+L"-"+std::to_wstring(dpi);
                bitmap(folder/(label+L".bmp"),renderer.pixelWidth(dpi),renderer.pixelHeight(dpi),pixels);
                if(cases++)report<<',';
                report<<"{\"skin\":\""<<utf8(name)<<"\",\"layout\":"<<layout<<",\"scenario\":"<<scenario<<",\"font\":"<<font<<",\"dpi\":"<<dpi<<",\"width\":"<<renderer.pixelWidth(dpi)<<",\"height\":"<<renderer.pixelHeight(dpi)<<",\"requested_candidates\":"<<text.items.size()<<",\"visible_candidates\":"<<renderer.items().size()<<",\"baseline_visible\":"<<baselineVisible<<'}';
            }
        }
    }
    report<<"],\"passed\":true,\"case_count\":"<<cases<<"}\n";
    require(bool(report),"Cannot save skin review report");
    std::cout<<"Skin review cases passed: "<<cases<<"\n";
}
}
int wmain(int argc,wchar_t** argv){
    try{
        require(argc==3 || (argc==4 && std::wstring_view(argv[3])==L"--review") ||
            (argc==5 && std::wstring_view(argv[3])==L"--review-against") ||
            (argc==6 && (std::wstring_view(argv[3])==L"--compact-review" || std::wstring_view(argv[3])==L"--vertical-review")),
            "Usage: ssf_render_probe SKIN_DIRECTORY OUTPUT_DIRECTORY [--review | --review-against BASELINE_DIRECTORY | --compact-review BASELINE_DIRECTORY FONT_FILE | --vertical-review BASELINE_DIRECTORY FONT_FILE]");
        require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"COM unavailable");
        const auto root=std::filesystem::path(argv[1]),output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output);
        if(argc==6){
            if(std::wstring_view(argv[3])==L"--vertical-review")verticalReview(root,output,argv[4],argv[5]);
            else compactReview(root,output,argv[4],argv[5]);return 0;
        }
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
                    // Ceil-rounded surfaces may retain one transparent padding
                    // row/column outside the fractional painted extent. Internal
                    // slice edges must still be uniformly filled, including alpha.
                    for(LONG y=0;y<size.cy;++y)for(LONG x=0;x<size.cx;++x){
                        const auto pixel=pixels[static_cast<std::size_t>(y)*size.cx+x];
                        require(pixel==expected || (pixel==0 && (x==size.cx-1 || y==size.cy-1)),"Flat skin has a slice seam or transparency overlap");
                    }
                    ++checks;
                }
            }
            std::cout<<"Flat skin seam cases passed: "<<checks<<"\n";return 0;
        }
        std::ofstream report(output/L"render-report.json");report<<"{\"cases\":[";bool first=true;unsigned cases=0,animated=0;
        for(const auto& name:skin::skinFiles(root))for(int layout:{2,4,5,6,7})for(int mode=0;mode<4;++mode){
            const bool vertical=layout==3 || layout==4 || layout==6;
            CandidateStyle style;style.theme=name;style.setLayoutMode(layout);style.font=u"Microsoft YaHei UI";style.skinFont=false;style.fontSize=17;style.skinAnimation=false;
            CandidatePresentation presentation;
            if(layout==7)presentation.placeholder=u"abcd";else if(style.showCode)presentation.code=u"abcd";
            if(mode!=2 && !style.hideCandidates){presentation.items={u"1 你好〔注释〕",u"2 世界",u"3 输入法"};presentation.annotationOffsets={4,4,5};}
            if(mode==1 && !style.hideCandidates){presentation.items={u"1 你好〔注释〕",u"2 世界",u"3 输入法",u"4 虎娘",u"5 测试"};presentation.annotationOffsets={4,4,5,4,4};}
            if(mode==3 && !style.hideCandidates){presentation.items={u"1 这是一条用于检查装饰边界和长句显示的候选文本"};presentation.annotationOffsets.clear();}
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
        if(argc==4)reviewSkins(root,output);
        if(argc==5)reviewSkins(root,output,std::filesystem::path(argv[4]));
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}catch(HRESULT error){std::cerr<<"HRESULT "<<std::hex<<static_cast<unsigned>(error)<<'\n';return 2;}
}
