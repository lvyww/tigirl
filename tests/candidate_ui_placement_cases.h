#pragma once
#include "candidate_placement_math.h"

// Included after the friend probe's definition: reuse its real windows,
// renderer, controlled clock and TSF boundary instead of building another mock.
inline void tiger::tsf::CandidateUIPresentationProbe::runPlacement(std::shared_ptr<const Dictionary> dictionary) {
    using namespace candidate_probe;
    const auto checksBefore=checks,casesBefore=cases;
    checkCandidatePlacement(require);
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    const RECT start{100,100,101,120};
    require(GetMonitorInfoW(MonitorFromRect(&start,MONITOR_DEFAULTTONEAREST),&monitor)!=FALSE,"Missing work area");
    const RECT work=monitor.rcWork;
    auto atBottom=[&](CandidateUIPresentationProbe& p) {
        p.caret={work.left+100,work.bottom-26,work.left+101,work.bottom-6};
        p.layout();
    };
    auto bottom=[&](const CandidateUIPresentationProbe& p) {
        const auto r=p.rect();
        require(r.y+r.height==work.bottom,"Overflow must clamp to the work-area bottom, not above the caret");
    };
    for(bool vertical:{true,false}) {
        {
            CandidateUIPresentationProbe p(dictionary,vertical);atBottom(p);p.first();bottom(p);
            const auto before=p.rect();p.change();
            require(p.ui->transition_.Active() && p.rect()==before,"Bottom resize lost its normal animation");
            require(p.ui->transition_.Target().y+p.ui->transition_.Target().height==work.bottom,
                    "Bottom resize still targets the caret's upper edge");
            // The unchanged animation rounds top and height independently; allow
            // its existing one-physical-pixel intermediate rounding, not a flip.
            for(int i=0;i<4;++i) {
                p.timer(2,40);const auto r=p.rect();
                require(std::abs(r.y+r.height-work.bottom)<=1,"Bottom resize jumped away from the work area");
            }
            p.timer(2,41);p.finalFrame();bottom(p);
            p.press('C');p.decode({u"first"});p.update();p.pump();
            p.timer(2,201);p.finalFrame();bottom(p);++cases;
        }
        {
            CandidateUIPresentationProbe p(dictionary,vertical);atBottom(p);p.pump();
            const auto placeholder=p.rect();p.decode({u"first",u"second"});failPublish=1;p.update();p.pump();
            require(!p.ui->hasPresentedCandidates_ && !p.ui->transition_.Active(),"Failed bottom first frame latched presentation");
            require(p.rect()==placeholder,"Failed bottom first frame moved the published placeholder");
            p.timer(3,100);p.finalFrame();bottom(p);++cases;
        }
        {
            CandidateUIPresentationProbe p(dictionary,vertical);atBottom(p);p.first();p.change();
            UINT pages[]={0,2};p.ui->SetPageIndex(pages,2);p.ui->SetSelection(3);p.pump();p.timer(2,50);
            const auto revision=p.ui->modelRevision_;const auto flags=p.ui->updatedFlags_;
            const auto frozen=p.rect();p.layout(false,true);
            require(p.visible() && p.rect()==frozen && p.ui->hasPresentedCandidates_,"Bottom layout grace lost its published frame");
            now+=40;p.layout();p.pump();
            UINT selected=0,count=0,actual[2]{};
            p.ui->GetSelection(&selected);p.ui->GetPageIndex(actual,2,&count);
            require(selected==3 && count==2 && actual[0]==0 && actual[1]==2,"Bottom layout recovery reset host selection/pages");
            require(p.ui->modelRevision_==revision && p.ui->updatedFlags_==flags,"Bottom layout recovery dirtied the candidate model");
            require(p.ui->transition_.Active(),"Bottom QQ-style recovery lost animation");
            p.timer(4);p.timer(2,201);p.finalFrame();bottom(p);
            p.layout(false,true);p.timer(4,101);
            require(!p.visible() && !p.ui->hasPresentedCandidates_,"Bottom layout timeout failed to end presentation");
            p.layout();p.pump();p.finalFrame();bottom(p);++cases;
        }
        {
            CandidateUIPresentationProbe p(dictionary,vertical,250,500,true);atBottom(p);p.pump();
            p.timer(1,249);require(!p.visible(),"Bottom placement shortened the candidate delay");
            p.timer(1,1);p.finalFrame();bottom(p);
            require(p.ui->presentation_.items.at(0)==u"1 first","Bottom placement shortened the annotation delay");
            p.timer(1,249);require(p.ui->presentation_.items.at(0)==u"1 first","Bottom annotation appeared early");
            p.timer(1,1);require(p.ui->transition_.Active(),"Bottom annotation expansion no longer animates");
            p.timer(2,201);p.finalFrame();bottom(p);++cases;
        }
        // Frequent word commits recreate the UI; no placement policy may depend
        // on the previous UI surviving. Exercise disabled/default/custom timing.
        for(int duration:{0,100,200})for(int vk:{'A','B','C'}) {
            CandidateUIPresentationProbe p(dictionary,vertical);atBottom(p);
            p.style.animationDurationMs=duration;p.ui->setStyle(p.style,nullptr);
            p.state->engine.cancel();p.state->engine.enableSentenceInput(false,1);p.press(vk);
            p.update();p.pump();p.finalFrame();bottom(p);
            require(p.ui->snapshot_.mode==Mode::Composing,"Word fixture unexpectedly entered sentence mode");
            require(p.ui->snapshot_.total==(vk=='A'?2u:vk=='B'?5u:1u),"Word fixture has wrong candidate count");
            const auto expected=p.state->engine.candidateAt(p.ui->snapshot_.total-1).commit;
            const auto hit=p.ui->itemRects_.back();
            const auto window=p.ui->window_;const auto published=frames.size();
            PostMessageW(window,WM_TIMER,2,0);PostMessageW(window,WM_APP+0x351,0,0);
            // Use the real window's relocated client hit rectangles, not just
            // a direct Finalize call, to protect mouse selection after movement.
            SendMessageW(window,WM_LBUTTONDOWN,0,MAKELPARAM((hit.left+hit.right)/2,(hit.top+hit.bottom)/2));
            require(committed==expected && !p.state->engine.composing(),"Moved word candidate click submitted the wrong text");
            require(!IsWindow(window) && !p.ui->hasPresentedCandidates_,"Word commit did not immediately detach at the bottom");
            MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))DispatchMessageW(&message);
            require(frames.size()==published,"Old word-session messages resurrected the bottom window");++cases;
        }
    }
    std::cout<<"{\"bottom_placement_cases\":"<<cases-casesBefore<<",\"bottom_placement_checks\":"<<checks-checksBefore
             <<",\"synthetic_work_area_dpis\":4,\"real_current_monitor\":true,\"physical_multimonitor_tested\":false}\n";
}
