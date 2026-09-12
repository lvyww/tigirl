// Reuse the controlled-clock production renderer/window harness, not its main.
#define wmain originalPresentationMain
#include "candidate_ui_presentation_probe.cpp"
#undef wmain

namespace tiger::tsf {
struct CandidateUIPlacementProbe : CandidateUIPresentationProbe {
    using Base=CandidateUIPresentationProbe;
    HWND inputOwner=nullptr;
    explicit CandidateUIPlacementProbe(std::shared_ptr<const Dictionary> dictionary)
        :Base(std::move(dictionary),true) {
        style.showCode=false;
        CandidateDpiScope scope;
        const auto monitor=MonitorFromRect(&caret,MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{};info.cbSize=sizeof(info);
        candidate_probe::require(GetMonitorInfoW(monitor,&info)!=FALSE,"Missing test monitor");
        const auto dpi=candidateMonitorDpi(monitor);
        CandidateRenderer smallRenderer(style,{});
        smallRenderer.layout({u"",{u"1 first"}},1000);
        const auto height=static_cast<LONG>(smallRenderer.pixelHeight(dpi));
        caret={info.rcWork.left+100,info.rcWork.bottom-height-35,
               info.rcWork.left+101,info.rcWork.bottom-height-15};
        ui->setStyle(style,nullptr);
    }
    void update() { ui->update(&caret,inputOwner,false,CandidateUpdate::Content); }
    void prepare(bool large) {
        press('B');
        if(large)decode({u"first",u"second",u"third",u"fourth",u"fifth"});
        else decode({u"first"});
        update();
    }
    void settle() { pump();if(ui->transition_.Active())timer(2,201); }
    void draw(bool large) { prepare(large);settle(); }
    void above() {
        candidate_probe::require(state->placement.above(),"Above direction not recorded");
        const auto r=rect();
        candidate_probe::require(r.y+r.height==caret.top-5,"Popup not aligned above current caret");
    }
    void nextComposition(bool vertical=true,int delay=0) {
        ui->detach();ui.Reset();candidate_probe::currentUI=nullptr;
        state->engine.cancel();press('A');decode({u"first"});
        style.vertical=vertical;style.candidateDelayMs=delay;
        ui.Attach(new CandidateUI(owner,state,style,nullptr));candidate_probe::currentUI=ui.Get();
        ui->Show(TRUE);update();pump();
    }
    static void run(std::shared_ptr<const Dictionary> dictionary) {
        using namespace candidate_probe;
        for(bool abort:{false,true})for(bool vertical:{false,true}) {
            CandidateUIPlacementProbe p(dictionary);p.draw(true);p.above();
            const auto revision=p.state->placement.revision();
            const auto window=p.ui->window_;
            require(SUCCEEDED(abort?p.ui->Abort():p.ui->Finalize()),"Choice failed");
            require(!IsWindow(window) && !p.state->engine.composing(),"Choice did not tear down popup/composition");
            require(p.state->placement.above() && p.state->placement.revision()==revision,
                    "Popup teardown cleared direction memory");
            const auto first=frames.size();p.nextComposition(vertical);p.finalFrame();
            require(p.state->placement.above(),"Small next composition forgot the above direction");
            p.above();
            MONITORINFO info{};info.cbSize=sizeof(info);GetMonitorInfoW(MonitorFromRect(&p.caret,MONITOR_DEFAULTTONEAREST),&info);
            require(p.caret.bottom+5+p.rect().height<=info.rcWork.bottom,"Small popup would not fit below: invalid regression");
            require(frames.size()==first+1 && frames[first].rect.y+frames[first].rect.height==p.caret.top-5,
                    "New composition first appeared below before moving above");
            require(!p.ui->transition_.Active(),"New composition inherited resize animation");
            ++cases;
        }
        {
            CandidateUIPlacementProbe p(dictionary);p.draw(true);p.above();
            p.draw(false);p.above(); // Size shrink alone must not change sides.
            const auto reference=p.state->placement.referenceY();
            --p.caret.top;--p.caret.bottom;p.update();p.settle();p.above();
            require(p.state->placement.referenceY()==reference,"Jitter replaced stable reference");
            p.caret.top-=20;p.caret.bottom-=20;p.update();p.settle();
            require(!p.state->placement.above() && p.rect().y==p.caret.bottom+5,"Real upward move did not return below");
            ++cases;
        }
        {
            CandidateUIPlacementProbe p(dictionary);p.draw(true);p.above();
            const auto revision=p.state->placement.revision();
            p.layout(false,true);p.timer(4,101);
            require(!p.visible() && p.state->placement.revision()==revision,"Layout timeout cleared placement memory");
            RECT zero{};p.ui->update(&zero,nullptr);p.pump();
            require(!p.visible() && p.state->placement.revision()==revision,"Zero caret polluted placement memory");
            p.update();p.settle();p.above();
            ++cases;
        }
        for(int failure=0;failure<3;++failure) {
            CandidateUIPlacementProbe p(dictionary);p.prepare(true);
            if(failure==0)failPrepare=1;else if(failure==1)failPublish=1;else failShow=1;
            p.pump();require(!p.state->placement.valid(),"Failed first frame recorded direction");
            p.timer(3,100);p.finalFrame();p.above();++cases;
        }
        {
            CandidateUIPlacementProbe p(dictionary);p.draw(false);
            require(!p.state->placement.above(),"Small initial popup unexpectedly above");
            const auto revision=p.state->placement.revision();
            p.prepare(true);failPublish=1;p.pump();
            require(!p.state->placement.above() && p.state->placement.revision()==revision,
                    "Failed later flip overwrote direction");
            p.timer(3,100);p.settle();p.above();++cases;
        }
        {
            CandidateUIPlacementProbe p(dictionary);p.prepare(true);
            duringPublish=[&p]{p.state->resetPlacement();};
            p.pump();require(!p.state->placement.valid(),"Reentrant reset was overwritten by stale placement");
            p.update();p.settle();p.above();++cases;
        }
        {
            CandidateUIPlacementProbe p(dictionary);p.prepare(true);
            duringShow=[&p]{p.state->resetPlacement();p.ui->Show(FALSE);};
            p.pump();require(!p.visible() && !p.state->placement.valid(),"Reentrant hide published stale direction");
            p.ui->Show(TRUE);p.update();p.settle();p.above();++cases;
        }
        {
            CandidateUIPlacementProbe p(dictionary);p.draw(true);p.above();
            p.nextComposition(true,200);
            require(!p.visible() && p.state->placement.above(),"Delayed reveal cleared remembered direction");
            p.timer(1,200);p.finalFrame();p.above();++cases;
        }
        {
            CandidateUIPlacementProbe p(dictionary);p.draw(true);
            p.state->resetPlacement();p.nextComposition();
            require(!p.state->placement.above() && p.rect().y==p.caret.bottom+5,"Explicit environment reset kept above bias");
            ++cases;
        }
        {
            CandidateUIPlacementProbe p(dictionary);
            HWND owner=CreateWindowExW(0,L"STATIC",L"Placement owner",WS_POPUP,100,100,400,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            require(owner!=nullptr,"Cannot create placement owner");
            p.inputOwner=owner;p.draw(true);p.above();p.draw(false);p.above();
            require(SetWindowPos(owner,nullptr,110,100,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE,"Cannot move placement owner");
            p.update();p.settle();
            require(!p.state->placement.above(),"Owner-window move reused above bias");
            DestroyWindow(owner);p.inputOwner=nullptr;++cases;
        }
    }
};
}
int wmain(int argc,wchar_t** argv) {
    using namespace candidate_probe;
    try {
        require(argc==2,"Usage: candidate_ui_placement_probe <new-fixture-path>");
        require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"COM initialization failed");
        tiger::tsf::CandidateDpiScope scope;
        const std::filesystem::path path=argv[1];require(!std::filesystem::exists(path),"Fixture exists");
        tiger::ImportedLexicon lexicon;lexicon.main={{u"a",{u"first",u"second"}}};lexicon.indexedMain={{u"a",8,0}};
        const auto bytes=tiger::serializeImportedLexicon(lexicon);
        {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
         file.close();require(static_cast<bool>(file),"Fixture write failed");}
        tiger::tsf::CandidateUIPlacementProbe::run(tiger::Dictionary::Open(path));
        require(dllRefs==0,"CandidateUI module references leaked");CoUninitialize();
        std::cout<<"{\"status\":\"passed\",\"probe\":\"candidate_ui_direction_memory\",\"cases\":"<<cases
                 <<",\"checks\":"<<checks<<",\"real_renderer\":true,\"real_layered_windows\":true,"
                   "\"controlled_clock\":true,\"tsf_host_mocked\":true,\"physical_input_tested\":false}\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
