// Reuse the production renderer/publication fixture, not its entry point.
#define wmain unusedPresentationProbeMain
#include "candidate_ui_presentation_probe.cpp"
#undef wmain

namespace tiger::tsf {
struct CandidateUIDirectionProbe : CandidateUIPresentationProbe {
    RECT work{};
    int line=20;
    CandidateUIDirectionProbe(std::shared_ptr<const Dictionary> dictionary,bool animation,bool large=true)
        :CandidateUIPresentationProbe(std::move(dictionary),true) {
        const auto monitor=MonitorFromRect(&caret,MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{};info.cbSize=sizeof(info);
        candidate_probe::require(GetMonitorInfoW(monitor,&info)!=FALSE,"No test work area");work=info.rcWork;
        const auto dpi=candidateMonitorDpi(monitor);
        line=MulDiv(20,static_cast<int>(dpi),96);
        caret={work.left+150,work.bottom-MulDiv(70,static_cast<int>(dpi),96)-line,
               work.left+151,work.bottom-MulDiv(70,static_cast<int>(dpi),96)};
        style.fontSize=16;style.showCode=false;style.animationEnabled=animation;
        ui->setStyle(style,nullptr);
        if(large)decode({u"first",u"second",u"third",u"fourth",u"fifth"});
        else decode({u"first"});
        update();
    }
    void settle() {pump();if(ui->transition_.Active())timer(2,201);}
    void above() const {
        candidate_probe::require(visible() && rect().y+rect().height==caret.top-5,
                                 "Published popup lost inherited above direction");
        candidate_probe::require(state->candidatePlacement.above(),"Context did not retain above memory");
    }
    void below() const {
        candidate_probe::require(visible() && rect().y==caret.bottom+5,"Popup did not re-evaluate below");
    }
    void content(bool large) {
        press('B');
        if(large)decode({u"first",u"second",u"third",u"fourth",u"fifth"});
        else decode({u"first"});
        update();
    }
    void reopen(bool commit) {
        const auto oldWindow=ui->window_;
        candidate_probe::require(SUCCEEDED(commit?ui->Finalize():ui->Abort()),"Close composition failed");
        candidate_probe::require(!IsWindow(oldWindow),"Popup survived composition teardown");
        ui.Reset();press('A');decode({u"first"});
        ui.Attach(new CandidateUI(owner,state,style,nullptr));candidate_probe::currentUI=ui.Get();
        ui->Show(TRUE);update();
    }
    static void runDirections(std::shared_ptr<const Dictionary> dictionary) {
        using namespace candidate_probe;
        for(bool animation:{false,true}) {
            CandidateUIDirectionProbe p(dictionary,animation);p.pump();p.finalFrame();p.above();
            p.content(false);p.settle();p.above();
            require(p.caret.bottom+5+p.rect().height<=p.work.bottom,"Short popup must fit below too");
            for(bool commit:{true,false,true}) {
                const auto before=frames.size();p.reopen(commit);p.pump();p.finalFrame();p.above();
                require(frames.size()==before+1,"Recreated popup flashed an intermediate frame");
                ++cases;
            }
            const auto reference=p.state->candidatePlacement.referenceY();
            ++p.caret.top;++p.caret.bottom;p.layout();p.settle();p.above();
            require(p.state->candidatePlacement.referenceY()==reference,"UI jitter rewrote reference Y");
            p.caret.top+=p.line;p.caret.bottom+=p.line;p.layout();p.settle();p.above();
            p.caret.top-=p.line;p.caret.bottom-=p.line;p.layout();p.settle();p.below();++cases;
        }
        {
            CandidateUIDirectionProbe p(dictionary,false);p.pump();p.above();
            const auto version=p.state->candidatePlacement.revision();
            p.layout(false,true);p.timer(4,101);
            require(!p.visible() && p.state->candidatePlacement.revision()==version,"Layout timeout erased direction memory");
            p.content(false);p.pump();p.above();
            const auto after=p.state->candidatePlacement.revision();RECT zero{};
            p.ui->update(&zero,nullptr);p.pump();
            require(!p.visible() && p.state->candidatePlacement.revision()==after,"Zero caret changed memory or displayed stale geometry");
            p.update();p.pump();p.above();++cases;
        }
        for(bool prepare:{false,true}) {
            CandidateUIDirectionProbe p(dictionary,false,false);p.pump();p.below();
            const auto version=p.state->candidatePlacement.revision();
            if(prepare)failPrepare=1;else failPublish=1;
            p.content(true);p.pump();
            require(!p.state->candidatePlacement.above() && p.state->candidatePlacement.revision()==version,
                    "Failed direction target polluted cross-composition memory");
            p.timer(3,100);p.above();++cases;
        }
        {
            CandidateUIDirectionProbe p(dictionary,false);failShow=1;p.pump();
            require(!p.state->candidatePlacement.above(),"Failed first show recorded above direction");
            p.timer(3,100);p.above();++cases;
        }
        {
            CandidateUIDirectionProbe p(dictionary,false);
            duringPublish=[&]{p.ui->Show(FALSE);};p.pump();
            require(!p.state->candidatePlacement.above(),"Reentrant hide recorded an unpublished direction");
            p.ui->Show(TRUE);p.update();p.pump();p.above();++cases;
        }
        {
            CandidateUIDirectionProbe p(dictionary,false);p.pump();p.above();
            p.style.candidateDelayMs=250;p.reopen(true);p.pump();
            require(!p.visible() && p.state->candidatePlacement.above(),"Reveal wait erased or displayed direction");
            p.timer(1,250);p.finalFrame();p.above();++cases;
        }
        {
            CandidateUIDirectionProbe p(dictionary,false);p.pump();p.above();
            HWND host=CreateWindowExW(0,L"STATIC",L"Direction owner",WS_POPUP,100,100,400,300,
                                      nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            require(host!=nullptr,"Cannot create hidden owner fixture");
            struct WindowGuard {HWND value;~WindowGuard(){DestroyWindow(value);}} guard{host};
            p.content(false);p.ui->update(&p.caret,host);p.pump();p.below();
            p.content(true);p.ui->update(&p.caret,host);p.pump();p.above();
            require(SetWindowPos(host,nullptr,100,120,400,300,SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE,"Cannot move hidden owner");
            p.content(false);p.ui->update(&p.caret,host);p.pump();p.below();++cases;
        }
        {
            // A fresh Context must not inherit another context's identical Y.
            CandidateUIDirectionProbe p(dictionary,false,false);p.pump();p.below();++cases;
        }
    }
};
}
int wmain(int argc,wchar_t** argv) {
    using namespace candidate_probe;
    try {
        require(argc==2,"direction_ui_probe <new-fixture-path>");
        require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"COM initialization failed");
        tiger::tsf::CandidateDpiScope scope;
        const std::filesystem::path path=argv[1];require(!std::filesystem::exists(path),"Fixture exists");
        tiger::ImportedLexicon lexicon;lexicon.main={{u"a",{u"first",u"second"}}};lexicon.indexedMain={{u"a",8,0}};
        const auto bytes=tiger::serializeImportedLexicon(lexicon);
        {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));file.close();require(static_cast<bool>(file),"Fixture write failed");}
        tiger::tsf::CandidateUIDirectionProbe::runDirections(tiger::Dictionary::Open(path));
        require(dllRefs==0,"Direction test leaked module references");CoUninitialize();
        std::cout<<"{\"probe\":\"candidate_direction_ui\",\"status\":\"passed\",\"cases\":"<<cases<<",\"checks\":"<<checks
                 <<",\"real_layered_windows\":true,\"real_renderer\":true,\"controlled_clock\":true,\"tsf_host_mocked\":true,\"physical_input_tested\":false}\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
