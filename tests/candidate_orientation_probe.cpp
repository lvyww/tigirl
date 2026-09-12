#define NOMINMAX
#include "CandidateOrientation.h"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace tiger::tsf;
namespace {
unsigned checks=0,placements=0;
void require(bool ok,const char* message) { ++checks;if(!ok)throw std::runtime_error(message); }
CandidatePlacementEnvironment environment() {
    CandidatePlacementEnvironment e;e.monitor=1;e.owner=2;e.root=3;
    e.work={0,0,1920,1040};e.ownerBounds=e.rootBounds={100,100,1500,1040};return e;
}
POINT place(CandidateOrientation& memory,const RECT& caret,const CandidatePlacementEnvironment& e,int w,int h) {
    const auto p=memory.place(caret,e,w,h);++placements;
    require(p.has_value(),"Valid placement was rejected");
    require(p->x>=e.work.left && p->y>=e.work.top,"Top/left boundary escaped");
    if(h<=std::int64_t(e.work.bottom)-e.work.top)
        require(std::int64_t(p->y)+h<=e.work.bottom,"Bottom boundary escaped");
    if(w+2<=std::int64_t(e.work.right)-e.work.left)
        require(std::int64_t(p->x)+w<=e.work.right-2,"Right reserve escaped");
    return *p;
}
}
int main() {
    try {
        auto e=environment();RECT c{500,960,501,980};CandidateOrientation memory;
        require(place(memory,c,e,200,30).y==985 && !memory.above(),"Initial short list must prefer below");
        require(place(memory,c,e,200,120).y==835 && memory.above(),"Tall list did not flip above");
        // Hiding/destroying/recreating CandidateUI does not destroy this Context-owned value.
        require(place(memory,c,e,200,20).y==935 && memory.above(),"Above preference lost across short compositions");
        for(int dy:{1,-1,2,-2,3,-3,0}) {
            auto jitter=c;jitter.top+=dy;jitter.bottom+=dy;
            require(place(memory,jitter,e,200,20).y==jitter.top-25 && memory.above(),"Jitter caused a direction change");
        }
        auto moved=c;moved.top-=4;moved.bottom-=4;
        require(place(memory,moved,e,200,20).y==moved.bottom+5 && !memory.above(),"Upward threshold did not re-evaluate");
        place(memory,c,e,200,120);
        for(int dy:{-1,-2,-3,-4}) {
            moved=c;moved.top+=dy;moved.bottom+=dy;place(memory,moved,e,200,20);
        }
        require(!memory.above(),"Adjacent-sample drift swallowed a real upward move");
        place(memory,c,e,200,120);
        moved=c;moved.top+=20;moved.bottom+=20;place(memory,moved,e,200,20);
        require(memory.above(),"Downward motion lost above preference");
        moved.top-=4;moved.bottom-=4;place(memory,moved,e,200,20);
        require(!memory.above(),"Downward reference was not advanced");
        // Upward movement permits re-evaluation, not mandatory below placement.
        place(memory,c,e,200,120);moved=c;moved.top-=4;moved.bottom-=4;
        place(memory,moved,e,200,120);require(memory.above(),"Upward move forced a non-fitting below placement");
        // Visibility overrides memory when a shorter caret/upper edge removes room above.
        auto high=e;high.work.top=950;place(memory,c,high,200,20);
        require(!memory.above(),"An unusable above region defeated a fitting below region");
        // All environment changes must invalidate a formerly preferred side.
        for(int change=0;change<9;++change) {
            CandidateOrientation m;place(m,c,e,200,120);auto changed=e;
            switch(change) {
                case 0:++changed.epoch;break;case 1:++changed.monitor;break;
                case 2:++changed.owner;break;case 3:++changed.root;break;case 4:changed.dpi=144;break;
                case 5:--changed.work.bottom;break;case 6:++changed.ownerBounds.left;break;
                case 7:++changed.rootBounds.top;break;case 8:++changed.ownerBounds.right;break;
            }
            place(m,c,changed,200,20);require(!m.above(),"Changed placement environment retained old direction");
        }
        CandidateOrientation other;place(other,c,e,200,20);require(!other.above(),"Contexts shared direction state");
        place(memory,c,e,200,120);memory.reset();place(memory,c,e,200,20);require(!memory.above(),"Explicit reset failed");
        place(memory,c,e,200,120);
        require(!memory.place({},e,200,20),"Zero caret was accepted");
        require(!memory.place({10,10,9,30},e,200,20),"Inverted caret accepted");
        require(!memory.place(c,e,0,20),"Empty window accepted");
        auto empty=e;empty.work={};require(!memory.place(c,empty,200,20),"Empty work area accepted");
        place(memory,c,e,200,20);require(memory.above(),"Invalid layout polluted remembered direction");
        place(memory,c,e,200,120);
        auto tallerCaret=c;tallerCaret.top=e.work.top;
        place(memory,tallerCaret,e,200,20);
        require(!memory.above(),"Above preference ignored loss of room above the caret");
        const RECT areas[]={{0,0,1920,1040},{-1920,0,0,1040},{0,-1080,1920,-40},
            {-2560,-1440,0,-40},{1920,120,4480,1520},{0,48,1920,1080},{48,0,1920,1080},{0,0,1872,1080}};
        for(const auto& work:areas)for(unsigned dpi:{96u,120u,144u,192u}) {
            auto env=e;env.work=work;env.dpi=dpi;
            const int line=static_cast<int>(24*dpi/96),shortHeight=static_cast<int>(16*dpi/96);
            const int large=static_cast<int>(150*dpi/96),width=static_cast<int>(240*dpi/96);
            const RECT caret{work.right-10,work.bottom-80-line,work.right-9,work.bottom-80};
            CandidateOrientation m;unsigned flips=0;bool old=false;
            for(int session=0;session<100;++session)for(int height:{shortHeight,large,shortHeight,large+10}) {
                place(m,caret,env,width,height);if(m.above()!=old)++flips;old=m.above();
            }
            require(flips==1,"Repeated word sessions did not reduce direction changes to one");
            const int threshold=static_cast<int>((dpi*3+48)/96);
            auto jitter=caret;jitter.top-=threshold;jitter.bottom-=threshold;
            place(m,jitter,env,width,shortHeight);require(m.above(),"DPI-scaled boundary was not inclusive");
            --jitter.top;--jitter.bottom;place(m,jitter,env,width,shortHeight);
            require(!m.above(),"DPI-scaled upward move not detected");
            // Small caret heights cap the jitter band, independently of candidate font size.
            auto tiny=caret;tiny.top=tiny.bottom-8;CandidateOrientation cap;
            place(cap,tiny,env,width,large);tiny.top-=3;tiny.bottom-=3;
            place(cap,tiny,env,width,shortHeight);require(!cap.above(),"Tolerance exceeded a quarter of caret height");
            for(int h=1;h<=1600;h+=7)place(m,caret,env,width,h);
            place(m,caret,env,work.right-work.left+100,50);
        }
        // Saturating work-area clamp must not overflow on extreme screen coordinates.
        auto extreme=e;extreme.work={std::numeric_limits<LONG>::min(),std::numeric_limits<LONG>::min(),
                                    std::numeric_limits<LONG>::max(),std::numeric_limits<LONG>::max()};
        CandidateOrientation m;
        place(m,{0,std::numeric_limits<LONG>::max()-20,1,std::numeric_limits<LONG>::max()},extreme,200,50);
        place(m,{0,std::numeric_limits<LONG>::min(),1,std::numeric_limits<LONG>::min()+20},extreme,200,50);
        std::cout<<"{\"probe\":\"candidate_orientation\",\"status\":\"passed\",\"placements\":"<<placements
                 <<",\"checks\":"<<checks<<",\"physical_input_tested\":false}\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
