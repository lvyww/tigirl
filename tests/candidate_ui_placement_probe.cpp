// Reuse the existing production-window fixture and its controlled clock/failure
// boundaries; give its standalone entry point a different name in this binary.
// No second copy of the TSF mock or the real rendering/publication plumbing.
#define wmain presentationBaselineMain
#include "candidate_ui_presentation_probe.cpp"
#undef wmain
#include "../native/tsf/CandidatePlacement.h"

namespace candidate_probe {
unsigned geometryChecks=0,placementCases=0;

void geometry() {
    using tiger::tsf::candidatePosition;
    using tiger::tsf::FrameRect;
    using tiger::tsf::FrameTransition;
    const auto before=checks;
    for(const RECT work : {RECT{0,0,1920,1040}, RECT{-1920,-1080,0,-40},
                          RECT{1920,40,3840,1080}, RECT{80,0,1920,1080},
                          RECT{0,0,1840,1080}, RECT{0,0,1920,1080}}) {
        for(int dpi : {96,120,144,192}) {
            const RECT caret{work.left+100,work.bottom-140,work.left+101,work.bottom-120};
            const int width=MulDiv(180,dpi,96);
            POINT previous{};
            // Cross the fit threshold in one-pixel steps: no discontinuous flip.
            for(int height=1;height<=350;++height) {
                const auto at=candidatePosition(caret,work,width,height);
                const LONG expected=std::min(caret.bottom+5,work.bottom-height-2);
                require(at.x==caret.left && at.y==expected,"Rectangle placement differs from bottom clamp");
                require(at.y>=work.top && at.y+height<=work.bottom-2,"Rectangle escaped work area");
                if(height>1)require(std::abs(at.y-previous.y)<=1,"One-pixel growth caused a placement jump");
                previous=at;
            }
            for(int height : {180,25,300,10,180}) {
                const auto at=candidatePosition(caret,work,width,height);
                require(at.y==std::min(caret.bottom+5,work.bottom-height-2),"Shrink/new word retained direction state");
            }
            RECT edge{work.right-1,work.bottom-21,work.right,work.bottom-1};
            const int height=MulDiv(100,dpi,96);
            const auto at=candidatePosition(edge,work,width,height);
            require(at.x+width==work.right-2 && at.y+height==work.bottom-2,"Right/bottom work-area inset changed");
            const auto huge=candidatePosition(edge,work,5000,5000);
            require(huge.x==work.left && huge.y==work.top,"Oversized popup lost top/left guard");
            RECT outside{work.left-100,work.top-50,work.left-99,work.top-30};
            const auto top=candidatePosition(outside,work,width,height);
            require(top.x==work.left && top.y==work.top,"Negative/outside caret was not clamped");
            // Same interpolation used in production; allow only integer-rounding
            // error at the 2 px inset, never a taskbar crossing or an above jump.
            const auto small=candidatePosition(edge,work,width,30);
            FrameTransition animation;
            animation.Start({small.x,small.y,width,30},{at.x,at.y,width,height},1000,60,100);
            for(unsigned tick=0;tick<=100;++tick) {
                const auto frame=animation.Sample(1000+tick);
                require(std::abs(frame.y+frame.height-(work.bottom-2))<=1,"Animation changed bottom anchor");
                require(frame.y+frame.height<=work.bottom,"Animation crossed work-area bottom");
            }
        }
    }
    geometryChecks=checks-before;
}

RECT workArea() {
    MONITORINFO info{};info.cbSize=sizeof(info);
    require(GetMonitorInfoW(MonitorFromPoint({100,100},MONITOR_DEFAULTTONEAREST),&info)!=FALSE,"No monitor work area");
    require(info.rcWork.bottom-info.rcWork.top>=500,"Window fixture requires at least 500 physical pixels of height");
    return info.rcWork;
}
void nearBottom(tiger::tsf::CandidateUIPresentationProbe& p,const RECT& work) {
    p.caret={work.left+100,work.bottom-36,work.left+101,work.bottom-16};p.layout();
}
void atExpectedPosition(tiger::tsf::CandidateUIPresentationProbe& p,const RECT& work) {
    const auto r=p.rect();
    // Independent oracle, not a call back to the production positioning helper.
    const LONG expected=std::max(work.top,std::min(p.caret.bottom+5,work.bottom-r.height-2));
    require(r.y==expected,"Bottom-edge candidate flipped above caret instead of sliding into work area");
    require(r.x>=work.left && r.x+r.width<=work.right,"Published window escaped work-area width");
}
bool animating() {
    return std::any_of(timers.begin(),timers.end(),[](const auto& entry){return entry.first.second==2;});
}
void settle(tiger::tsf::CandidateUIPresentationProbe& p,const RECT& work) {
    p.timer(2,201);p.finalFrame();atExpectedPosition(p,work);
}
void checkIntermediateBottom(tiger::tsf::CandidateUIPresentationProbe& p,const RECT& work) {
    require(animating(),"Post-presentation resize lost animation");p.timer(2,100);
    const auto r=p.rect();
    require(std::abs(r.y+r.height-(work.bottom-2))<=1,"Published animation lost the work-area bottom anchor");
}
void hiddenAfterChoice(tiger::tsf::CandidateUIPresentationProbe& p,bool abort) {
    const auto window=FindWindowW(L"NativeTiger.Candidate.v1",nullptr);
    require(window!=nullptr,"No window to test stale messages");
    const auto count=frames.size();
    for(UINT_PTR timer : {1u,2u,3u,4u})PostMessageW(window,WM_TIMER,timer,0);
    PostMessageW(window,WM_APP+0x351,0,0);
    require(SUCCEEDED(abort?p.ui->Abort():p.ui->Finalize()),"Commit/cancel failed");
    require(!IsWindow(window) && !p.state->engine.composing(),"Commit/cancel left candidate UI alive");
    now+=1000;MSG message{};
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))DispatchMessageW(&message);
    require(frames.size()==count,"Stale animation republished a committed/cancelled window");
}

