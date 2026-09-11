// Exercise the production CandidateUI implementation with real layered windows
// and rendering. Only clock/timer delivery and failures at the Win32 publication
// boundary are controlled. The unactivated TSF owner is a link-time test double.
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <functional>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>
#include "../SampleIME/Private.h"
#include "../SampleIME/Globals.h"
#include "../native/tsf/Service.h"
#include "../native/tsf/CandidateRenderer.h"
#include "../native/tsf/FrameTransition.h"
#include "../native/tsf/CandidateDpi.h"
#include "../native/LexiconSerialize.h"

namespace candidate_probe {
ULONGLONG now=1000;
unsigned checks=0,cases=0,combinedCases=0;
unsigned failPrepare=0,failPublish=0,failShow=0;
std::map<std::pair<HWND,UINT_PTR>,UINT> timers;
struct Frame { tiger::tsf::FrameRect rect;std::vector<std::uint32_t> pixels;bool success=false; };
std::vector<Frame> frames;
std::function<void()> duringPublish,duringShow;
void require(bool ok,const char* message) {
    ++checks;if(!ok)throw std::runtime_error(message);
}
ULONGLONG WINAPI tick() { return now; }
UINT_PTR WINAPI setTimer(HWND window,UINT_PTR id,UINT delay,TIMERPROC) {
    timers[{window,id}]=delay;return id;
}
BOOL WINAPI killTimer(HWND window,UINT_PTR id) { return timers.erase({window,id})?TRUE:FALSE; }
HBITMAP WINAPI dib(HDC dc,const BITMAPINFO* info,UINT usage,void** bits,HANDLE section,DWORD offset) {
    if(failPrepare) { --failPrepare;SetLastError(ERROR_NOT_ENOUGH_MEMORY);return nullptr; }
    return CreateDIBSection(dc,info,usage,bits,section,offset);
}
BOOL WINAPI publish(HWND window,HDC target,POINT* at,SIZE* size,HDC pixels,POINT* origin,COLORREF key,BLENDFUNCTION* blend,DWORD flags) {
    require(at && size && pixels,"Publication missing complete geometry/pixels");
    DIBSECTION bitmap{};
    require(GetObjectW(GetCurrentObject(pixels,OBJ_BITMAP),sizeof(bitmap),&bitmap)!=0 && bitmap.dsBm.bmBits,
            "Publication has no complete backing bitmap");
    Frame frame{{at->x,at->y,size->cx,size->cy}};
    const auto begin=static_cast<const std::uint32_t*>(bitmap.dsBm.bmBits);
    frame.pixels.assign(begin,begin+static_cast<std::size_t>(size->cx)*size->cy);
    if(failPublish) { --failPublish;SetLastError(ERROR_GEN_FAILURE); }
    else frame.success=UpdateLayeredWindow(window,target,at,size,pixels,origin,key,blend,flags)!=FALSE;
    const auto success=frame.success;frames.push_back(std::move(frame));
    if(duringPublish) { auto callback=std::move(duringPublish);duringPublish={};callback(); }
    return success?TRUE:FALSE;
}
BOOL WINAPI show(HWND window,HWND after,int x,int y,int width,int height,UINT flags) {
    if((flags&SWP_SHOWWINDOW) && failShow) { --failShow;SetLastError(ERROR_GEN_FAILURE);return FALSE; }
    const auto result=SetWindowPos(window,after,x,y,width,height,flags);
    if((flags&SWP_SHOWWINDOW) && duringShow) { auto callback=std::move(duringShow);duringShow={};callback(); }
    return result;
}
}
// Include the real implementation once; the production DLL has none of these
// substitutions. Headers declaring the Win32 APIs were included above.
#define GetTickCount64 candidate_probe::tick
#define SetTimer candidate_probe::setTimer
#define KillTimer candidate_probe::killTimer
#define CreateDIBSection candidate_probe::dib
#define UpdateLayeredWindow candidate_probe::publish
#define SetWindowPos candidate_probe::show
#include "../native/tsf/CandidateUI.cpp"
#undef GetTickCount64
#undef SetTimer
#undef KillTimer
#undef CreateDIBSection
#undef UpdateLayeredWindow
#undef SetWindowPos
#include "candidate_ui_probe_owner.h"

