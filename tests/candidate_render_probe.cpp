#define NOMINMAX
#include "../native/tsf/CandidateRenderer.h"
#include "../native/tsf/CandidateDpi.h"
#include "../native/tsf/CandidateFrame.h"
#include "../native/tsf/CandidatePlacement.h"
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace tiger;
using namespace tiger::tsf;
using Microsoft::WRL::ComPtr;
void check(HRESULT hr){if(FAILED(hr))throw hr;}
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
void png(const std::filesystem::path& path,UINT w,UINT h,const std::vector<std::uint32_t>& input) {
    std::vector<std::uint32_t> pixels=input;
    for(auto& p:pixels){const unsigned a=p>>24; if(a){unsigned out=a<<24;for(unsigned s=0;s<24;s+=8)out|=std::min(255u,((p>>s&255)*255+a/2)/a)<<s;p=out;}}
    ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICStream> stream;check(factory->CreateStream(&stream));check(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;check(encoder->CreateNewFrame(&frame,nullptr));check(frame->Initialize(nullptr));check(frame->SetSize(w,h));
    auto format=GUID_WICPixelFormat32bppBGRA;check(frame->SetPixelFormat(&format));require(format==GUID_WICPixelFormat32bppBGRA,"PNG format rejected");
    check(frame->WritePixels(h,w*4,static_cast<UINT>(pixels.size()*4),reinterpret_cast<BYTE*>(pixels.data())));check(frame->Commit());check(encoder->Commit());
}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==3,"probe <private-font.ttf> <output-dir>");check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
        for(auto context:{DPI_AWARENESS_CONTEXT_UNAWARE,DPI_AWARENESS_CONTEXT_SYSTEM_AWARE,DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2}) {
            const auto original=SetThreadDpiAwarenessContext(context);require(original!=nullptr,"Cannot set test DPI context");
            HWND owner=CreateWindowExW(0,L"STATIC",L"DPI test",WS_POPUP,100,100,300,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            require(owner!=nullptr,"Cannot create hidden DPI owner");
            RECT logical{};require(GetWindowRect(owner,&logical)!=FALSE,"Cannot read owner geometry");
            const auto physical=candidatePhysicalCaret(logical,owner);
            {
                CandidateDpiScope scope;require(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)!=FALSE,"Candidate not PMv2");
                RECT actual{};require(GetWindowRect(owner,&actual)!=FALSE,"Cannot read physical geometry");
                require(std::abs(actual.left-physical.left)<=1 && std::abs(actual.top-physical.top)<=1 &&
                    std::abs(actual.right-physical.right)<=1 && std::abs(actual.bottom-physical.bottom)<=1,"Virtualized caret conversion mismatch");
                HWND candidate=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"Candidate DPI",WS_POPUP,physical.left,physical.bottom,100,100,owner,nullptr,GetModuleHandleW(nullptr),nullptr);
                require(candidate && AreDpiAwarenessContextsEqual(GetWindowDpiAwarenessContext(candidate),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2),"Candidate window did not retain PMv2");
                DestroyWindow(candidate);
            }
            require(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),context)!=FALSE,"Host DPI context not restored");
            DestroyWindow(owner);SetThreadDpiAwarenessContext(original);
        }
        {
            CandidateDpiScope scope;
            HWND window=CreateWindowExW(WS_EX_LAYERED|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,L"STATIC",L"Atomic frame probe",WS_POPUP,50,60,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            require(window!=nullptr,"Cannot create frame probe");
            RECT original{};GetWindowRect(window,&original);
            HDC memory=CreateCompatibleDC(nullptr);
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=120;info.bmiHeader.biHeight=-80;
            info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* bits=nullptr;
            HBITMAP bitmap=CreateDIBSection(memory,&info,DIB_RGB_COLORS,&bits,nullptr,0);
            require(memory && bitmap && bits,"Cannot prepare backing frame");
            auto old=SelectObject(memory,bitmap);
            std::fill_n(static_cast<std::uint32_t*>(bits),120*80,0xffee9933u);
            RECT prepared{};GetWindowRect(window,&prepared);
            require(EqualRect(&original,&prepared) && !IsWindowVisible(window),"Preparation changed window geometry or visibility");
            BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
            require(!publishCandidateFrame(window,nullptr,{300,200},{120,80},blend),"Incomplete frame accepted");
            GetWindowRect(window,&prepared);require(EqualRect(&original,&prepared),"Rejected frame changed geometry");
            require(publishCandidateFrame(window,memory,{300,200},{120,80},blend),"Frame publication failed");
            RECT published{};GetWindowRect(window,&published);
            require(published.left==300 && published.top==200 && published.right==420 && published.bottom==280,"Pixels and final geometry not published together");
            require(!IsWindowVisible(window),"Publication showed intentionally hidden window");
            require(publishCandidateFrame(window,memory,{200,300},{60,40},blend),"Shrinking frame failed");
            GetWindowRect(window,&published);
            require(published.left==200 && published.top==300 && published.right==260 && published.bottom==340,"Shrinking frame kept stale geometry");
            SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);DestroyWindow(window);
        }
        const std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
        const std::vector<std::filesystem::path> files={argv[1]};
        {
            CandidateStyle style;CandidateRenderer renderer(style,files);
            renderer.layout({u"ab",{u"1 候选文字",u"2 字号保持正常"}},400);
            std::vector<std::uint32_t> full,clippedPixels;renderer.render(96,0,full);
            const auto width=renderer.pixelWidth(96),height=renderer.pixelHeight(96);
            SIZE clip{static_cast<LONG>(width/2),static_cast<LONG>(height)};
            renderer.render(96,0,clippedPixels,&clip);
            // Interior text pixels remain at the same coordinates, not scaled.
            for(UINT y=12;y+12<height;++y)for(UINT x=12;x+12<static_cast<UINT>(clip.cx);++x)
                require(full[y*width+x]==clippedPixels[y*clip.cx+x],"Animation stretched text instead of clipping");
        }
        unsigned cases=0;
        for(bool vertical:{false,true})for(const auto theme:candidateThemeNames) {
            CandidateStyle style;style.vertical=vertical;style.theme=theme;
            CandidateRenderer renderer(style,files);require(renderer.privateFamily(),"Bundled family fell back to a system font");
            CandidatePresentation content{u"ab 〔1/2〕",{u"1 交 [ab]",u"2 疒 · 注释",u"3 测试汉字 Aa 123",u"4 𠮷 e\u0301 😀"}};
            renderer.layout(content,340.f);require(renderer.width()<=340.f,"Work-area width limit exceeded");
            for(const auto& r:renderer.items())require(r.left>=0 && r.top>=0 && r.right<=renderer.width() && r.bottom<=renderer.height(),"Hit target outside surface");
            std::vector<std::uint32_t> reference;
            for(UINT dpi:{96u,120u,144u,192u,96u}) {
                std::vector<std::uint32_t> pixels;renderer.render(dpi,0,pixels);
                std::vector<std::uint32_t> unselected,second;
                renderer.render(dpi,UINT_MAX,unselected);renderer.render(dpi,1,second);
                require(pixels==unselected,"First candidate still has a highlight");
                require(second!=unselected,"Non-first selection lost its highlight");
                require(renderer.pixelWidth(dpi)==static_cast<UINT>(std::ceil(renderer.width()*dpi/96.)),"Physical width mismatch");
                std::size_t ink=0;
                for(auto p:pixels){const auto a=p>>24;require((p&255)<=a && (p>>8&255)<=a && (p>>16&255)<=a,"Invalid premultiplied alpha");if(a)++ink;}
                require(ink>100,"Blank candidate surface");
                if(dpi==96){if(reference.empty())reference=pixels;else require(reference==pixels,"DPI roundtrip changed rendering");}
                if(theme==u"默认" || theme==u"通透" || theme==u"赛博朋克") {
                    auto name=std::wstring(vertical?L"vertical-":L"horizontal-")+std::wstring(reinterpret_cast<const wchar_t*>(theme.data()),theme.size())+L"-"+std::to_wstring(dpi)+L".png";
                    png(output/name,renderer.pixelWidth(dpi),renderer.pixelHeight(dpi),pixels);
                }
                ++cases;
            }
        }
        for(UINT dpi:{96u,120u,144u,192u}) {
            const int scale=static_cast<int>(dpi);
            RECT work{-1920,0,0,1080},caret{-100,1020,-99,1040};
            const int w=MulDiv(180,scale,96),h=MulDiv(100,scale,96);
            auto pos=placeCandidateWindow(caret,work,w,h);
            require(pos.x+w<=work.right-2 && pos.y+h==work.bottom,"Work-area bottom/right constraint failed");
            const auto smaller=placeCandidateWindow(caret,work,w,10);
            require(smaller.y==caret.bottom+5,"Shrink should return below the caret without an above latch");
            caret={-1800,20,-1799,40};pos=placeCandidateWindow(caret,work,w,h);
            require(pos.x==caret.left && pos.y==45,"Caret gap or negative-monitor position differs");
        }
        for(bool vertical:{false,true})for(double size:{3.,17.,31.5,200.}) {
            CandidateStyle style;style.vertical=vertical;style.fontSize=size;
            CandidateRenderer layout(style,files);CandidatePresentation content{u"",{u"一",u"二"}};
            layout.layout(content,2000);
            const float border=static_cast<float>(candidateTheme(style.theme).borderWidth);
            require(std::abs(layout.items()[0].left-((vertical?12:8)+border))<.01,"Mode left padding differs");
            require(std::abs(layout.items()[0].top-(8+border))<.01,"Mode top padding differs");
            require(layout.width()>=std::ceil(size*(vertical?3.76:2.88)+15)+2*border,"Mode minimum width differs");
            if(vertical)require(layout.items()[1].top-layout.items()[0].top==std::ceil(size*1.5),"Vertical row differs");
            else require(layout.items()[1].top==layout.items()[0].top,"Horizontal unexpectedly wrapped");
            CandidatePresentation code{u"ab",{},true};layout.layout(code,2000);
            require(std::abs(layout.height()-(std::ceil(size)+4+2*border))<.01,"Code-only padding/row differs");
            std::vector<std::uint32_t> outputPixels;layout.render(144,0,outputPixels);
            if(size==17 && vertical)png(output/L"code-only-144.png",layout.pixelWidth(144),layout.pixelHeight(144),outputPixels);
        }
        CandidateStyle fallback;fallback.font=u"Missing NativeTiger Test Family";
        CandidateRenderer system(fallback,files);require(!system.privateFamily(),"Missing family unexpectedly private");
        system.layout({u"abc",{u"中文 fallback"}},80);std::vector<std::uint32_t> pixels;system.render(144,0,pixels);
        std::cout<<"{\"status\":\"passed\",\"layout_modes\":3,\"placement_dpis\":4,\"mode_sizes\":4,\"render_cases\":"<<cases<<",\"private_font\":true,\"dpi_roundtrip\":true,\"premultiplied_alpha\":true,\"fallback\":true,\"dpi_host_contexts\":3}\n";
        return 0;
    }catch(HRESULT hr){std::cerr<<"HRESULT "<<std::hex<<static_cast<unsigned long>(hr)<<'\n';return 1;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
