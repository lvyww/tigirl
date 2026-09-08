#define NOMINMAX
#include <windows.h>
#include "../native/tsf/PrivateFonts.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
static void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==2,"private_font_probe <font-directory>");
        auto fonts=tiger::tsf::PrivateFonts::Open(argv[1]);
        require(fonts->files()==1 && fonts->faces()>=1,"Bundled font did not load");
        auto other=tiger::tsf::PrivateFonts::Open(argv[1]);
        require(fonts==other,"Font resources not reused within process");
        std::weak_ptr<tiger::tsf::PrivateFonts> weak=fonts;
        fonts.reset(); require(!weak.expired(),"Font lease released prematurely");
        HDC dc=CreateCompatibleDC(nullptr); require(dc!=nullptr,"Cannot create DC");
        HFONT font=CreateFontW(-17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"霞鹜文楷 GB 屏幕阅读版");
        require(font!=nullptr,"Cannot select Chinese family name");
        auto old=SelectObject(dc,font);
        const DWORD size=GetFontData(dc,0,0,nullptr,0); require(size!=GDI_ERROR,"Cannot retrieve selected font data");
        std::vector<char> actual(size);
        require(GetFontData(dc,0,0,actual.data(),size)==size,"Cannot read selected font");
        const auto path=std::filesystem::path(argv[1])/L"LXGWWenKaiGBScreen.ttf";
        std::ifstream input(path,std::ios::binary);
        std::vector<char> expected((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
        require(actual==expected,"GDI selected fallback or different font bytes");
        WORD glyphs[2]; require(GetGlyphIndicesW(dc,L"交疒",2,glyphs,GGI_MARK_NONEXISTING_GLYPHS)!=GDI_ERROR && glyphs[0]!=0xffff && glyphs[1]!=0xffff,"Candidate glyphs missing");
        SelectObject(dc,old); DeleteObject(font); DeleteDC(dc);
        other.reset(); require(weak.expired(),"Font resource lease leaked");
        require(!RemoveFontResourceExW(path.c_str(),FR_PRIVATE|FR_NOT_ENUM,nullptr),"Font registration remained after last lease");
        auto reloaded=tiger::tsf::PrivateFonts::Open(argv[1]); require(reloaded->faces()>=1,"Reload after final release failed");
        std::cout<<"{\"font_bytes\":"<<size<<",\"exact_gdi_font_match\":true,\"shared_lease\":true,\"release_and_reload\":true,\"status\":\"passed\"}\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
