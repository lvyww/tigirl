// Real CandidateUI/Engine/layered windows. The TSF owner is the existing
// unactivated test double; clock/publication failures are controlled.
#define NOMINMAX
#include <windows.h>
#include <functional>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "../SampleIME/Private.h"
#include "../SampleIME/Globals.h"
#include "../native/tsf/Service.h"
#include "../native/tsf/FrameTransition.h"
#include "../native/tsf/CandidateDpi.h"
#include "../native/LexiconSerialize.h"
namespace orientation_probe {
unsigned checks=0,cases=0,failPublish=0,failShow=0;
ULONGLONG now=1000;
std::vector<tiger::tsf::FrameRect> frames;
std::function<void()> duringPublish;
void require(bool ok,const char* why) { ++checks;if(!ok)throw std::runtime_error(why); }
ULONGLONG WINAPI tick() {return now;}
BOOL WINAPI publish(HWND window,HDC target,POINT* at,SIZE* size,HDC pixels,POINT* origin,COLORREF key,BLENDFUNCTION* blend,DWORD flags) {
    if(failPublish){--failPublish;SetLastError(ERROR_GEN_FAILURE);return FALSE;}
    const auto result=UpdateLayeredWindow(window,target,at,size,pixels,origin,key,blend,flags);
    if(result)frames.push_back({at->x,at->y,size->cx,size->cy});
    if(duringPublish){auto callback=std::move(duringPublish);duringPublish={};callback();}
    return result;
}
BOOL WINAPI show(HWND window,HWND after,int x,int y,int w,int h,UINT flags) {
    if(failShow && (flags&SWP_SHOWWINDOW)){--failShow;SetLastError(ERROR_GEN_FAILURE);return FALSE;}
    return SetWindowPos(window,after,x,y,w,h,flags);
}
}
#define GetTickCount64 orientation_probe::tick
#define UpdateLayeredWindow orientation_probe::publish
#define SetWindowPos orientation_probe::show
#include "../native/tsf/CandidateUI.cpp"
#undef GetTickCount64
#undef UpdateLayeredWindow
#undef SetWindowPos
#include "candidate_ui_probe_owner.h"

