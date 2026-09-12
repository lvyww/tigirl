#define NOMINMAX
#include "CandidatePlacement.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using tiger::tsf::placeCandidateWindow;
namespace {
unsigned cases=0;
void require(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
}
POINT check(const RECT& caret,const RECT& work,int width,int height) {
    const auto p=placeCandidateWindow(caret,work,width,height);
    ++cases;
    // Independent constraints: remain below the caret when possible, otherwise
    // touch the work-area bottom, never align the bottom to the caret's top.
    const auto available=work.bottom-work.top;
    if(height>available)require(p.y==work.top,"Oversized height lost the work-area top");
    else if(caret.bottom+5<work.top)require(p.y==work.top,"Top work-area clamp failed");
    else if(caret.bottom+5+height<=work.bottom)
        require(p.y==caret.bottom+5,"Below-caret gap changed or above placement was retained");
    else require(p.y+height==work.bottom,"Overflow must slide to the work-area bottom, not flip above the caret");
    require(p.x>=work.left && p.y>=work.top,"Position escaped work-area top/left");
    if(width+2<=work.right-work.left)require(p.x+width<=work.right-2,"Right reserve lost");
    else require(p.x==work.left,"Oversized width lost the work-area left");
    if(height<=available)require(p.y+height<=work.bottom,"Candidate extends below work area");
    return p;
}
}
int main() {
    try {
        const RECT work{0,0,1920,1040},caret{500,980,501,1000};
        auto p=check(caret,work,240,120);
        require(p.y==920 && p.y+120==1040,"Bottom overflow did not use taskbar edge");
        p=check(caret,work,240,20);
        require(p.y==1005,"Shrinking content must return below the caret, without a session latch");
        require(check(caret,work,240,35).y==1005,"Exact-fit height moved unnecessarily");
        require(check(caret,work,240,36).y==1004,"One-pixel overflow caused a flip");
        // A changed caret top (e.g. a taller text line) must not move the popup.
        const RECT tallCaret{500,900,501,1000};
        require(check(tallCaret,work,240,120).y==920,"Placement still depends on caret top");
        const RECT areas[]={
            {0,0,1920,1040}, {-1920,0,0,1040}, {0,-1080,1920,-40},
            {-2560,-1440,0,-40}, {1920,120,4480,1520},
            {0,48,1920,1080}, {48,0,1920,1080}, {0,0,1872,1080}
        };
        for(const auto& area:areas)for(int dpi:{96,120,144,192}) {
            const int width=(240*dpi+95)/96;
            const RECT bottom{area.right-30,area.bottom-80,area.right-29,area.bottom-60};
            auto previous=check(bottom,area,width,1);
            // Sweep across the overflow threshold in both directions. Changing
            // height by one pixel may move the top by zero or one pixel only.
            for(int height=2;height<=600;++height) {
                p=check(bottom,area,width,height);
                require(previous.y-p.y>=0 && previous.y-p.y<=1,"Growing candidates jumped across the caret");
                previous=p;
            }
            for(int height=599;height>=1;--height) {
                p=check(bottom,area,width,height);
                require(p.y-previous.y>=0 && p.y-previous.y<=1,"Shrinking candidates jumped across the caret");
                previous=p;
            }
            // Repeated word commits must produce the same geometry as one
            // long sentence session, including code-only and delayed expansion.
            for(int session=0;session<50;++session)for(int dipHeight:{20,140,35,200,10,60}) {
                check(bottom,area,width,(dipHeight*dpi+95)/96);
            }
            const RECT middle{area.left+200,area.top+200,area.left+201,area.top+220};
            p=check(middle,area,width,120);
            require(p.x==middle.left && p.y==middle.bottom+5,"Normal placement or monitor transition changed");
            const RECT left{area.left-10,area.top,area.left-9,area.top+20};
            require(check(left,area,width,50).x==area.left,"Left work-area clamp failed");
            const RECT above{area.left,area.top-100,area.left+1,area.top-80};
            check(above,area,width,50);
            const RECT outside{area.right+10,area.bottom+30,area.right+11,area.bottom+50};
            check(outside,area,width,50);
            check(bottom,area,width,area.bottom-area.top);
            check(bottom,area,width,area.bottom-area.top+200);
            check(bottom,area,area.right-area.left+100,50);
            // Changing taskbar/work-area size is applied immediately.
            RECT reduced=area;reduced.bottom-=48;
            check(bottom,reduced,width,160);
            check(bottom,area,width,160);
        }
        // Tiny work areas must not invert std::clamp's limits.
        check({100,200,101,201},{100,200,102,202},300,200);
        check({100,200,101,201},{100,200,101,201},1,1);
        std::cout<<"{\"probe\":\"candidate_placement\",\"status\":\"passed\",\"geometry_cases\":"<<cases
                 <<",\"work_areas\":8,\"dpis\":4,\"physical_input_tested\":false}\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<'\n';return 1;
    }
}