namespace tiger::tsf {
struct CandidateUIPresentationProbe {
    Service* owner=new Service;
    std::shared_ptr<Context> state;
    ComPtr<CandidateUI> ui;
    RECT caret{100,100,101,120};
    CandidateStyle style;
    using Rect=FrameRect;
    explicit CandidateUIPresentationProbe(std::shared_ptr<const Dictionary> dictionary,bool vertical=true,
                                         int delay=0,int annotationDelay=0,bool predecoded=false) {
        using namespace candidate_probe;
        now+=1000;frames.clear();timers.clear();committed.clear();
        style.font=u"Segoe UI";style.vertical=vertical;
        style.animationDurationMs=200; // Fixed timeline for intermediate-frame assertions.
        style.candidateDelayMs=delay;style.annotationDelayMs=annotationDelay;
        state=std::make_shared<Context>(nullptr,std::make_shared<Lexicon>(std::move(dictionary)),Config{});
        state->engine.enableSentenceInput(true,1);press('A');
        if(predecoded)decode({u"first"});
        ui.Attach(new CandidateUI(owner,state,style,nullptr));currentUI=ui.Get();
        ui->Show(TRUE);update();
    }
    ~CandidateUIPresentationProbe() {
        ui->detach();ui.Reset();owner->Release();candidate_probe::currentUI=nullptr;
        candidate_probe::duringPublish={};candidate_probe::duringShow={};
        candidate_probe::failPrepare=candidate_probe::failPublish=candidate_probe::failShow=0;
    }
    void press(int vk) { KeyEvent key;key.vk=vk;state->engine.process(key); }
    void decode(std::initializer_list<std::u16string> values) {
        SentenceDecodeResult result;
        const auto ticket=state->engine.sentenceRequest();
        candidate_probe::require(ticket.has_value(),"No pending sentence input");
        for(const auto& value:values) { SentenceCandidate c;c.text=value;c.segmentedCode=ticket->raw;result.candidates.push_back(std::move(c)); }
        candidate_probe::require(state->engine.applySentenceResult(*ticket,std::move(result)),"Sentence fixture rejected");
    }
    void update(bool available=true,bool pending=false) {
        ui->update(available?&caret:nullptr,nullptr,pending,CandidateUpdate::Content);
    }
    void layout(bool available=true,bool pending=false) {
        ui->update(available?&caret:nullptr,nullptr,pending,CandidateUpdate::Layout);
    }
    void pump() {
        MSG message{};unsigned count=0;
        while(PeekMessageW(&message,nullptr,WM_APP+0x351,WM_APP+0x351,PM_REMOVE)) {
            candidate_probe::require(++count<50,"Refresh messages did not settle");DispatchMessageW(&message);
        }
    }
    void timer(UINT_PTR id,unsigned elapsed=0) {
        candidate_probe::now+=elapsed;SendMessageW(ui->window_,WM_TIMER,id,0);pump();
    }
    Rect rect() const {
        RECT r{};candidate_probe::require(GetWindowRect(ui->window_,&r)!=FALSE,"Missing candidate window");
        return {r.left,r.top,r.right-r.left,r.bottom-r.top};
    }
    bool visible() const { return ui->window_ && IsWindowVisible(ui->window_); }
    void finalFrame() const {
        using namespace candidate_probe;
        require(visible() && !frames.empty() && frames.back().success,"No successful visible frame");
        require(rect()==frames.back().rect && rect().width==ui->width_ && rect().height==ui->height_,
                "First candidates did not publish final geometry atomically");
        require(frames.back().pixels==ui->finalPixels_,"Published frame was clipped/incomplete");
        require(!ui->transition_.Active() && !timers.count({ui->window_,2}),"Unexpected first-candidate animation");
        require(ui->hasPresentedCandidates_,"Successful candidate publication was not recorded");
    }
    void first() {
        using namespace candidate_probe;
        pump();require(visible() && !ui->hasPresentedCandidates_,"Code placeholder counted as candidates");
        SendMessageW(ui->window_,WM_PAINT,0,0);
        require(!ui->hasPresentedCandidates_,"WM_PAINT counted as candidate presentation");
        const auto before=frames.size();decode({u"first",u"second"});update();
        require(!ui->hasPresentedCandidates_,"Decode/layout queued prematurely latched presentation");
        duringPublish=[this]{candidate_probe::require(!ui->hasPresentedCandidates_,"State set before publication returned");};
        pump();require(frames.size()==before+1,"First candidates published intermediate frames");finalFrame();
    }
    void change() { press('B');decode({u"a much wider next candidate",u"second",u"third",u"fourth"});update();pump(); }
    static void runPlacement(std::shared_ptr<const Dictionary> dictionary);
    static void run(std::shared_ptr<const Dictionary> dictionary) {
        using namespace candidate_probe;
        for(bool vertical:{true,false}) {
            {
                CandidateUIPresentationProbe p(dictionary,vertical);p.first();
                const auto before=p.rect();p.change();
                require(p.ui->transition_.Active() && p.rect()==before,"Subsequent update lost normal animation");
                p.timer(2,100);require(p.rect()!=before && p.rect()!=p.ui->transition_.Target(),"Missing intermediate animation frame");
                p.timer(2,101);p.finalFrame();++cases;
            }
            {
                CandidateUIPresentationProbe p(dictionary,vertical);p.pump();
                // Even an already-running code-only motion must be cancelled.
                p.caret.left+=80;p.caret.right+=80;p.layout();p.pump();
                require(p.ui->transition_.Active(),"Placeholder-motion precondition failed");
                p.decode({u"first",u"second"});p.update();p.pump();p.finalFrame();++cases;
            }
            for(bool prepareFailure:{true,false}) {
                CandidateUIPresentationProbe p(dictionary,vertical);p.pump();const auto before=p.rect();
                if(prepareFailure)failPrepare=1;else failPublish=1;
                p.decode({u"first",u"second"});p.update();p.pump();
                require(!p.ui->hasPresentedCandidates_ && !p.ui->transition_.Active(),"Failed first frame latched state/animation");
                require(p.rect()==before && timers.count({p.ui->window_,3}),"Failure changed geometry or lost retry");
                p.timer(3,100);p.finalFrame();++cases;
            }
            {
                CandidateUIPresentationProbe p(dictionary,vertical,0,0,true);failShow=1;p.pump();
                require(!p.visible() && !p.ui->hasPresentedCandidates_,"Failed ShowWindow path latched presentation");
                p.timer(3,100);p.finalFrame();++cases;
            }
            {
                CandidateUIPresentationProbe p(dictionary,vertical);p.first();
                p.ui->Show(FALSE);require(!p.visible() && !p.ui->hasPresentedCandidates_,"Explicit hide did not reset session");
                p.state->engine.cancel();p.press('A');p.ui->Show(TRUE);p.update();p.first();++cases;
            }
            {
                CandidateUIPresentationProbe p(dictionary,vertical);p.first();
                p.press('B');p.decode({});p.update();p.pump();
                require(p.ui->hasPresentedCandidates_,"Temporary empty decode reset presentation");
                p.timer(2,201);p.change();require(p.ui->transition_.Active(),"Candidates after empty decode treated as first");++cases;
            }
            {
                CandidateUIPresentationProbe p(dictionary,vertical);p.first();p.change();p.timer(2,50);
                const auto frozen=p.rect();p.layout(false,true);
                require(p.visible() && p.rect()==frozen && p.ui->hasPresentedCandidates_,"Short layout loss reset/hid prior frame");
                require(!p.ui->transition_.Active(),"Layout-pending animation not stopped");
                now+=40;p.layout(false,true);p.pump();
                require(p.visible() && p.ui->hasPresentedCandidates_,"Queued refresh broke layout grace");
                p.layout();p.pump();require(p.ui->transition_.Active(),"QQ-style layout recovery lost animation");
                p.timer(4);require(p.visible() && p.ui->hasPresentedCandidates_,"Old layout deadline hid restored frame");
                p.timer(2,201);p.finalFrame();
                p.layout(false,true);p.timer(4,101);
                require(!p.visible() && !p.ui->hasPresentedCandidates_,"Actual layout timeout did not reset state");
                p.layout();p.pump();p.finalFrame();++cases;
            }
            {
                CandidateUIPresentationProbe p(dictionary,vertical,250,500,true);
                p.pump();require(!p.visible() && !p.ui->hasPresentedCandidates_,"Candidate delay bypassed");
                p.timer(1,249);require(!p.visible() && !p.ui->hasPresentedCandidates_,"Candidates appeared before delay");
                p.timer(1,1);p.finalFrame();
                require(p.ui->presentation_.items.at(0)==u"1 first","Annotation delay bypassed");
                p.timer(1,249);require(p.ui->presentation_.items.at(0)==u"1 first","Annotation appeared too early");
                p.timer(1,1);require(p.ui->transition_.Active(),"Annotation expansion lost existing animation");
                require(p.ui->presentation_.items.at(0).find(u"annotation")!=std::u16string::npos,"Annotation did not expand");++cases;
            }
            for(bool abort:{false,true}) {
                CandidateUIPresentationProbe p(dictionary,vertical);p.first();p.change();
                const auto window=p.ui->window_;const auto before=frames.size();
                for(UINT_PTR timer:{1u,2u,3u,4u})PostMessageW(window,WM_TIMER,timer,0);
                PostMessageW(window,WM_APP+0x351,0,0);
                require(SUCCEEDED(abort?p.ui->Abort():p.ui->Finalize()),"Host choice failed");
                require(!IsWindow(window) && !p.ui->hasPresentedCandidates_ && !p.ui->transition_.Active(),"Commit/cancel did not immediately detach");
                require(!p.state->engine.composing(),"Host failed to finish composition");
                require(abort?committed.empty():committed==u"a much wider next candidate","Commit/cancel output mismatch");
                now+=1000;MSG message{};
                while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))DispatchMessageW(&message);
                p.ui->refreshReveal();p.ui->animate();
                require(!IsWindow(window) && frames.size()==before,"Old messages resurrected detached window");++cases;
            }
            for(bool onShow:{false,true}) {
                CandidateUIPresentationProbe p(dictionary,vertical,0,0,true);
                auto hide=[&p]{p.ui->Show(FALSE);};
                if(onShow)duringShow=hide;else duringPublish=hide;
                p.pump();require(!p.visible() && !p.ui->hasPresentedCandidates_,"Reentrant hide latched/resurrected stale frame");
                p.ui->Show(TRUE);p.update();p.pump();p.finalFrame();++cases;
            }
            {
                CandidateUIPresentationProbe p(dictionary,vertical);p.first();
                DestroyWindow(p.ui->window_);
                require(!p.ui->window_ && !p.ui->hasPresentedCandidates_,"External window destruction retained session");
                p.update();p.pump();p.finalFrame();++cases;
            }
            {
                // Both fixes must coexist in the same sentence display session:
                // first publication is atomic, layout preserves host state, and
                // later geometry/content updates still animate normally.
                CandidateUIPresentationProbe p(dictionary,vertical);p.pump();
                require(p.visible() && !p.ui->hasPresentedCandidates_,"Joint fixture lacks a code placeholder");
                const auto firstFrame=frames.size();
                p.decode({u"first",u"second",u"third",u"fourth",u"fifth",u"sixth",u"seventh"});
                p.update();p.pump();p.finalFrame();
                require(frames.size()==firstFrame+1,"Joint first presentation emitted interpolation frames");
                UINT custom[]={0,2,5};
                require(SUCCEEDED(p.ui->SetPageIndex(custom,3)) && SUCCEEDED(p.ui->SetSelection(5)),
                        "Joint fixture failed to set host selection/pages");
                const auto revision=p.ui->modelRevision_;
                const auto flags=p.ui->updatedFlags_;
                auto preserved=[&] {
                    UINT selected=0,page=0,count=0,indices[3]{};
                    require(SUCCEEDED(p.ui->GetSelection(&selected)) && SUCCEEDED(p.ui->GetCurrentPage(&page)) &&
                            SUCCEEDED(p.ui->GetPageIndex(indices,3,&count)),"Joint host query failed");
                    require(selected==5 && page==2 && count==3 && std::equal(indices,indices+3,custom),
                            "Joint layout reset host selection or paging");
                    require(p.ui->modelRevision_==revision && p.ui->updatedFlags_==flags,
                            "Joint layout rebuilt or dirtied the candidate model");
                    require(p.ui->hasPresentedCandidates_,"Joint layout reset first-presentation state");
                };
                p.caret.left+=80;p.caret.right+=80;p.layout();p.pump();preserved();
                require(p.ui->transition_.Active(),"Joint post-presentation motion lost animation");
                p.timer(2,50);const auto frozen=p.rect();p.layout(false,true);preserved();
                require(p.visible() && p.rect()==frozen && !p.ui->transition_.Active(),
                        "Joint layout grace did not freeze the last frame");
                now+=40;p.layout();p.pump();preserved();
                require(p.ui->transition_.Active(),"Joint layout recovery lost animation");
                p.timer(4);preserved();p.timer(2,201);p.finalFrame();
                p.press('B');p.decode({u"a much wider next candidate",u"second",u"third",u"fourth",u"fifth",u"sixth next",u"seventh"});
                p.update();p.pump();
                require(p.ui->transition_.Active() && p.ui->hasPresentedCandidates_,
                        "Joint content update was incorrectly treated as first presentation");
                UINT selected=0,count=0,indices[2]{};
                require(SUCCEEDED(p.ui->GetSelection(&selected)) && selected==0 &&
                        SUCCEEDED(p.ui->GetPageIndex(indices,2,&count)) && count==2 && indices[0]==0 && indices[1]==5,
                        "Joint content update did not restore engine selection/pages");
                require(p.ui->modelRevision_>revision,"Joint content update did not advance model revision");
                require(SUCCEEDED(p.ui->SetSelection(5)),"Joint final selection failed");
                const auto window=p.ui->window_;const auto published=frames.size();
                PostMessageW(window,WM_TIMER,2,0);PostMessageW(window,WM_APP+0x351,0,0);
                require(SUCCEEDED(p.ui->Finalize()) && committed==u"sixth next", "Joint finalization chose the wrong candidate");
                require(!IsWindow(window) && !p.ui->hasPresentedCandidates_ && p.ui->updatedFlags_==0,
                        "Joint detach did not reset both presentation and notification state");
                MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))DispatchMessageW(&message);
                require(frames.size()==published,"Joint old message republished a detached frame");
                ++combinedCases;++cases;
            }
        }
    }
};
}
#include "candidate_ui_placement_cases.h"