void windows(std::shared_ptr<const tiger::Dictionary> dictionary) {
    using tiger::tsf::CandidateUIPresentationProbe;
    const auto work=workArea();
    for(bool vertical : {true,false}) {
        {
            CandidateUIPresentationProbe p(dictionary,vertical);nearBottom(p,work);
            p.first();atExpectedPosition(p,work);
            require(p.rect().y+p.rect().height==work.bottom-2,"First candidate frame missed bottom edge");
            p.change();checkIntermediateBottom(p,work);settle(p,work);
            const int tall=p.rect().height;
            p.press('C');p.decode({u"first"});p.update();p.pump();settle(p,work);
            const int small=p.rect().height;
            if(vertical) {
                require(tall>small,"Vertical fixture did not change candidate row count");
                p.caret.bottom=work.bottom-(tall+small)/2-7;p.caret.top=p.caret.bottom-20;
                p.layout();p.pump();settle(p,work);
                require(p.rect().y==p.caret.bottom+5,"Fitting small candidate did not return below caret");
                p.change();settle(p,work);
                require(p.rect().y+p.rect().height==work.bottom-2,"Growing candidate did not slide to bottom edge");
                p.press('D');p.decode({u"first"});p.update();p.pump();settle(p,work);
                require(p.rect().y==p.caret.bottom+5,"Shrinking candidate retained an above-caret latch");
            }
            ++placementCases;
        }
        {
            CandidateUIPresentationProbe p(dictionary,vertical);nearBottom(p,work);p.pump();
            failPublish=1;p.decode({u"first",u"second"});p.update();p.pump();
            require(!animating(),"Failed first frame started animation");
            p.timer(3,100);p.finalFrame();atExpectedPosition(p,work);++placementCases;
        }
        {
            CandidateUIPresentationProbe p(dictionary,vertical);nearBottom(p,work);p.first();
            UINT custom[]={0,1};require(SUCCEEDED(p.ui->SetPageIndex(custom,2)) && SUCCEEDED(p.ui->SetSelection(1)),"Host state fixture failed");
            DWORD flags=0;p.ui->GetUpdatedFlags(&flags);
            p.caret.left+=40;p.caret.right+=40;p.layout();p.pump();p.timer(2,50);
            const auto frozen=p.rect();p.layout(false,true);now+=40;p.pump();
            require(p.visible() && p.rect()==frozen,"QQ layout grace hid or moved the frame");
            p.layout();p.pump();require(animating(),"QQ recovery no longer animates");settle(p,work);
            UINT selected=0,page=0,count=0,indices[2]{};DWORD after=0;
            p.ui->GetSelection(&selected);p.ui->GetCurrentPage(&page);p.ui->GetUpdatedFlags(&after);
            require(SUCCEEDED(p.ui->GetPageIndex(indices,2,&count)) && selected==1 && page==1 && count==2 && indices[1]==1 && after==flags,
                    "Bottom-edge layout reset host selection/pages or dirtied notifications");
            p.layout(false,true);p.timer(4,101);require(!p.visible(),"Layout timeout failed to hide");
            p.layout();p.pump();p.finalFrame();atExpectedPosition(p,work);++placementCases;
        }
        {
            CandidateUIPresentationProbe p(dictionary,vertical,250,500,true);nearBottom(p,work);p.pump();
            p.timer(1,249);require(!p.visible(),"Bottom-edge placement bypassed reveal delay");
            p.timer(1,1);p.finalFrame();atExpectedPosition(p,work);
            p.timer(1,249);require(!animating(),"Annotation expanded before its deadline");
            p.timer(1,1);require(animating(),"Delayed annotation expansion lost animation");
            settle(p,work);++placementCases;
        }
        for(bool abort : {false,true}) {
            // Fresh word-mode UIs emulate repeated short compositions. Page 2
            // has only two candidates instead of five; no sentence-only latch.
            for(unsigned word=0;word<3;++word) {
                CandidateUIPresentationProbe p(dictionary,vertical);nearBottom(p,work);
                p.state->engine.enableSentenceInput(false,1);p.press('A');p.update();p.pump();
                p.finalFrame();atExpectedPosition(p,work);
                if(word==1) { p.state->engine.setPage(1);p.update();p.pump();settle(p,work); }
                p.press('B');p.update();p.pump();
                require(animating(),"Word-mode candidate update lost animation");
                checkIntermediateBottom(p,work);settle(p,work);
                // Begin another resize, then commit/cancel before its timer fires.
                p.press('C');p.update();p.pump();require(animating(),"Word-mode hide fixture has no animation");
                hiddenAfterChoice(p,abort);
                require(abort?committed.empty():committed==u"short","Word-mode commit selected the wrong text");
            }
            ++placementCases;
        }
    }
}
}

