#define NOMINMAX
#include "CandidatePlacement.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace tiger::tsf;
namespace {
unsigned checks=0;
void require(bool value,const char* message) { ++checks;if(!value)throw std::runtime_error(message); }
void atAbove(POINT p,const RECT& caret,int height) {
    require(p.y+height==caret.top-5,"Above-caret gap differs");
}
void atBelow(POINT p,const RECT& caret) { require(p.y==caret.bottom+5,"Expected below-caret placement"); }
void inWork(POINT p,const RECT& work,int width,int height) {
    require(p.x>=work.left && p.y>=work.top,"Outside work-area top/left");
    if(width+2<=work.right-work.left)require(p.x+width<=work.right-2,"Lost right reserve");
    else require(p.x==work.left,"Oversized width not clamped");
    if(height<=work.bottom-work.top)require(p.y+height<=work.bottom,"Outside work-area bottom");
    else require(p.y==work.top,"Oversized height not clamped");
}
}
int main() {
    try {
        const RECT work{0,0,1920,1040};
        RECT caret{500,960,501,980};
        CandidatePlacementEnvironment env;env.monitor=1;env.owner=2;env.hasOwnerRect=true;env.ownerRect={0,0,1000,1000};
        CandidatePlacement memory;
        atBelow(memory.place(caret,work,200,30,env),caret);
        atAbove(memory.place(caret,work,200,140,env),caret,140);
        // Recreated popup/new composition uses the same Context-owned memory.
        require(memory.place(caret,work,200,20,env).y+20==caret.top-5,
                "Cross-composition above placement was not retained");
        require(memory.above(),"Above direction not recorded");
        const auto reference=memory.referenceY();
        for(int offset:{-3,0,2,-2,3,0}) {
            RECT jitter=caret;jitter.top+=offset;jitter.bottom+=offset;
            atAbove(memory.place(jitter,work,200,20,env),jitter,20);
            require(memory.referenceY()==reference,"Jitter moved the stable reference");
        }
        RECT down=caret;down.top+=20;down.bottom+=20;
        atAbove(memory.place(down,work,200,20,env),down,20);
        require(memory.referenceY()==down.bottom,"Downward reference did not advance");
        RECT up=down;up.top-=2;up.bottom-=2;
        atAbove(memory.place(up,work,200,20,env),up,20);
        up.top-=2;up.bottom-=2;
        atBelow(memory.place(up,work,200,20,env),up);
        require(!memory.above(),"Slow upward movement stayed latched");
        memory.reset();atBelow(memory.place(caret,work,200,20,env),caret);
        // Repeated commits, cancel/retype, code-only and delayed expansion sizes.
        for(int session=0;session<100;++session) {
            atAbove(memory.place(caret,work,200,140,env),caret,140);
            for(int h:{20,45,140,1,200,30})atAbove(memory.place(caret,work,200,h,env),caret,h);
        }
        // Focus/mode epoch, owner, owner bounds, monitor, DPI, work area: each
        // invalidates the old direction even if the new caret has the same Y.
        for(int change=0;change<8;++change) {
            CandidatePlacement p;p.place(caret,work,200,140,env);
            auto next=env;RECT changedWork=work;
            switch(change) {
                case 0:++next.epoch;break;
                case 1:++next.owner;break;
                case 2:++next.monitor;break;
                case 3:next.dpi=144;break;
                case 4:++next.ownerRect.left;break;
                case 5:++next.ownerRect.bottom;break;
                case 6:next.hasOwnerRect=false;break;
                case 7:++changedWork.bottom;break;
            }
            atBelow(p.place(caret,changedWork,200,20,next),caret);
        }
        // Invalid geometry and failed/unpublished previews do not change memory.
        const auto revision=memory.revision();
        for(const RECT invalid: {RECT{},RECT{0,20,1,10},RECT{5,10,4,30}}) {
            memory.place(invalid,work,200,20,env);
            require(memory.revision()==revision && memory.above(),"Invalid caret corrupted memory");
        }
        auto preview=memory;preview.reset();preview.place(caret,work,200,20,env);
        require(memory.revision()==revision && memory.above(),"Unpublished preview mutated live memory");
        memory.place(caret,work,0,20,env);memory.place(caret,RECT{},200,20,env);
        require(memory.revision()==revision,"Invalid layout advanced memory");
        // An inherited above preference never wins over a fully fitting below.
        RECT high{500,100,501,120};CandidatePlacement p;
        RECT shortWork{0,0,1920,150};
        atAbove(p.place(high,shortWork,100,50,env),high,50);
        high.top=1; // Same bottom but a tall input line leaves no room above.
        atBelow(p.place(high,shortWork,100,20,env),high);
        // New upward row still flips normally when neither the old nor new
        // below space can hold the candidates (re-evaluate != force below).
        p.reset();p.place(caret,work,200,140,env);
        RECT upRow=caret;upRow.top-=20;upRow.bottom-=20;
        atAbove(p.place(upRow,work,200,200,env),upRow,200);
        const RECT areas[]={
            {0,0,1920,1040},{-1920,0,0,1040},{0,-1080,1920,-40},
            {-2560,-1440,0,-40},{1920,120,4480,1520},
            {0,48,1920,1080},{48,0,1920,1080},{0,0,1872,1080}
        };
        for(const auto& area:areas)for(unsigned dpi:{96u,120u,144u,192u}) {
            auto e=env;e.dpi=dpi;
            const int line=static_cast<int>(20*dpi/96),w=static_cast<int>(200*dpi/96);
            RECT c{area.right-30,area.bottom-60-line,area.right-29,area.bottom-60};
            const auto tolerance=CandidatePlacement::tolerance(c,dpi);
            require(tolerance<line && tolerance<=line/4,"Tolerance exceeds line-height bound");
            CandidatePlacement continuous;
            for(int h=1;h<=700;++h) {
                const auto pos=continuous.place(c,area,w,h,e);inWork(pos,area,w,h);
                if(h<=55)atBelow(pos,c);else atAbove(pos,c,h);
            }
            for(int h=699;h>=1;--h)atAbove(continuous.place(c,area,w,h,e),c,h);
            for(int h=1;h<=700;++h) {
                CandidatePlacement fresh;
                const auto pos=fresh.place(c,area,w,h,e);inWork(pos,area,w,h);
                if(h<=55)atBelow(pos,c);else atAbove(pos,c,h);
            }
            // A full line upward must leave the deadband at every DPI.
            c.top-=line;c.bottom-=line;atBelow(continuous.place(c,area,w,20,e),c);
            for(int height:{1,2,3,4,8,12,20,40}) {
                RECT small{20,0,21,height};
                require(CandidatePlacement::tolerance(small,dpi)<=height/4,"Tiny caret tolerance too large");
            }
            CandidatePlacement oversized;
            inWork(oversized.place(c,area,w,area.bottom-area.top+200,e),area,w,area.bottom-area.top+200);
            require(!oversized.above(),"Oversized overlap was remembered as above");
            inWork(oversized.place(c,area,area.right-area.left+100,20,e),area,area.right-area.left+100,20);
        }
        // Arithmetic is widened before adding offsets/subtracting dimensions.
        const LONG lo=std::numeric_limits<LONG>::min(),hi=std::numeric_limits<LONG>::max();
        CandidatePlacement extremes;
        const auto low=extremes.place({lo,lo,lo+1,lo+20},{lo,lo,lo+100,lo+100},200,200,env);
        require(low.x==lo && low.y==lo,"Negative extreme clamping failed");
        const auto upper=extremes.place({hi-1,hi-20,hi,hi},{hi-100,hi-100,hi,hi},20,20,env);
        require(upper.y==hi-45 && upper.x==hi-22,"Positive extreme placement failed");
        std::cout<<"{\"probe\":\"candidate_placement\",\"status\":\"passed\",\"checks\":"<<checks
                 <<",\"work_areas\":8,\"dpis\":4,\"cross_composition\":true,\"physical_input_tested\":false}\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