namespace tiger::tsf {
struct CandidateUIPresentationProbe {
    ComPtr<Service> owner;
    std::shared_ptr<Context> state;
    std::shared_ptr<const Lexicon> lexicon;
    ComPtr<CandidateUI> ui;
    CandidateStyle style;
    RECT caret{},work{};
    HWND host=nullptr;
    explicit CandidateUIPresentationProbe(std::shared_ptr<const Dictionary> dictionary) {
        using namespace orientation_probe;
        now+=1000;frames.clear();candidate_probe::committed.clear();
        owner.Attach(new Service);lexicon=std::make_shared<Lexicon>(std::move(dictionary));
        state=std::make_shared<Context>(nullptr,lexicon,Config{});
        style.font=u"Segoe UI";style.vertical=true;style.showCode=false;
        style.animationEnabled=false;style.candidateDelayMs=style.annotationDelayMs=0;
        MONITORINFO info{};info.cbSize=sizeof(info);
        POINT origin{};require(GetMonitorInfoW(MonitorFromPoint(origin,MONITOR_DEFAULTTOPRIMARY),&info)!=FALSE,"Work area unavailable");
        work=info.rcWork;caret={work.left+100,work.top+100,work.left+101,work.top+120};
        model(1);create();update();pump();
        const int h=ui->height_,line=MulDiv(20,static_cast<int>(ui->dpi_),96);
        const LONG bottom=work.bottom-h-5-8;
        caret={work.left+100,bottom-line,work.left+101,bottom};
        update(false);pump();below();
    }
    ~CandidateUIPresentationProbe() {
        orientation_probe::duringPublish={};orientation_probe::failPublish=orientation_probe::failShow=0;
        if(ui)ui->detach();ui.Reset();candidate_probe::currentUI=nullptr;
        if(host)DestroyWindow(host);
    }
    void model(unsigned count) {
        state->engine.cancel();state->engine.enableSentenceInput(true,1);
        KeyEvent key;key.vk='A';state->engine.process(key);
        const auto ticket=state->engine.sentenceRequest();
        orientation_probe::require(ticket.has_value(),"No sentence ticket");
        SentenceDecodeResult result;
        for(unsigned i=0;i<count;++i) {SentenceCandidate c;c.text=u"candidate";c.segmentedCode=ticket->raw;result.candidates.push_back(std::move(c));}
        orientation_probe::require(state->engine.applySentenceResult(*ticket,std::move(result)),"Sentence fixture failed");
    }
    void create(bool show=true) {
        if(ui)ui->detach();ui.Reset();
        ui.Attach(new CandidateUI(owner.Get(),state,style,nullptr));candidate_probe::currentUI=ui.Get();
        ui->Show(show?TRUE:FALSE);
    }
    void update(bool content=true) {ui->update(&caret,host,false,content?CandidateUpdate::Content:CandidateUpdate::Layout);}
    void pump() {
        MSG msg{};unsigned count=0;
        while(PeekMessageW(&msg,nullptr,WM_APP+0x351,WM_APP+0x351,PM_REMOVE)) {
            orientation_probe::require(++count<50,"Refresh queue did not settle");DispatchMessageW(&msg);
        }
    }
    RECT rect() {
        RECT r{};orientation_probe::require(GetWindowRect(ui->window_,&r)!=FALSE,"Candidate HWND missing");return r;
    }
    void above() {orientation_probe::require(state->candidateOrientation.above() && rect().bottom==caret.top-5,"Candidate did not remain above");}
    void below() {orientation_probe::require(!state->candidateOrientation.above() && rect().top==caret.bottom+5,"Candidate did not return below");}
    void large() {model(5);update();pump();above();}
    static void run(std::shared_ptr<const Dictionary> dictionary) {
        using namespace orientation_probe;
        {
            CandidateUIPresentationProbe p(dictionary);p.large();
            for(bool abort:{false,true,false}) {
                const auto old=p.ui->window_;
                require(SUCCEEDED(abort?p.ui->Abort():p.ui->Finalize()),"Choice failed");
                require(!IsWindow(old) && !p.state->engine.composing(),"Commit/cancel did not destroy composition UI");
                require(p.state->candidateOrientation.above(),"Detach discarded Context direction");
                p.model(1);p.style.animationEnabled=true;p.create();frames.clear();p.update();p.pump();p.above();
                require(frames.size()==1 && !p.ui->transition_.Active(),"New composition flashed/animated from the wrong side");
                require(frames[0].y+frames[0].height==p.caret.top-5,"First published frame did not inherit above");
            }
            require(!candidate_probe::committed.empty(),"Commit did not produce text");++cases;
        }
        {
            CandidateUIPresentationProbe p(dictionary);p.large();p.model(1);
            const auto original=p.caret;const int threshold=MulDiv(3,static_cast<int>(p.ui->dpi_),96);
            p.caret.top-=threshold;p.caret.bottom-=threshold;p.update();p.pump();p.above();
            --p.caret.top;--p.caret.bottom;p.update(false);p.pump();p.below();
            p.caret=original;p.large();p.model(1);
            for(int i=1;i<=threshold+1;++i) {--p.caret.top;--p.caret.bottom;p.update();p.pump();}
            p.below();++cases;
        }
        {
            CandidateUIPresentationProbe p(dictionary);p.large();p.model(1);
            p.ui->update(nullptr,p.host,true);p.pump();
            require(p.state->candidateOrientation.above(),"NOLAYOUT reset direction");
            RECT invalid{};p.ui->update(&invalid,p.host);p.pump();
            require(!IsWindowVisible(p.ui->window_) && p.state->candidateOrientation.above(),"Zero layout polluted memory or stayed visible");
            p.update();p.pump();p.above();++cases;
        }
        {
            CandidateUIPresentationProbe p(dictionary);p.model(5);failPublish=1;p.update();p.pump();
            require(!p.state->candidateOrientation.above(),"Failed publication latched above");
            p.model(1);p.update();p.pump();p.below();++cases;
        }
        {
            CandidateUIPresentationProbe p(dictionary);p.model(5);
            duringPublish=[&]{p.ui->Show(FALSE);};p.update();p.pump();
            require(!p.state->candidateOrientation.above(),"Reentrant hide latched above");
            p.model(1);p.ui->Show(TRUE);p.update();p.pump();p.below();++cases;
        }
        {
            CandidateUIPresentationProbe p(dictionary);p.model(5);p.create();failShow=1;p.update();p.pump();
            require(!p.state->candidateOrientation.above() && !IsWindowVisible(p.ui->window_),"Failed show latched above");
            p.model(1);p.update();p.pump();p.below();++cases;
        }
        {
            CandidateUIPresentationProbe p(dictionary);
            p.host=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"orientation host",WS_POPUP,
                p.work.left+20,p.work.top+20,300,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            require(p.host!=nullptr,"Owner fixture missing");p.large();p.model(1);p.update();p.pump();p.above();
            require(SetWindowPos(p.host,nullptr,p.work.left+40,p.work.top+20,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER)!=FALSE,"Owner movement failed");
            p.update(false);p.pump();p.below();++cases;
        }
        {
            CandidateUIPresentationProbe p(dictionary);p.large();p.create(false);p.model(1);p.update();p.pump();
            require(!p.ui->window_ && p.state->candidateOrientation.above(),"UI-less session changed direction memory");
            p.ui->Show(TRUE);p.update();p.pump();p.above();++cases;
        }
        {
            CandidateUIPresentationProbe p(dictionary);p.large();p.ui->detach();
            p.state=std::make_shared<Context>(nullptr,p.lexicon,Config{});
            p.model(1);p.create();p.update();p.pump();p.below();++cases;
        }
        {
            CandidateUIPresentationProbe p(dictionary);p.large();p.model(1);
            p.style.showCode=true;p.style.candidateDelayMs=100;p.style.animationEnabled=true;
            p.create();p.update();p.pump();p.above();
            require(!p.ui->hasPresentedCandidates_,"Delayed code placeholder counted as candidates");
            now+=100;frames.clear();SendMessageW(p.ui->window_,WM_TIMER,1,0);p.pump();p.above();
            require(p.ui->hasPresentedCandidates_ && !p.ui->transition_.Active() && frames.size()==1,
                    "Delayed first candidates lost atomic above publication");++cases;
        }
    }
};
}
int wmain(int argc,wchar_t** argv) {
    using namespace orientation_probe;
    try {
        require(argc==2,"orientation_ui <new-fixture-path>");
        require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"COM unavailable");
        tiger::tsf::CandidateDpiScope scope;
        const std::filesystem::path path=argv[1];require(!std::filesystem::exists(path),"Fixture exists");
        tiger::ImportedLexicon data;data.main={{u"a",{u"candidate"}}};data.indexedMain={{u"a",8,0}};
        const auto bytes=tiger::serializeImportedLexicon(data);
        {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));file.close();require(bool(file),"Fixture write failed");}
        tiger::tsf::CandidateUIPresentationProbe::run(tiger::Dictionary::Open(path));
        require(candidate_probe::dllRefs==0,"UI module references leaked");CoUninitialize();
        std::cout<<"{\"probe\":\"candidate_orientation_ui\",\"status\":\"passed\",\"cases\":"<<cases<<",\"checks\":"<<checks
                 <<",\"real_layered_windows\":true,\"tsf_owner_mocked\":true,\"physical_input_tested\":false}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
