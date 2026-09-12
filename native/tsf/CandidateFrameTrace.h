#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace tiger::tsf {
// Opt-in diagnostics. Capture at most three CandidateUI lifetimes per process,
// 32 successful frames and 8 MiB of pixels each. No filesystem work while visible.
class CandidateFrameTrace {
public:
    struct Geometry {
        std::uint64_t sequence=0,tick=0,owner=0,caretWindow=0;
        int callerAwareness=-1,queryAwareness=-1,ownerAwareness=-1,caretAwareness=-1;
        unsigned ownerDpi=0,caretDpi=0;
        long textResult=E_FAIL;
        bool clipped=false,guiValid=false;
        const char* reason="unknown";
        RECT reported{},converted{},ownerLogical{},ownerPhysical{},guiClient{},guiScreen{},guiPhysical{};
    };
    struct Record {
        std::uint64_t tick=0,model=0,visual=0,content=0;
        unsigned candidates=0,items=0,rawLength=0,dpi=0;
        int x=0,y=0,width=0,height=0,targetWidth=0,targetHeight=0;
        bool appearing=false,firstCandidates=false,animation=false,annotations=false,codeOnly=false;
        const char* source="layout";
        Geometry geometry;
    };
    CandidateFrameTrace() noexcept {
        try {
            const auto length=GetEnvironmentVariableW(L"TIGIRL_FRAME_TRACE_DIR",nullptr,0);
            if(!length || length>32768)return;
            std::wstring path(length,L'\0');
            const auto n=GetEnvironmentVariableW(L"TIGIRL_FRAME_TRACE_DIR",path.data(),length);
            if(!n || n>=length)return;
            path.resize(n);
            std::filesystem::path root(path);if(!root.is_absolute())return;
            const auto slot=slots_.fetch_add(1);if(slot>=3)return;
            LARGE_INTEGER clock{};QueryPerformanceCounter(&clock);
            directory_=root/(std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(clock.QuadPart)+L"-"+std::to_wstring(slot));
            frames_.reserve(32);geometries_.reserve(128);enabled_=true;
        }catch(...){}
    }
    bool enabled() const noexcept{return enabled_ && frames_.size()<32;}
    void geometry(const Geometry& value) noexcept {
        if(!enabled_ || geometries_.size()>=128)return;
        try{geometries_.push_back(value);}catch(...){}
    }
    void capture(const Record& record,const std::vector<std::uint32_t>& pixels) noexcept {
        if(!enabled())return;
        try {
            Frame frame;frame.record=record;
            if(pixels.size()<= (8*1024*1024-bytes_)/sizeof(std::uint32_t)){
                frame.pixels=pixels;bytes_+=pixels.size()*sizeof(std::uint32_t);
            }
            frames_.push_back(std::move(frame));
        }catch(...){enabled_=false;}
    }
    // Call only after hide/detach. Once flushed, this trace is closed.
    void flush() noexcept {
        if(frames_.empty() && geometries_.empty())return;
        enabled_=false;
        try {
            std::filesystem::create_directories(directory_);
            std::ofstream geometryLog(directory_/L"geometry.tsv");
            geometryLog<<"sequence\ttick\treason\towner\tcaret_window\tcaller_awareness\tquery_awareness\towner_awareness\tcaret_awareness\towner_dpi\tcaret_dpi\ttext_hresult\tclipped\tgui_valid\treported\tconverted\towner_logical\towner_physical\tgui_client\tgui_screen\tgui_physical\n";
            auto rect=[&](const RECT& r){geometryLog<<'\t'<<r.left<<','<<r.top<<','<<r.right<<','<<r.bottom;};
            for(const auto& g:geometries_){
                geometryLog<<g.sequence<<'\t'<<g.tick<<'\t'<<g.reason<<'\t'<<g.owner<<'\t'<<g.caretWindow<<'\t'<<g.callerAwareness<<'\t'<<g.queryAwareness<<'\t'<<g.ownerAwareness<<'\t'<<g.caretAwareness<<'\t'<<g.ownerDpi<<'\t'<<g.caretDpi<<'\t'<<g.textResult<<'\t'<<g.clipped<<'\t'<<g.guiValid;
                rect(g.reported);rect(g.converted);rect(g.ownerLogical);rect(g.ownerPhysical);rect(g.guiClient);rect(g.guiScreen);rect(g.guiPhysical);geometryLog<<'\n';
            }
            geometryLog.flush();if(!geometryLog)throw std::runtime_error("geometry trace write failed");
            std::ofstream log(directory_/L"frames.tsv");
            log<<"frame\ttick\tmodel\tvisual\tcontent_hash\tcandidates\titems\traw_length\tdpi\tx\ty\twidth\theight\ttarget_width\ttarget_height\tappearing\tfirst_candidates\tanimation\tannotations\tcode_only\tsource\tpixels_saved\tgeometry_sequence\n";
            for(std::size_t i=0;i<frames_.size();++i){
                const auto& frame=frames_[i];const auto& r=frame.record;
                if(!frame.pixels.empty())saveBitmap(directory_/(std::to_wstring(i)+L".bmp"),r,frame.pixels);
                log<<i<<'\t'<<r.tick<<'\t'<<r.model<<'\t'<<r.visual<<'\t'<<r.content<<'\t'<<r.candidates<<'\t'<<r.items<<'\t'<<r.rawLength<<'\t'<<r.dpi<<'\t'<<r.x<<'\t'<<r.y<<'\t'<<r.width<<'\t'<<r.height<<'\t'<<r.targetWidth<<'\t'<<r.targetHeight<<'\t'<<r.appearing<<'\t'<<r.firstCandidates<<'\t'<<r.animation<<'\t'<<r.annotations<<'\t'<<r.codeOnly<<'\t'<<r.source<<'\t'<<!frame.pixels.empty()<<'\t'<<r.geometry.sequence<<'\n';
            }
            log.flush();if(!log)throw std::runtime_error("trace write failed");
        }catch(...){OutputDebugStringW(L"Tigirl: candidate frame trace write failed\n");}
        frames_.clear();geometries_.clear();bytes_=0;
    }
private:
    struct Frame{Record record;std::vector<std::uint32_t> pixels;};
    static void saveBitmap(const std::filesystem::path& path,const Record& r,const std::vector<std::uint32_t>& pixels){
        // Published pixels are premultiplied BGRA; composite on white for viewing.
        BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};
        file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+static_cast<DWORD>(pixels.size()*4);
        info.biSize=sizeof(info);info.biWidth=r.width;info.biHeight=-r.height;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
        std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info),sizeof(info));
        for(auto pixel:pixels){const auto white=255-(pixel>>24);std::uint32_t rgb=0xff000000;
            for(unsigned shift:{0u,8u,16u})rgb|=((pixel>>shift&255)+white)<<shift;
            out.write(reinterpret_cast<const char*>(&rgb),4);
        }
        out.flush();if(!out)throw std::runtime_error("trace bitmap write failed");
    }
    inline static std::atomic<unsigned> slots_{0};
    bool enabled_=false;std::size_t bytes_=0;
    std::filesystem::path directory_;std::vector<Frame> frames_;
    std::vector<Geometry> geometries_;
};
}
