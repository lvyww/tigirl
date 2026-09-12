#define NOMINMAX
#include "../native/tsf/CandidateDpi.h"
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace tiger::tsf;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool same(const RECT& a,const RECT& b){return abs(a.left-b.left)<=1&&abs(a.top-b.top)<=1&&abs(a.right-b.right)<=1&&abs(a.bottom-b.bottom)<=1;}
int main(){try{
    CandidateDpiScope physical;
    std::vector<RECT> screens;
    EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR,HDC,LPRECT r,LPARAM p)->BOOL{reinterpret_cast<std::vector<RECT>*>(p)->push_back(*r);return TRUE;},reinterpret_cast<LPARAM>(&screens));
    unsigned checks=0,oldFailures=0,extentChecks=0,childRejections=0;
    for(const auto& screen:screens)for(auto awareness:{DPI_AWARENESS_CONTEXT_UNAWARE,DPI_AWARENESS_CONTEXT_SYSTEM_AWARE,DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2}){
        auto old=SetThreadDpiAwarenessContext(awareness);
        HWND owner=CreateWindowExW(0,L"STATIC",L"Hidden caret DPI regression",WS_POPUP,0,0,200,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        SetThreadDpiAwarenessContext(old);require(owner!=nullptr,"Create host failed");
        SetWindowPos(owner,nullptr,screen.left+200,screen.top+200,400,200,SWP_NOACTIVATE|SWP_NOZORDER);
        RECT physicalRect{},cachedLogical{};GetWindowRect(owner,&physicalRect);
        {CandidateHostDpiScope host(owner);GetWindowRect(owner,&cachedLogical);}
        for(auto caller:{DPI_AWARENESS_CONTEXT_UNAWARE,DPI_AWARENESS_CONTEXT_SYSTEM_AWARE,DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2}){
            auto previous=SetThreadDpiAwarenessContext(caller);
            // An application may return a rectangle cached in its own window
            // context. The old helper alone depends on the callback context.
            if(!same(candidatePhysicalCaret(cachedLogical,owner),physicalRect))++oldFailures;
            {
                CandidateHostDpiScope host(owner);
                require(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),GetWindowDpiAwarenessContext(owner)),"Host context not adopted");
                RECT fresh{};GetWindowRect(owner,&fresh);
                require(same(candidatePhysicalCaret(fresh,owner),physicalRect),"Fresh host geometry shifted between callers");
                require(same(candidatePhysicalCaret(cachedLogical,owner),physicalRect),"Cached host geometry shifted between callers");
                {CandidateDpiScope paint;require(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2),"Paint context not PMv2");}
                require(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),GetWindowDpiAwarenessContext(owner)),"Nested paint did not restore host context");
                ++checks;
            }
            require(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),caller),"Caller context not restored");
            SetThreadDpiAwarenessContext(previous);
        }
        {
            CandidateHostDpiScope host(owner);
            HWND edit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD,20,20,100,23,owner,nullptr,GetModuleHandleW(nullptr),nullptr);
            require(edit!=nullptr,"Create child edit failed");
            RECT bounds{};require(GetWindowRect(edit,&bounds)!=FALSE,"Child bounds unavailable");
            for(int edge=0;edge<3;++edge){
                RECT extent{bounds.left+4,bounds.top,bounds.left+4,bounds.bottom};
                if(edge==1)extent.bottom+=2;
                if(edge==2){extent.right=bounds.right+2;extent.bottom+=2;}
                POINT first{extent.left,extent.top},last{extent.right,extent.bottom};
                require(LogicalToPhysicalPointForPerMonitorDPI(owner,&first)&&LogicalToPhysicalPointForPerMonitorDPI(owner,&last),"Root reference conversion failed");
                const RECT expected{first.x,first.y,last.x,last.y};
                POINT childLast{extent.right,extent.bottom};
                if(!LogicalToPhysicalPointForPerMonitorDPI(edit,&childLast))++childRejections;
                require(same(candidatePhysicalCaret(extent,edit),expected),"Child extent fallback shifted or converted a corner twice");
                ++extentChecks;
            }
            if(GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext())!=DPI_AWARENESS_PER_MONITOR_AWARE){
                RECT rootBounds{};GetWindowRect(owner,&rootBounds);
                RECT outside{rootBounds.right+10000,rootBounds.bottom+10000,rootBounds.right+10001,rootBounds.bottom+10001};
                require(same(candidatePhysicalCaret(outside,edit),RECT{}),"Failed conversion returned logical coordinates as physical");
                ++extentChecks;
            }
            DestroyWindow(edit);
        }
        DestroyWindow(owner);
    }
    std::cout<<"{\"status\":\"passed\",\"monitors\":"<<screens.size()<<",\"caller_host_cases\":"<<checks<<",\"old_conversion_mismatches\":"<<oldFailures<<",\"extent_cases\":"<<extentChecks<<",\"child_conversion_rejections\":"<<childRejections<<",\"hidden_windows_only\":true}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
