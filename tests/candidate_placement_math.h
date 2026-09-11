#pragma once
#include "../native/tsf/CandidatePlacement.h"

namespace tiger::tsf {
// Geometry-only checks. All inputs/expectations are physical pixels; work areas
// model bottom/top/side taskbars and monitors with negative virtual coordinates.
template<class Check> void checkCandidatePlacement(Check check) {
    const RECT work{0,0,1920,1040},caret{400,900,401,920};
    auto expect=[&](RECT c,RECT w,int width,int height,LONG x,LONG y) {
        const auto p=placeCandidateWindow(c,w,width,height);
        check(p.x==x && p.y==y,"Candidate work-area placement mismatch");
    };
    expect(caret,work,300,50,400,925);    // ample room, retain 5 px caret gap
    expect(caret,work,300,115,400,925);  // exactly fits
    expect(caret,work,300,116,400,924);  // one pixel overflow -> one pixel up
    expect(caret,work,300,200,400,840);  // not caret.top - gap - height
    expect(caret,work,300,10,400,925);   // shrinking after overflow is not latched
    expect({1900,900,1901,920},work,300,200,1618,840); // retain right reserve
    expect({-10,-30,-9,-10},work,300,200,0,0);
    expect(caret,work,2400,1500,0,0);    // oversized window: valid clamp bounds
    expect(caret,{0,0,1920,990},300,200,400,790); // taskbar/work-area change
    expect({0,10,1,30},{48,0,1920,1080},300,40,48,35); // left taskbar
    expect({20,-30,21,-10},{0,48,1920,1080},300,40,20,48); // top taskbar
    expect({1900,10,1901,30},{0,0,1872,1080},300,40,1570,35); // right taskbar
    expect({-100,-100,-99,-80},{-1920,-1080,0,-40},300,200,-302,-240);
    expect({-1800,-1050,-1799,-1030},{-1920,-1080,0,-40},300,100,-1800,-1025);

    // Crossing the fit boundary must never introduce the old caret-height jump.
    LONG last=placeCandidateWindow(caret,work,300,1).y;
    for(int height=2;height<=1200;++height) {
        const auto p=placeCandidateWindow(caret,work,300,height);
        check(p.y==last || p.y==last-1,"Height change caused a placement flip");
        if(height<=1040)check(p.y+height<=work.bottom,"Final geometry crosses the work-area bottom");
        last=p.y;
    }
    for(int dpi:{96,120,144,192}) {
        const int width=180*dpi/96,height=100*dpi/96;
        for(const RECT w:{RECT{0,0,1920,1040},RECT{-1920,0,0,1040},
                          RECT{1920,80,3840,1440},RECT{-1200,-1080,720,-40}}) {
            RECT c{w.right-100,w.bottom-60,w.right-99,w.bottom-40};
            auto p=placeCandidateWindow(c,w,width,height);
            check(p.y+height==w.bottom,"Scaled candidate did not align with this monitor's work-area bottom");
            check(p.x+width==w.right-2,"Scaled candidate lost the right-edge reserve");
            const auto small=placeCandidateWindow(c,w,width,10);
            check(small.y==c.bottom+5,"Short candidate retained a stale above-caret placement");
            c={w.left+20,w.top+20,w.left+21,w.top+40};
            p=placeCandidateWindow(c,w,width,height);
            check(p.x==c.left && p.y==c.bottom+5,"Monitor change retained a stale placement");
        }
    }
}
}