int wmain(int argc,wchar_t** argv) {
    using namespace candidate_probe;
    try {
        require(argc==2,"Usage: candidate_ui_presentation_probe <new-fixture-path>");
        require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"COM initialization failed");
        tiger::tsf::CandidateDpiScope scope;
        const std::filesystem::path path=argv[1];require(!std::filesystem::exists(path),"Fixture exists");
        tiger::ImportedLexicon lexicon;lexicon.main={{u"a",{u"first",u"second"}},{u"b",{u"one",u"two",u"three",u"four",u"five"}},{u"c",{u"only"}}};lexicon.indexedMain={{u"a",8,0},{u"b",8,1},{u"c",8,2}};
        lexicon.comments[u"first"]=u"wide annotation for delayed expansion";
        const auto bytes=tiger::serializeImportedLexicon(lexicon);
        {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));file.close();require(static_cast<bool>(file),"Fixture write failed");}
        tiger::tsf::CandidateUIPresentationProbe::run(tiger::Dictionary::Open(path));
        tiger::tsf::CandidateUIPresentationProbe::runPlacement(tiger::Dictionary::Open(path));
        require(dllRefs==0,"CandidateUI lifetime leaked module references");
        CoUninitialize();
        std::cout<<"{\"status\":\"passed\",\"cases\":"<<cases<<",\"checks\":"<<checks
                 <<",\"combined_layout_presentation_cases\":"<<combinedCases
                 <<",\"real_layered_windows\":true,\"real_renderer\":true,\"controlled_clock\":true,\"tsf_host_mocked\":true,\"physical_input_tested\":false}\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