int wmain(int argc,wchar_t** argv) {
    using namespace candidate_probe;
    try {
        require(argc==2,"Usage: candidate_ui_placement_probe <new-fixture-path>");
        require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"COM initialization failed");
        tiger::tsf::CandidateDpiScope scope;
        geometry();
        const std::filesystem::path path=argv[1];require(!std::filesystem::exists(path),"Fixture exists");
        tiger::ImportedLexicon lexicon;
        lexicon.main={{u"a",{u"first",u"second",u"third",u"fourth",u"fifth",u"sixth",u"seventh"}},
                      {u"ab",{u"a substantially wider word candidate",u"second"}}, {u"abc",{u"short"}}};
        lexicon.indexedMain={{u"a",8,0},{u"ab",8,0},{u"abc",8,0}};
        lexicon.comments[u"first"]=u"wide annotation for delayed expansion";
        const auto bytes=tiger::serializeImportedLexicon(lexicon);
        {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
         file.close();require(static_cast<bool>(file),"Fixture write failed");}
        windows(tiger::Dictionary::Open(path));
        require(dllRefs==0,"Placement test leaked module references");CoUninitialize();
        std::cout<<"{\"status\":\"passed\",\"placement_cases\":"<<placementCases<<",\"checks\":"<<checks
                 <<",\"geometry_checks\":"<<geometryChecks<<",\"real_layered_windows\":true,\"controlled_clock\":true,\"physical_chat_host_tested\":false}\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
