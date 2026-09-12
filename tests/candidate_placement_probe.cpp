#define NOMINMAX
#include "CandidatePlacement.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace tiger::tsf;
namespace {
unsigned checks=0;
void require(bool value,const char* message) { ++checks;if(!value)throw std::runtime_error(message); }
POINT place(CandidatePlacement& memory,const RECT& caret,const CandidatePlacementEnvironment& env,int w,int h) {
    POINT p{};require(memory.place(caret,env,w,h,p),"Valid geometry rejected");
    require(p.x>=env.work.left && p.y>=env.work.top,"Top/left clamp lost");
    if(h<=static_cast<long long>(env.work.bottom)-env.work.top)
        require(static_cast<long long>(p.y)+h<=env.work.bottom,"Bottom clamp lost");
    return p;
}
}
int main() {
    try {
        CandidatePlacementEnvironment env;env.work={0,0,1920,1040};
        const RECT caret{500,980,501,1000};
        CandidatePlacement memory;
        auto p=place(memory,caret,env,240,120);
        require(memory.above() && p.y==855,"Overflow did not flip above caret");
        p=place(memory,caret,env,240,20);
        require(memory.above() && p.y==955,"Cross-composition above preference was lost");
        const auto anchor=memory.referenceY();
        for(int dy:{-1,1,-3,3,0}) {
            const RECT moved{500,980+dy,501,1000+dy};
            p=place(memory,moved,env,240,20);
            require(memory.above() && memory.referenceY()==anchor,"Jitter moved reference or lost direction");
            require(p.y==moved.top-5-20,"Direction hysteresis froze actual caret coordinates");
        }
        for(int dy:{-2,-4}) {
            const RECT moved{500,980+dy,501,1000+dy};
            place(memory,moved,env,240,20);
        }
        require(!memory.above() && memory.referenceY()==996,"Slow upward movement was swallowed");
        memory.reset();place(memory,caret,env,240,120);
        const RECT down{500,985,501,1005};
        place(memory,down,env,240,20);
        require(memory.above() && memory.referenceY()==1005,"Downward motion lost above memory");
        const RECT up{500,980,501,1000};
        place(memory,up,env,240,20);require(!memory.above(),"Upward motion did not re-evaluate");
        memory.reset();place(memory,caret,env,240,120);
        place(memory,{500,970,501,990},env,240,120);
        require(memory.above(),"Upward motion forced below even though below does not fit");
        memory.reset();place(memory,caret,env,240,120);
        require(memory.above(),"Tall fixture did not flip");
        // Move downward enough to inherit, then make the area above insufficient
        // using a taller caret, not an upward Y change. Below still fits.
        place(memory,{500,5,501,1001},env,240,20);
        require(!memory.above(),"Remembered direction overrode above-space failure");
        const auto savedRevision=memory.revision();POINT sentinel{123,456};
        require(!memory.place({},env,240,20,sentinel),"Zero caret accepted");
        require(memory.revision()==savedRevision && sentinel.x==123 && sentinel.y==456,"Invalid geometry changed state");
        for(auto size:{0,-1})require(!memory.place(caret,env,240,size,sentinel),"Invalid height accepted");
        auto bad=env;bad.work.bottom=bad.work.top;
        require(!memory.place(caret,bad,240,20,sentinel),"Empty work area accepted");
        // Test each environment component independently while both sides fit.
        for(int component=0;component<8;++component) {
            CandidatePlacement m;place(m,caret,env,240,120);auto changed=env;
            switch(component) {
            case 0:changed.monitor=reinterpret_cast<HMONITOR>(1);break;
            case 1:changed.owner=reinterpret_cast<HWND>(1);break;
            case 2:changed.dpi=144;break;
            case 3:changed.work.bottom+=10;break;
            case 4:changed.work.left+=10;break;
            case 5:changed.hasOwnerRect=true;break;
            case 6:env.hasOwnerRect=true;env.ownerRect={0,0,100,100};m.reset();place(m,caret,env,240,120);
                changed=env;changed.ownerRect.top+=1;break;
            case 7:changed.hasOwnerRect=false;break;
            }
            place(m,caret,changed,240,20);
            require(!m.above(),"Environment change inherited stale direction");
        }
        env.hasOwnerRect=false;
        const RECT areas[]={{0,0,1920,1040},{-1920,0,0,1040},{0,-1080,1920,-40},
            {-2560,-1440,0,-40},{1920,120,4480,1520},{0,48,1920,1080},{48,0,1920,1080},{0,0,1872,1080}};
        for(const auto& area:areas)for(UINT dpi:{96u,120u,144u,192u}) {
            env.work=area;env.dpi=dpi;CandidatePlacement m;
            const int line=(20*dpi+48)/96;
            RECT c{area.right-30,area.bottom-30-line,area.right-29,area.bottom-30};
            place(m,c,env,240,150);require(m.above(),"Initial flip failed");
            const auto epsilon=CandidatePlacement::tolerance(c,dpi);
            require(epsilon>=0 && epsilon<=line/4,"Threshold can hide a whole line");
            for(int session=0;session<100;++session) {
                for(int h:{10,80,20,160,15}) {
                    const auto q=place(m,c,env,240,h);
                    require(m.above() && q.y+h==c.top-5,"Repeated words changed direction");
                    require(q.x+240<=area.right-2,"Right reserve lost");
                }
            }
            const auto reference=m.referenceY();
            for(int offset=1;offset<=epsilon+1;++offset) {
                auto moved=c;moved.top-=offset;moved.bottom-=offset;
                place(m,moved,env,240,10);
                require(m.above()==(offset<=epsilon),"Upward threshold is not cumulative");
            }
            require(m.referenceY()==reference-epsilon-1,"Reference did not accept real upward motion");
            m.reset();place(m,c,env,240,10);require(!m.above(),"Reset did not restore below preference");
            // New contexts have independent memory, not a global lastY.
            CandidatePlacement other;place(other,c,env,240,10);require(!other.above(),"Context memory leaked");
            // Exact fit belongs below; one additional pixel requires flipping.
            m.reset();const int available=area.bottom-c.bottom-5;
            place(m,c,env,240,available);require(!m.above(),"Exact below fit flipped");
            place(m,c,env,240,available+1);require(m.above(),"One pixel overflow did not flip");
            place(m,c,env,area.right-area.left+200,area.bottom-area.top+200);
        }
        // Arithmetic must not overflow near LONG's extrema, including caret gap.
        const LONG lo=(std::numeric_limits<LONG>::min)(),hi=(std::numeric_limits<LONG>::max)();
        for(RECT area:{RECT{lo,lo,lo+1920,lo+1080},RECT{hi-1920,hi-1080,hi,hi}}) {
            env.work=area;CandidatePlacement m;
            place(m,{area.left,area.bottom-20,area.left+1,area.bottom},env,240,120);
        }
        require(CandidatePlacement::tolerance({0,0,1,3},192)==0,"Tiny caret threshold too large");
        std::cout<<"{\"status\":\"passed\",\"probe\":\"candidate_direction_memory\",\"checks\":"<<checks
                 <<",\"work_areas\":8,\"dpis\":4,\"physical_input_tested\":false}\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
