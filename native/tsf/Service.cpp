#define NOMINMAX
#include "../../SampleIME/Private.h"
#include "../../SampleIME/Globals.h"
#include "../../SampleIME/DisplayAttributeInfo.h"
#include "../../SampleIME/EnumDisplayAttributeInfo.h"
#include "Service.h"
#include "ManagementLaunch.h"
#include "PackageLayout.h"
#include "CandidateUI.h"
#include "CandidateDpi.h"
#include "AddWordUI.h"
#include "ManualTimer.h"
#include "../SelectionKeys.h"
#include "../Settings.h"
#include "../ConfigStore.h"
#include "../ReminderLaunch.h"
#include "../SchemaCatalog.h"
#include "../OrdinalCase.h"
#include "../Text.h"
#include <shlobj.h>
#include <KnownFolders.h>
#include <algorithm>
#include <stdexcept>

namespace tiger::tsf {
namespace {
void check(HRESULT hr) { if(FAILED(hr)) throw hr; }
const wchar_t* wide(std::u16string_view value) { return reinterpret_cast<const wchar_t*>(value.data()); }
std::filesystem::path userRoot() {
    wchar_t overridePath[32768];
    const auto length=GetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",overridePath,32768);
    if(length) {
        if(length>=32768) throw std::runtime_error("Native Tiger user root is too long");
        std::filesystem::path root(overridePath);
        if(!root.is_absolute()) throw std::runtime_error("Native Tiger user root must be absolute");
        return root;
    }
    PWSTR local=nullptr;
    // The NativeTiger child is shared, but LocalAppData itself need not be.
    // Resolve the path without probing the inaccessible parent directory.
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_NO_PACKAGE_REDIRECTION|KF_FLAG_DONT_VERIFY,nullptr,&local))) return {};
    std::filesystem::path root;
    try { root=std::filesystem::path(local)/L"Tigirl"; }
    catch(...) { CoTaskMemFree(local); throw; }
    CoTaskMemFree(local); return root;
}
class Session final : public ITfEditSession {
public:
    Session(Service* service,std::function<HRESULT(TfEditCookie)> fn) : owner_(service),fn_(std::move(fn)) {}
    STDMETHODIMP QueryInterface(REFIID iid,void** out) override {
        if(!out) return E_POINTER;
        *out=nullptr;
        if(iid!=IID_IUnknown && iid!=IID_ITfEditSession) return E_NOINTERFACE;
        *out=static_cast<ITfEditSession*>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    STDMETHODIMP_(ULONG) Release() override { auto n=InterlockedDecrement(&refs_); if(!n) delete this; return n; }
    STDMETHODIMP DoEditSession(TfEditCookie cookie) override {
        try { return fn_(cookie); } catch(HRESULT hr) { return hr; } catch(...) { return E_FAIL; }
    }
private:
    LONG refs_=1;
    ComPtr<ITfTextInputProcessorEx> owner_;
    std::function<HRESULT(TfEditCookie)> fn_;
};
bool compartment(IUnknown* object,REFGUID guid) {
    ComPtr<ITfCompartmentMgr> manager;
    ComPtr<ITfCompartment> value;
    if(!object || FAILED(object->QueryInterface(IID_PPV_ARGS(&manager))) || FAILED(manager->GetCompartment(guid,&value))) return false;
    VARIANT v; VariantInit(&v);
    const bool enabled=SUCCEEDED(value->GetValue(&v)) && v.vt==VT_I4 && v.lVal!=0;
    VariantClear(&v); return enabled;
}
void selectionAtEnd(ITfContext* context,ITfRange* range,TfEditCookie cookie) {
    ComPtr<ITfRange> caret;
    check(range->Clone(&caret)); check(caret->Collapse(cookie,TF_ANCHOR_END));
    TF_SELECTION selection{caret.Get(),{TF_AE_NONE,FALSE}};
    check(context->SetSelection(cookie,1,&selection));
}
}
Service::Service() { DllAddRef(); }
Service::~Service() { if(addWordUI_) addWordUI_->close(); hideUI(); DllRelease(); }
ULONG Service::AddRef() { return InterlockedIncrement(&refs_); }
ULONG Service::Release() { auto n=InterlockedDecrement(&refs_); if(!n) delete this; return n; }
HRESULT Service::CreateInstance(IUnknown* outer,REFIID iid,void** object) {
    if(!object) return E_POINTER;
    *object=nullptr;
    if(outer) return CLASS_E_NOAGGREGATION;
    auto service=new(std::nothrow) Service;
    if(!service) return E_OUTOFMEMORY;
    const auto hr=service->QueryInterface(iid,object); service->Release(); return hr;
}
HRESULT Service::QueryInterface(REFIID iid,void** object) {
    if(!object) return E_POINTER;
    *object=nullptr;
    if(iid==IID_IUnknown || iid==IID_ITfTextInputProcessor || iid==IID_ITfTextInputProcessorEx)
        *object=static_cast<ITfTextInputProcessorEx*>(this);
    else if(iid==IID_ITfKeyEventSink) *object=static_cast<ITfKeyEventSink*>(this);
    else if(iid==IID_ITfCompositionSink) *object=static_cast<ITfCompositionSink*>(this);
    else if(iid==IID_ITfThreadMgrEventSink) *object=static_cast<ITfThreadMgrEventSink*>(this);
    else if(iid==IID_ITfThreadFocusSink) *object=static_cast<ITfThreadFocusSink*>(this);
    else if(iid==IID_ITfTextEditSink) *object=static_cast<ITfTextEditSink*>(this);
    else if(iid==IID_ITfTextLayoutSink) *object=static_cast<ITfTextLayoutSink*>(this);
    else if(iid==IID_ITfDisplayAttributeProvider) *object=static_cast<ITfDisplayAttributeProvider*>(this);
    if(!*object) return E_NOINTERFACE;
    AddRef(); return S_OK;
}
void Service::report(const char* message) const {
    OutputDebugStringA("NativeTiger: "); OutputDebugStringA(message); OutputDebugStringA("\n");
}
HRESULT Service::ActivateEx(ITfThreadMgr* manager,TfClientId client,DWORD flags) {
    if(!manager) return E_INVALIDARG;
    if(active_) return E_UNEXPECTED;
    const wchar_t* stage=L"Load dictionary";
    try {
        const auto resources=packageResourceDirectory(Global::dllInstanceHandle);
        dictionaryPath_=resources/L"tiger-v2.tcd";
        auto dictionary=Dictionary::Open(dictionaryPath_);
        fontDirectory_=resources/L"字体";
        lexicon_=std::make_shared<Lexicon>(dictionary);
        secure_=(flags&TF_TMAE_SECUREMODE)!=0;
        stage=L"Resolve user data directory";
        if(!secure_) {
            const auto root=userRoot(); userRoot_=root;
            if(!root.empty()) {
                selectionPath_=root/L"自定义选重键.txt";
                settingsPath_=root/L"config.txt";
            }
        }
        stage=L"Load user settings";
        reloadSettings();
        chinese_=config_.defaultChinese;
        manager_=manager; client_=client; active_=true;
        stage=L"Query thread manager source";
        check(manager_.As(&source_));
        stage=L"Advise thread manager events";
        check(source_->AdviseSink(IID_ITfThreadMgrEventSink,static_cast<ITfThreadMgrEventSink*>(this),&mgrCookie_));
        stage=L"Advise thread focus events";
        check(source_->AdviseSink(IID_ITfThreadFocusSink,static_cast<ITfThreadFocusSink*>(this),&focusCookie_));
        stage=L"Query keystroke manager";
        ComPtr<ITfKeystrokeMgr> keys; check(manager_.As(&keys));
        stage=L"Advise keystroke events";
        check(keys->AdviseKeyEventSink(client_,static_cast<ITfKeyEventSink*>(this),TRUE));
        manager_.As(&uiManager_);
        ComPtr<ITfCategoryMgr> categories;
        stage=L"Create category manager";
        check(CoCreateInstance(CLSID_TF_CategoryMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&categories)));
        stage=L"Register display attribute";
        check(categories->RegisterGUID(Global::SampleIMEGuidDisplayAttributeInput,&attribute_));
        stage=L"Synchronize input mode";
        check(modes_.open(manager_.Get(),client_,chinese_,[this](bool chinese){modeChanged(chinese);}));
        stage=L"Create language bar";
        languageBar_.Attach(new LanguageBar(Global::dllInstanceHandle,Global::SampleIMECLSID,secure_));
        if(!secure_)languageBar_->management(userRoot_,[this] {
            if(!active_ || !store_ || !lexicon_)return;
            if(!addWordUI_)addWordUI_=AddWordUI::create(Global::dllInstanceHandle,*store_,lexicon_->dictionary(),
                [this](std::shared_ptr<const Lexicon> lexicon) {
                    lexicon_=std::move(lexicon);
                    if(languageBar_)languageBar_->userWordFailure(false);
                    for(auto& item:contexts_){item.second->engine.setLexicon(lexicon_);++item.second->revision;}
                });
            auto current=state(focused_.Get(),false);
            addWordUI_->request(current?current->engine.recentElements():std::vector<std::u16string>{});
        });
        check(languageBar_->open(manager_.Get(),client_));
        OnSetThreadFocus();
        if(!secure_ && !userRoot_.empty()) {
            stage=L"Create user data refresh timer";
            dataChanges_.open(userRoot_);
            nextDataWatch_=0; dataDirty_=true;
            dataTimer_=ManualTimer::create(Global::dllInstanceHandle,[this] {
                ComPtr<ITfTextInputProcessorEx> alive(this);
                if(!active_ || !dataTimer_)return;
                dataTimer_->schedule(250);
                pollDataChanges();
            },L"NativeTiger.DataRefresh.");
            dataTimer_->schedule(250);
        }
        if(!secure_) {
            stage=L"Create sentence timer";
            sentenceTimer_=ManualTimer::create(Global::dllInstanceHandle,[this] {
                ComPtr<ITfTextInputProcessorEx> alive(this);pollSentence();
            },L"NativeTiger.Sentence.");
            if(sentenceWorker_ && sentenceWorker_->busy())sentenceTimer_->schedule(10);
        }
        return S_OK;
    } catch(HRESULT hr) {
        OutputDebugStringW(L"NativeTiger activation failed: ");
        OutputDebugStringW(stage);
        Deactivate();
        ComPtr<ICreateErrorInfo> create;
        if(SUCCEEDED(CreateErrorInfo(&create))) {
            create->SetDescription(const_cast<LPOLESTR>(stage));
            ComPtr<IErrorInfo> info; if(SUCCEEDED(create.As(&info))) SetErrorInfo(0,info.Get());
        }
        return hr;
    }
      catch(const std::exception& error) {
        report(error.what());
        // Preserve the failing stage for callers even when a file operation
        // throws a C++ exception instead of returning an HRESULT.
        std::wstring description=stage;
        description+=L": ";
        const auto count=MultiByteToWideChar(CP_UTF8,0,error.what(),-1,nullptr,0);
        if(count>0) {
            std::wstring detail(count,L'\0');
            MultiByteToWideChar(CP_UTF8,0,error.what(),-1,detail.data(),count);
            detail.resize(count-1);description+=detail;
        }
        Deactivate();
        ComPtr<ICreateErrorInfo> create;
        if(SUCCEEDED(CreateErrorInfo(&create))) {
            create->SetDescription(description.data());
            ComPtr<IErrorInfo> info;if(SUCCEEDED(create.As(&info)))SetErrorInfo(0,info.Get());
        }
        return E_FAIL;
      }
}
HRESULT Service::Deactivate() {
    if(sentenceTimer_){sentenceTimer_->close();sentenceTimer_.reset();}
    sentenceWorker_.reset();sentenceResources_.reset();sentenceSignature_.clear();++sentenceRevision_;
    if(dataTimer_) {dataTimer_->close();dataTimer_.reset();}
    dataChanges_.close(); dataDirty_=false; nextDataWatch_=0;
    if(languageBar_) { languageBar_->close(); languageBar_.Reset(); }
    modes_.close(); ++modeRevision_; chinese_=true;
    if(addWordUI_) { addWordUI_->close(); addWordUI_.reset(); }
    active_=false; hideUI();
    std::vector<std::shared_ptr<Context>> contexts;
    for(auto& item:contexts_) contexts.push_back(item.second);
    for(auto& context:contexts) {
        if(context->composition) edit(context,TF_ES_ASYNCDONTCARE|TF_ES_READWRITE,[this,context](TfEditCookie cookie) { return end(context,cookie); });
        forget(context);
    }
    contexts_.clear(); focused_.Reset();
    if(manager_) {
        ComPtr<ITfKeystrokeMgr> keys;
        if(SUCCEEDED(manager_.As(&keys))) keys->UnadviseKeyEventSink(client_);
    }
    if(source_) {
        if(mgrCookie_!=TF_INVALID_COOKIE) source_->UnadviseSink(mgrCookie_);
        if(focusCookie_!=TF_INVALID_COOKIE) source_->UnadviseSink(focusCookie_);
    }
    mgrCookie_=focusCookie_=TF_INVALID_COOKIE;
    source_.Reset(); uiManager_.Reset(); manager_.Reset(); store_.reset(); lexicon_.reset();
    sentenceRequestedSource_.reset();sentenceLoadedSource_.reset();
    selectionPath_.clear(); settingsPath_.clear(); config_=Config{}; candidateStyle_=CandidateStyle{};
    dictionaryPath_.clear(); userRoot_.clear(); schema_.clear();
    fonts_.reset(); fontDirectory_.clear();
    client_=TF_CLIENTID_NULL;
    return S_OK;
}
std::shared_ptr<Context> Service::state(ITfContext* context,bool create) {
    if(!context) return {};
    ComPtr<IUnknown> identity; check(context->QueryInterface(IID_PPV_ARGS(&identity)));
    auto found=contexts_.find(identity.Get());
    if(found!=contexts_.end()) return found->second;
    if(!create || !active_) return {};
    auto result=std::make_shared<Context>(context,lexicon_,config_);
    result->engine.setChinese(chinese_);
    ComPtr<ITfSource> source;
    if(SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&source)))) {
        source->AdviseSink(IID_ITfTextEditSink,static_cast<ITfTextEditSink*>(this),&result->editCookie);
        source->AdviseSink(IID_ITfTextLayoutSink,static_cast<ITfTextLayoutSink*>(this),&result->layoutCookie);
    }
    contexts_.emplace(identity.Get(),result);
    return result;
}
void Service::forget(const std::shared_ptr<Context>& context) {
    if(sentenceWorker_ && context->sentenceQueuedIdentity)sentenceWorker_->cancel(context->sentenceQueuedIdentity);
    context->sentenceDecoder.reset();
    ComPtr<ITfSource> source;
    if(SUCCEEDED(context->context.As(&source))) {
        if(context->editCookie!=TF_INVALID_COOKIE) source->UnadviseSink(context->editCookie);
        if(context->layoutCookie!=TF_INVALID_COOKIE) source->UnadviseSink(context->layoutCookie);
    }
    context->editCookie=context->layoutCookie=TF_INVALID_COOKIE;
}
HRESULT Service::edit(const std::shared_ptr<Context>& context,DWORD flags,std::function<HRESULT(TfEditCookie)> fn) {
    ComPtr<Session> session; session.Attach(new(std::nothrow) Session(this,std::move(fn)));
    if(!session) return E_OUTOFMEMORY;
    HRESULT inner=E_FAIL;
    const auto hr=context->context->RequestEditSession(client_,session.Get(),flags,&inner);
    return FAILED(hr)?hr:inner;
}
HRESULT Service::end(const std::shared_ptr<Context>& context,TfEditCookie cookie) {
    if(!context->composition) return S_OK;
    auto composition=context->composition;
    ComPtr<ITfRange> range; ComPtr<ITfProperty> property;
    if(SUCCEEDED(composition->GetRange(&range)) && SUCCEEDED(context->context->GetProperty(GUID_PROP_ATTRIBUTE,&property)))
        property->Clear(cookie,range.Get());
    // Clear first: EndComposition may reenter OnCompositionTerminated.
    context->composition.Reset();
    const auto hr=composition->EndComposition(cookie);
    if(FAILED(hr)) context->composition=composition;
    return hr;
}
HRESULT Service::apply(const std::shared_ptr<Context>& context,Engine next,KeyResult result,TfEditCookie cookie) {
    struct Guard { bool& flag; Guard(bool& value):flag(value){flag=true;} ~Guard(){flag=false;} } guard(context->editing);
    const auto snapshot=next.snapshot();
    ComPtr<ITfRange> range;
    bool committed=false;
    if(context->composition) {
        check(context->composition->GetRange(&range));
        if(!result.commit.empty() || snapshot.raw.empty() || result.cancelComposition) {
            check(range->SetText(cookie,0,wide(result.commit),static_cast<LONG>(result.commit.size())));
            selectionAtEnd(context->context.Get(),range.Get(),cookie);
            check(end(context,cookie)); committed=true;
            range.Reset();
        }
    }
    if(!result.commit.empty() && !committed) {
        ComPtr<ITfInsertAtSelection> insert; check(context->context.As(&insert));
        check(insert->InsertTextAtSelection(cookie,0,wide(result.commit),static_cast<LONG>(result.commit.size()),&range));
        selectionAtEnd(context->context.Get(),range.Get(),cookie); range.Reset();
    }
    if(!snapshot.raw.empty()) {
        if(!context->composition) {
            ComPtr<ITfInsertAtSelection> insert; ComPtr<ITfContextComposition> compositions;
            check(context->context.As(&insert)); check(context->context.As(&compositions));
            check(insert->InsertTextAtSelection(cookie,TF_IAS_QUERYONLY,nullptr,0,&range));
            check(compositions->StartComposition(cookie,range.Get(),this,&context->composition));
            if(!context->composition) return E_FAIL;
        } else if(!range) check(context->composition->GetRange(&range));
        const auto surface=displayComposition(snapshot,candidateStyle_);
        check(range->SetText(cookie,0,wide(surface),static_cast<LONG>(surface.size())));
        ComPtr<ITfProperty> property;
        if(SUCCEEDED(context->context->GetProperty(GUID_PROP_ATTRIBUTE,&property))) {
            VARIANT value; VariantInit(&value); value.vt=VT_I4; value.lVal=attribute_;
            property->SetValue(cookie,range.Get(),&value);
        }
        selectionAtEnd(context->context.Get(),range.Get(),cookie);
    }
    const auto changes=next.takeUserChanges();
    if(!changes.empty()) {
        try {
            if(!store_) throw std::runtime_error("User-word storage is unavailable");
            lexicon_=store_->commit(changes); next.setLexicon(lexicon_);
            if(languageBar_)languageBar_->userWordFailure(false);
        }
        catch(const std::exception& error) {
            report(error.what()); MessageBeep(MB_ICONWARNING);
            if(languageBar_)languageBar_->userWordFailure(true);
            // Show authoritative persisted data after a failed adjustment,
            // instead of leaving an unpersisted local reorder visible.
            try { if(store_) lexicon_=store_->refresh(); }
            catch(const std::exception& reloadError) { report(reloadError.what()); }
            next.setLexicon(lexicon_);
        }
    }
    context->engine=std::move(next); ++context->revision;
    // All document text operations above succeeded. Preview, failed edit
    // sessions, cancellation, and merely generating a commit cannot enter here.
    if(!secure_ && sentenceSettings_.selfLearning && sentenceLearningStore_ && sentenceWorker_ &&
       !result.commit.empty() && !result.learning.empty()) {
        auto store=sentenceLearningStore_;auto events=std::move(result.learning);
        events.erase(std::remove_if(events.begin(),events.end(),[&](const auto& e){return e.mode!=sentenceLearningMode_;}),events.end());
        for(auto& e:events)e.time=learningNow();
        if(!events.empty() && !sentenceWorker_->submitConfirmed([store,events=std::move(events)] {store->confirm(events);}))
            report("Learning write queue full; input was committed without learning");
        if(sentenceTimer_)sentenceTimer_->schedule(10);
    }
    queueSentence(context);
    publishMode(context);
    if(result.manualTimerMs>=0 && active_ && !secure_) {
        try {
            if(userRoot_.empty() || !userDataRootAvailable())throw std::runtime_error("Reminder user data is unavailable");
            launchReminder(dictionaryPath_.parent_path()/L"Tigirl.Reminder.exe",userRoot_/L"user"/L"timer.txt",result.manualTimerMs);
        } catch(const std::exception& error) { report(error.what()); MessageBeep(MB_ICONWARNING); }
    }
    if(result.toggleHiddenCandidates && active_ && !secure_) {
        try { candidateStyle_.hideCandidates=toggleHiddenCandidates(settingsPath_); }
        catch(const std::exception& error) {
            candidateStyle_.hideCandidates=!candidateStyle_.hideCandidates;
            report(error.what()); MessageBeep(MB_ICONWARNING);
        }
        hideUI();
    }
    if(active_) updateUI(context,cookie);
    if(result.openAddWord && active_ && !secure_ && store_) {
        try {
            if(!addWordUI_) addWordUI_=AddWordUI::create(Global::dllInstanceHandle,*store_,lexicon_->dictionary(),
                [this](std::shared_ptr<const Lexicon> lexicon) {
                    lexicon_=std::move(lexicon);
                    if(languageBar_)languageBar_->userWordFailure(false);
                    for(auto& item:contexts_) { item.second->engine.setLexicon(lexicon_); ++item.second->revision; }
                });
            addWordUI_->request(context->engine.recentElements());
        } catch(const std::exception& error) { report(error.what()); MessageBeep(MB_ICONWARNING); }
    }
    return S_OK;
}
HRESULT Service::key(ITfContext* context,WPARAM vk,LPARAM flags,BOOL* eaten,bool down,bool test) {
    if(!eaten) return E_POINTER;
    *eaten=FALSE;
    if(!active_ || !context) return S_OK;
    struct KeyGuard {unsigned& depth;explicit KeyGuard(unsigned& d):depth(d){++depth;}~KeyGuard(){--depth;}} keyGuard(keyDepth_);
    try {
        TF_STATUS status{};
        if((SUCCEEDED(context->GetStatus(&status)) && (status.dwDynamicFlags&TF_SD_READONLY)) ||
            compartment(context,GUID_COMPARTMENT_KEYBOARD_DISABLED) || compartment(context,GUID_COMPARTMENT_EMPTYCONTEXT)) return S_OK;
        auto current=state(context,true);
        const auto stamp=GetMessageTime();
        if(current->observed && current->observedVk==vk && current->observedFlags==flags &&
            current->observedTime==stamp && current->observedDown==down) {
            if(!test) current->observed=false;
            return S_OK;
        }
        current->observed=false;
        KeyEvent key;
        key.vk=static_cast<int>(vk); key.scan=(flags>>16)&255; key.repeat=flags&65535;
        key.extended=(flags&(1LL<<24))!=0; key.down=down;
        key.shift=(GetKeyState(VK_SHIFT)&0x8000)!=0; key.ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
        key.alt=(GetKeyState(VK_MENU)&0x8000)!=0;
        key.win=((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)!=0;
        key.caps=(GetKeyState(VK_CAPITAL)&1)!=0; key.num=(GetKeyState(VK_NUMLOCK)&1)!=0;
        if(!secure_ && !userRoot_.empty() && key.down && config_.recentSchemaEnabled &&
            config_.recentSchemaShortcut.matches(key.vk,key.shift,key.ctrl,key.alt,key.win)) {
            try { key.recentSchemaAvailable=!recentSchemaName(readConfiguration(settingsPath_),schemaNames(userRoot_)).empty(); }
            catch(const std::exception& error) { if(!test) report(error.what()); }
        }
        Engine next=current->engine;
        // A queued focus edit may not have run yet. Both preview and dispatch
        // must use the active schema before interpreting the next key.
        synchronizeEngine(next);
        SentencePathQueries sentenceQueries;
        if(current->sentenceDecoder && current->sentenceResourceRevision==sentenceLoadedRevision_) {
            auto decoder=current->sentenceDecoder;
            sentenceQueries.complete=[decoder](std::u16string_view raw,std::u16string_view prefix,
                std::optional<std::u16string_view> excluded,bool grouped,const SentenceLockedPrefix* locked) {
                return decoder->hasCompleteCandidate(raw,prefix,excluded,grouped,locked);
            };
            sentenceQueries.properPrefix=[decoder](std::u16string_view raw){return decoder->isProperCodePrefix(raw);};
            sentenceQueries.competingBoundaryEnd=[decoder](
                std::u16string_view raw,int committedRaw,int proposedRaw,int targetElements) {
                return decoder->competingBoundaryEnd(raw,committedRaw,proposedRaw,targetElements);
            };
        }
        auto result=next.process(key,sentenceQueries);
        if(!test && result.awaitSentenceDecode) {
            completeSentenceNow(current,next);result=next.process(key,sentenceQueries);
        }
        // Own the entire Space press after our Ctrl+Space toggle. Passing its
        // repeat/up lets the host/system handle the same shortcut a second time.
        const bool toggledControlSpace=vk==VK_SPACE && down && key.ctrl &&
            !key.shift && !key.alt && !key.win && config_.ctrlSpaceToggle &&
            next.chinese()!=current->engine.chinese();
        const bool consumedSpace=vk==VK_SPACE && current->controlSpaceConsumed;
        if(consumedSpace) result.handled=true;
        const bool nextConsumedSpace=vk==VK_SPACE ?
            (down && (current->controlSpaceConsumed || toggledControlSpace)) : current->controlSpaceConsumed;
        *eaten=result.handled?TRUE:FALSE;
        if(test && result.handled) return S_OK; // pure preview, no file/text writes
        if(result.switchRecentSchema) {
            if(FAILED(launchManagement(Global::dllInstanceHandle,L"recent")))MessageBeep(MB_ICONWARNING);
            result.switchRecentSchema=false;
        }
        // The oracle reports idle Enter as history text while passing the key.
        // The application receives that physical Enter; do not insert it twice.
        if(!result.handled && vk==VK_RETURN && result.commit==u"\n") result.commit.clear();
        const bool textChange=!result.commit.empty() || result.cancelComposition ||
            current->engine.compositionRaw()!=next.compositionRaw();
        if(textChange || result.handled) {
            const auto hr=edit(current,TF_ES_SYNC|TF_ES_READWRITE,[this,current,next=std::move(next),result,nextConsumedSpace](TfEditCookie cookie) mutable {
                const auto applied=apply(current,std::move(next),result,cookie);
                if(SUCCEEDED(applied)) current->controlSpaceConsumed=nextConsumedSpace;
                return applied;
            });
            if(FAILED(hr)) { *eaten=FALSE; report("Keystroke edit session failed"); return hr; }
        } else { current->engine=std::move(next); current->controlSpaceConsumed=nextConsumedSpace; ++current->revision; publishMode(current); }
        if(test) {
            current->observed=true; current->observedVk=vk; current->observedFlags=flags;
            current->observedTime=stamp; current->observedDown=down;
        }
        return S_OK;
    } catch(HRESULT hr) { return hr; }
      catch(const std::exception& error) { report(error.what()); return E_FAIL; }
}
HRESULT Service::OnTestKeyDown(ITfContext* c,WPARAM w,LPARAM l,BOOL* e) { return key(c,w,l,e,true,true); }
HRESULT Service::OnKeyDown(ITfContext* c,WPARAM w,LPARAM l,BOOL* e) { return key(c,w,l,e,true,false); }
HRESULT Service::OnTestKeyUp(ITfContext* c,WPARAM w,LPARAM l,BOOL* e) { return key(c,w,l,e,false,true); }
HRESULT Service::OnKeyUp(ITfContext* c,WPARAM w,LPARAM l,BOOL* e) { return key(c,w,l,e,false,false); }
HRESULT Service::OnPreservedKey(ITfContext*,REFGUID,BOOL* eaten) { if(!eaten) return E_POINTER; *eaten=FALSE; return S_OK; }
HRESULT Service::candidateMenu(POINT point,HWND window) {
    if(!active_ || secure_ || !languageBar_)return S_FALSE;
    ComPtr<ITfTextInputProcessorEx> alive=this;
    auto bar=languageBar_;
    return bar->showMenu(point,window);
}
HRESULT Service::candidateCycle() {
    if(!active_ || secure_ || !ui_ || settingsPath_.empty())return S_FALSE;
    ComPtr<ITfTextInputProcessorEx> alive=this;
    try {
        candidateStyle_=cycleCandidateMode(settingsPath_,horizontalCode_,verticalCode_);
        if(ui_)ui_->setStyle(candidateStyle_,fonts_);
        return S_OK;
    }catch(const std::exception& error){report(error.what());return E_FAIL;}
}
HRESULT Service::candidateWheel(int delta) {
    if(!active_ || secure_ || !ui_ || settingsPath_.empty() || !delta)return S_FALSE;
    ComPtr<ITfTextInputProcessorEx> alive=this;
    try {
        candidateStyle_.fontSize=adjustCandidateFontSize(settingsPath_,delta);
        if(ui_)ui_->setStyle(candidateStyle_,fonts_);
        return S_OK;
    }catch(const std::exception& error){report(error.what());return E_FAIL;}
}
void Service::hideUI() {
    if(ui_) ui_->detach();
    if(uiManager_ && uiId_!=TF_INVALID_UIELEMENTID) uiManager_->EndUIElement(uiId_);
    uiId_=TF_INVALID_UIELEMENTID; ui_.Reset();
}
void Service::updateUI(const std::shared_ptr<Context>& context,TfEditCookie cookie,CandidateUpdate update) {
    if(!active_ || !foreground_ || !context->engine.composing() || (focused_ && focused_.Get()!=context->context.Get())) {
        hideUI(); return;
    }
    const bool created=!ui_ || ui_->context()!=context;
    if(created) {
        hideUI(); ui_.Attach(new CandidateUI(this,context,candidateStyle_,fonts_));
    }
    auto ui=ui_; // Begin/UpdateUIElement can reenter and detach this element.
    if(created) {
        BOOL show=TRUE;DWORD id=TF_INVALID_UIELEMENTID;
        auto manager=uiManager_;
        if(manager && FAILED(manager->BeginUIElement(ui.Get(),&show,&id)))id=TF_INVALID_UIELEMENTID;
        if(!active_ || ui_.Get()!=ui.Get() || ui->context()!=context) {
            if(manager && id!=TF_INVALID_UIELEMENTID)manager->EndUIElement(id);
            return;
        }
        uiId_=id;ui->Show(show);
        // The constructor already published the initial model. Preserve any
        // selection/page table the host installed during BeginUIElement.
        update=CandidateUpdate::Layout;
    }
    ComPtr<ITfContextView> view; ComPtr<ITfRange> range;
    RECT caret{}; BOOL clipped=FALSE; HWND owner=nullptr; bool hasCaret=false,layoutPending=false;
    CandidateFrameTrace::Geometry geometry;
    const bool tracing=ui->tracingGeometry();
    if(tracing){
        geometry.tick=GetTickCount64();
        geometry.reason=update==CandidateUpdate::Layout?"layout":(keyDepth_?"key-edit":"async-edit");
        geometry.callerAwareness=GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext());
    }
    if(SUCCEEDED(context->context->GetActiveView(&view))) {
        view->GetWnd(&owner);
    }
    // Keep GetTextExt and CandidateUI's logical-to-physical conversion in the
    // same host DPI context, independent of how this edit session was dispatched.
    CandidateHostDpiScope hostDpi(owner);
    if(tracing){
        geometry.owner=reinterpret_cast<std::uintptr_t>(owner);
        geometry.queryAwareness=GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext());
        if(owner){
            geometry.ownerAwareness=GetAwarenessFromDpiAwarenessContext(GetWindowDpiAwarenessContext(owner));
            geometry.ownerDpi=GetDpiForWindow(owner);GetWindowRect(owner,&geometry.ownerLogical);
            {CandidateDpiScope physical;GetWindowRect(owner,&geometry.ownerPhysical);}
        }
    }
    if(view) {
        if(context->composition && SUCCEEDED(context->composition->GetRange(&range))) {
            if(SUCCEEDED(range->Collapse(cookie,TF_ANCHOR_END))) {
                const auto layout=view->GetTextExt(cookie,range.Get(),&caret,&clipped);
                if(tracing)geometry.textResult=layout;
                hasCaret=SUCCEEDED(layout) && caret.bottom>caret.top;
                layoutPending=layout==TS_E_NOLAYOUT;
            }
        }
    }
    if(tracing){
        geometry.reported=caret;geometry.clipped=clipped!=FALSE;
        if(hasCaret)geometry.converted=candidatePhysicalCaret(caret,owner);
        GUITHREADINFO info{};info.cbSize=sizeof(info);
        const auto thread=owner?GetWindowThreadProcessId(owner,nullptr):GetCurrentThreadId();
        if(GetGUIThreadInfo(thread,&info) && info.hwndCaret){
            geometry.caretWindow=reinterpret_cast<std::uintptr_t>(info.hwndCaret);
            geometry.caretAwareness=GetAwarenessFromDpiAwarenessContext(GetWindowDpiAwarenessContext(info.hwndCaret));
            geometry.caretDpi=GetDpiForWindow(info.hwndCaret);geometry.guiClient=info.rcCaret;
            CandidateHostDpiScope caretDpi(info.hwndCaret);
            POINT first{info.rcCaret.left,info.rcCaret.top},last{info.rcCaret.right,info.rcCaret.bottom};
            if(ClientToScreen(info.hwndCaret,&first) && ClientToScreen(info.hwndCaret,&last)){
                geometry.guiValid=true;geometry.guiScreen={first.x,first.y,last.x,last.y};
                geometry.guiPhysical=candidatePhysicalCaret(geometry.guiScreen,info.hwndCaret);
            }
        }
        ui->setGeometryTrace(geometry);
    }
    if(!active_ || ui_.Get()!=ui.Get() || ui->context()!=context)return;
    ui->update(hasCaret?&caret:nullptr,owner,layoutPending,update);
    if(ui_.Get()==ui.Get() && FAILED(ui->notifyUpdated(uiManager_.Get(),uiId_)))
        report("Candidate model notification failed");
}
void Service::reloadSchema(std::u16string name) {
    if(secure_ || userRoot_.empty()) return;
    if(name.empty()) name=u"虎码字词";
    const auto available=schemaNames(userRoot_);
    if(std::find(available.begin(),available.end(),name)==available.end()) {
        auto fallback=std::find(available.begin(),available.end(),u"虎码字词");
        name=fallback!=available.end()?*fallback:available.front();
        // Only publish the fallback setting once its compiled data is usable.
        auto prepared=prepareSchema(name);
        selectSchemaConfiguration(settingsPath_,name);
        publishSchema(std::move(prepared));return;
    }
    if(!schema_.empty() && ordinalCompareIgnoreCase(name,schema_)==0) {
        const auto path=activeSchemaDictionaryPath(userRoot_,dictionaryPath_,schema_);
        if(lexicon_ && Dictionary::Open(path)==lexicon_->dictionary()) return;
    }
    publishSchema(prepareSchema(std::move(name)));
}
Service::PreparedSchema Service::prepareSchema(std::u16string name) {
    // A schema is one directory name, never a caller-supplied file path.
    if(!validSchemaName(name))
        throw std::runtime_error("Invalid schema directory name");
    const auto names=schemaNames(userRoot_);
    const auto canonical=std::find_if(names.begin(),names.end(),[&](const auto& item){return ordinalCompareIgnoreCase(name,item)==0;});
    if(canonical==names.end()) throw std::runtime_error("Schema directory not found");
    name=*canonical;
    const auto dictionary=Dictionary::Open(activeSchemaDictionaryPath(userRoot_,dictionaryPath_,name));
    const auto journal=schemaJournalPath(userRoot_,name);
    auto nextStore=std::make_unique<UserStore>(dictionary,journal);
    auto nextLexicon=nextStore->refresh();
    return {std::move(name),std::move(nextStore),std::move(nextLexicon)};
}
void Service::publishSchema(PreparedSchema prepared) {
    // AddWordUI borrows the current store. Close it before replacing that store.
    if(addWordUI_) { addWordUI_->close(); addWordUI_.reset(); }
    store_=std::move(prepared.store); lexicon_=std::move(prepared.lexicon); schema_=std::move(prepared.name);
    if(languageBar_)languageBar_->userWordFailure(false);
}
bool Service::userDataRootAvailable() const {
    // First activation may initialize an absent root. An already loaded service
    // must not mistake temporarily unavailable user data for factory defaults.
    if(schema_.empty())return true;
    std::error_code error;
    return std::filesystem::is_directory(userRoot_,error) && !error;
}
void Service::reloadSettings() {
    if(!userDataRootAvailable())return;
    if(!settingsPath_.empty()) {
        try {
            const auto text=readConfiguration(settingsPath_);
            auto config=parseEngineSettings(text); auto style=parseCandidateStyle(text);
            // Keep the last good selection map if its independently saved file
            // is temporarily unavailable or malformed.
            config.selection=config_.selection;
            reloadSchema(currentSchemaSetting(text));
            if(active_ && !config.reloadRequest.empty() && config.reloadRequest!=config_.reloadRequest) {
                dataDirty_=true;++modeRevision_;chinese_=config.defaultChinese;
            }
            config_=std::move(config); candidateStyle_=std::move(style);
            if(!candidateStyle_.hideCandidates)(candidateStyle_.vertical?verticalCode_:horizontalCode_)=candidateStyle_.showCode;
            refreshSentenceResources(text);
        }
        catch(const std::exception& error) {
            report(error.what());
            // On first activation there is no previous selected schema to keep.
            // Recover the bundled schema's journal as well as its mapped table.
            if(schema_.empty()) {
                try { reloadSchema({}); }
                catch(const std::exception& fallbackError) { report(fallbackError.what()); }
            }
        }
    }
    std::shared_ptr<PrivateFonts> fonts;
    if(!candidateStyle_.font.empty() && candidateStyle_.font.front()==u'#' && !fontDirectory_.empty()) {
        try {
            fonts=PrivateFonts::Open(fontDirectory_);
            if(!fonts->faces()) report("Bundled font unavailable; using system fallback");
        } catch(const std::exception& error) { report(error.what()); }
    }
    fonts_=std::move(fonts);
    if(selectionPath_.empty()) return;
    try { config_.selection=SelectionKeys::load(selectionPath_).dispatch(); }
    catch(const std::exception& error) { report(error.what()); }
}
void Service::synchronizeEngine(Engine& engine) {
    const auto source=sentenceResources_ && sentenceLoadedSource_?sentenceLoadedSource_:lexicon_;
    if(engine.lexicon()->dictionary()!=source->dictionary())engine.switchSchema(source,config_);
    else {
        engine.refreshConfiguration(config_);
        if(engine.lexicon()!=source)engine.setLexicon(source);
    }
    engine.enableSentenceInput(sentenceResources_ && sentenceLoadedSource_ &&
        (sentenceLoadedSource_==source || sentenceLoadedSource_->equivalent(*source)),sentenceLoadedRevision_,
        sentenceSettings_.autoCommit,sentenceSettings_.minimumRetainedRaw);
}
void Service::pollDataChanges() {
    if(!active_ || secure_ || keyDepth_ || refreshingData_)return;
    for(const auto& item:contexts_)if(item.second->editing)return;
    struct Guard {bool& flag;explicit Guard(bool& f):flag(f){flag=true;}~Guard(){flag=false;}} guard(refreshingData_);
    try {
        bool changed=false;
        dataChanges_.poll(changed);
        const auto now=GetTickCount64();
        // Reopening also recovers root deletion/recreation and missed events.
        // Periodic reconciliation does not reset an unchanged engine's page.
        if(now>=nextDataWatch_) {
            dataChanges_.open(userRoot_);nextDataWatch_=now+5000;changed=true;
        }
        if(changed) {
            if(!userDataRootAvailable())return;
            const auto previousConfig=config_;
            const auto previousStyle=candidateStyle_;
            const auto previousLexicon=lexicon_;
            const auto previousFonts=fonts_;
            reloadSettings();
            if(store_) {
                try {lexicon_=store_->refresh();}
                catch(const std::exception&) {
                    // Recreate a removed journal parent through the normal
                    // schema preparation path, retaining the old view on failure.
                    publishSchema(prepareSchema(schema_));
                }
            }
            if(previousLexicon && lexicon_->equivalent(*previousLexicon))lexicon_=previousLexicon;
            if(lexicon_!=previousLexicon)refreshSentenceResources(readConfiguration(settingsPath_));
            if(ui_ && (!(candidateStyle_==previousStyle) || fonts_!=previousFonts))ui_->setStyle(candidateStyle_,fonts_);
            dataDirty_=dataDirty_ || !(config_==previousConfig) || !(candidateStyle_==previousStyle) ||
                lexicon_!=previousLexicon || fonts_!=previousFonts;
        }
        auto current=state(focused_.Get(),false);
        if(!current || current->dataEditQueued || !foreground_)return;
        if(!dataDirty_ && !current->engine.requiresConfigurationReload(config_))return;
        if(!current->composition) {
            auto next=current->engine;synchronizeEngine(next);
            current->engine=std::move(next);++current->revision;dataDirty_=false;
            publishMode(current);return;
        }
        current->dataEditQueued=true;
        const auto hr=edit(current,TF_ES_ASYNCDONTCARE|TF_ES_READWRITE,[this,current](TfEditCookie cookie) {
            current->dataEditQueued=false;
            if(!active_ || keyDepth_ || focused_.Get()!=current->context.Get() ||
                state(current->context.Get(),false)!=current || !foreground_)return S_FALSE;
            // Construct from the current engine at grant time. A key, schema
            // switch or newer file notification may have superseded the request.
            auto next=current->engine;synchronizeEngine(next);
            const auto result=apply(current,std::move(next),{},cookie);
            if(SUCCEEDED(result))dataDirty_=false;
            return result;
        });
        if(FAILED(hr)) {current->dataEditQueued=false;report("Live data refresh edit session failed");}
    } catch(const std::exception& error) {dataDirty_=true;report(error.what());}
      catch(HRESULT) {dataDirty_=true;report("Live data refresh context is unavailable");}
}
void Service::publishMode(const std::shared_ptr<Context>& context) {
    if(!active_ || focused_.Get()!=context->context.Get()) return;
    const bool next=context->engine.chinese();
    if(chinese_!=next) { chinese_=next; ++modeRevision_; }
    if(FAILED(modes_.publish(next))) report("Cannot publish TSF input mode");
    if(languageBar_) {
        TF_STATUS status{};
        const bool enabled=SUCCEEDED(context->context->GetStatus(&status)) && !(status.dwDynamicFlags&TF_SD_READONLY) &&
            !compartment(context->context.Get(),GUID_COMPARTMENT_KEYBOARD_DISABLED) &&
            !compartment(context->context.Get(),GUID_COMPARTMENT_EMPTYCONTEXT);
        languageBar_->update(next,enabled && foreground_);
    }
}
void Service::modeChanged(bool chinese) {
    if(!active_) return;
    const auto revision=++modeRevision_;
    try {
        auto current=state(focused_.Get(),false);
        if(!current) { chinese_=chinese; if(languageBar_)languageBar_->update(chinese,false); return; }
        if(!current->composition) {
            current->engine.setChinese(chinese); ++current->revision;
            current->observed=false; publishMode(current); return;
        }
        // Switching to English commits the raw code, matching the engine's
        // keyboard toggle. TSF may defer the required write lock.
        const auto hr=edit(current,TF_ES_ASYNCDONTCARE|TF_ES_READWRITE,
            [this,current,chinese,revision](TfEditCookie cookie) {
                if(!active_ || revision!=modeRevision_ || focused_.Get()!=current->context.Get()) return S_FALSE;
                try {
                    auto next=current->engine;
                    auto result=next.setChinese(chinese);
                    current->observed=false;
                    return apply(current,std::move(next),result,cookie);
                } catch(...) { publishMode(current); throw; }
            });
        // An accepted asynchronous request may later be refused by the host
        // without invoking our edit session. Keep the displayed mode at the
        // engine's committed state until apply actually succeeds.
        publishMode(current);
        if(FAILED(hr)) report("Input-mode edit session failed");
    } catch(...) {
        modes_.publish(chinese_);
        report("Cannot synchronize external input mode");
    }
}
void Service::refreshFocus(ITfContext* context) {
    const bool changed=focused_.Get()!=context;
    if(changed)++modeRevision_;
    focused_=context;
    if(!context && languageBar_)languageBar_->update(chinese_,false);
    hideUI();
    if(!active_ || !context) return;
    try {
        if(languageBar_) {
            ComPtr<ITfContextView> view;HWND window=nullptr;
            if(SUCCEEDED(context->GetActiveView(&view)) && SUCCEEDED(view->GetWnd(&window)))languageBar_->menuParent(window);
        }
        reloadSettings();
        if(store_ && userDataRootAvailable()) lexicon_=store_->refresh();
        auto current=state(context,true);
        // Focus state changes immediately. A later edit grant must not clear
        // modifiers pressed after this notification.
        if(changed) {current->engine.focusChanged();current->controlSpaceConsumed=false;current->observed=false;++current->revision;}
        const bool switched=current->engine.lexicon()->dictionary()!=lexicon_->dictionary();
        const bool reload=current->engine.requiresConfigurationReload(config_);
        auto next=current->engine;
        synchronizeEngine(next);
        if((switched || reload) && current->composition) {
            edit(current,TF_ES_ASYNCDONTCARE|TF_ES_READWRITE,[this,current](TfEditCookie cookie) {
                if(!active_ || !lexicon_ || !foreground_ || focused_.Get()!=current->context.Get() ||
                    state(current->context.Get(),false)!=current)return S_FALSE;
                auto refreshed=current->engine;
                synchronizeEngine(refreshed);
                return apply(current,std::move(refreshed),{},cookie);
            });
            return;
        }
        current->engine=std::move(next); ++current->revision;
        publishMode(current);
        if(current->composition) edit(current,TF_ES_ASYNCDONTCARE|TF_ES_READ,[this,current](TfEditCookie cookie) { updateUI(current,cookie); return S_OK; });
    } catch(const std::exception& error) { report(error.what()); }
      catch(HRESULT) { report("Focus context is unavailable"); }
}
HRESULT Service::OnSetFocus(ITfDocumentMgr* doc,ITfDocumentMgr*) {
    ComPtr<ITfContext> context;
    if(doc) doc->GetTop(&context);
    refreshFocus(context.Get()); return S_OK;
}
HRESULT Service::OnSetFocus(BOOL foreground) {
    foreground_=foreground!=FALSE;
    if(!foreground_) {
        ++modeRevision_; hideUI(); if(languageBar_)languageBar_->update(chinese_,false);
        for(auto& item:contexts_) {item.second->engine.focusChanged();item.second->controlSpaceConsumed=false;item.second->observed=false;}
    }
    return S_OK;
}
HRESULT Service::OnSetThreadFocus() {
    foreground_=true;
    ComPtr<ITfDocumentMgr> doc;
    if(manager_) manager_->GetFocus(&doc);
    return OnSetFocus(doc.Get(),nullptr);
}
HRESULT Service::OnKillThreadFocus() {
    ++modeRevision_;
    foreground_=false; hideUI();
    if(languageBar_)languageBar_->update(chinese_,false);
    for(auto& item:contexts_) { item.second->engine.focusChanged(); item.second->controlSpaceConsumed=false; item.second->observed=false; }
    return S_OK;
}
HRESULT Service::OnPopContext(ITfContext* context) {
    try {
        auto current=state(context,false);
        if(!current) return S_OK;
        if(ui_ && ui_->context()==current) hideUI();
        if(current->composition) edit(current,TF_ES_ASYNCDONTCARE|TF_ES_READWRITE,[this,current](TfEditCookie cookie) { return end(current,cookie); });
        forget(current);
        if(focused_.Get()==context) { focused_.Reset(); if(languageBar_)languageBar_->update(chinese_,false); }
        for(auto it=contexts_.begin();it!=contexts_.end();++it) if(it->second==current) { contexts_.erase(it); break; }
        return S_OK;
    } catch(...) { return E_FAIL; }
}
HRESULT Service::OnUninitDocumentMgr(ITfDocumentMgr* doc) {
    std::vector<ComPtr<ITfContext>> removed;
    for(auto& item:contexts_) {
        ComPtr<ITfDocumentMgr> owner;
        if(SUCCEEDED(item.second->context->GetDocumentMgr(&owner)) && owner.Get()==doc) removed.push_back(item.second->context);
    }
    for(auto& context:removed) OnPopContext(context.Get());
    return S_OK;
}
HRESULT Service::OnCompositionTerminated(TfEditCookie cookie,ITfComposition* composition) {
    for(auto& item:contexts_) if(item.second->composition.Get()==composition) {
        auto& current=item.second;
        ComPtr<ITfRange> range; ComPtr<ITfProperty> property;
        if(SUCCEEDED(composition->GetRange(&range)) && SUCCEEDED(current->context->GetProperty(GUID_PROP_ATTRIBUTE,&property)))
            property->Clear(cookie,range.Get());
        current->composition.Reset(); current->engine.cancel(); ++current->revision;
        queueSentence(current);
        if(ui_ && ui_->context()==current) hideUI();
        break;
    }
    return S_OK;
}
HRESULT Service::OnEndEdit(ITfContext* context,TfEditCookie cookie,ITfEditRecord* record) {
    try {
        auto current=state(context,false);
        if(!current || !current->composition || current->editing || !record) return S_OK;
        BOOL changed=FALSE; record->GetSelectionStatus(&changed);
        if(!changed) return S_OK;
        const auto outside=[current](TfEditCookie queryCookie) {
            TF_SELECTION selection{};ULONG fetched=0;
            if(FAILED(current->context->GetSelection(queryCookie,TF_DEFAULT_SELECTION,1,&selection,&fetched)) || !fetched)return false;
            ComPtr<ITfRange> selected;selected.Attach(selection.range);
            ComPtr<ITfRange> composing;check(current->composition->GetRange(&composing));
            LONG start=0,endPosition=0;
            check(composing->CompareStart(queryCookie,selected.Get(),TF_ANCHOR_START,&start));
            check(composing->CompareEnd(queryCookie,selected.Get(),TF_ANCHOR_END,&endPosition));
            return start>0 || endPosition<0;
        };
        if(outside(cookie)) {
            const auto revision=current->revision;
            edit(current,TF_ES_ASYNC|TF_ES_READWRITE,[this,current,revision,outside](TfEditCookie editCookie) {
                if(!active_ || current->revision!=revision || !current->composition ||
                    state(current->context.Get(),false)!=current || !outside(editCookie)) return S_OK;
                const auto hr=end(current,editCookie);
                if(SUCCEEDED(hr)) {
                    current->engine.cancel(); ++current->revision;
                    if(ui_ && ui_->context()==current) hideUI();
                }
                return hr;
            });
        }
        return S_OK;
    } catch(HRESULT hr) { return hr; } catch(...) { return E_FAIL; }
}
HRESULT Service::OnLayoutChange(ITfContext* context,TfLayoutCode code,ITfContextView*) {
    try {
        auto current=state(context,false);
        if(!current || current->editing) return S_OK;
        if(code==TF_LC_DESTROY) { if(ui_ && ui_->context()==current) hideUI(); return S_OK; }
        if(current->composition) edit(current,TF_ES_ASYNCDONTCARE|TF_ES_READ,[this,current](TfEditCookie cookie) {
            // Queued layout work must not recreate an ended UI or hide a new
            // focused context's candidates after focus/pop/deactivation.
            if(!active_ || !foreground_ || !current->composition ||
                focused_.Get()!=current->context.Get() || state(current->context.Get(),false)!=current)return S_FALSE;
            updateUI(current,cookie,CandidateUpdate::Layout);return S_OK;
        });
        return S_OK;
    } catch(...) { return E_FAIL; }
}
HRESULT Service::choose(const std::shared_ptr<Context>& current,UINT index,bool abort) {
    if(!active_ || !current || !current->engine.composing()) return TF_E_DISCONNECTED;
    const auto revision=current->revision;
    return edit(current,TF_ES_ASYNCDONTCARE|TF_ES_READWRITE,[this,current,index,abort,revision](TfEditCookie cookie) {
        if(!active_ || current->revision!=revision) return S_FALSE;
        Engine next=current->engine; KeyResult result;
        if(abort) { next.cancel(); result.handled=true; result.cancelComposition=true; }
        else {
            completeSentenceNow(current,next);
            auto selection=next.selectCandidate(index);
            if(!selection) return E_INVALIDARG;
            result=std::move(*selection);
        }
        return apply(current,std::move(next),result,cookie);
    });
}
HRESULT Service::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** values) {
    if(!values) return E_POINTER;
    *values=new(std::nothrow) CEnumDisplayAttributeInfo;
    return *values?S_OK:E_OUTOFMEMORY;
}
HRESULT Service::GetDisplayAttributeInfo(REFGUID guid,ITfDisplayAttributeInfo** value) {
    if(!value) return E_POINTER;
    *value=nullptr;
    if(guid==Global::SampleIMEGuidDisplayAttributeInput) *value=new(std::nothrow) CDisplayAttributeInfoInput;
    else if(guid==Global::SampleIMEGuidDisplayAttributeConverted) *value=new(std::nothrow) CDisplayAttributeInfoConverted;
    else return E_INVALIDARG;
    return *value?S_OK:E_OUTOFMEMORY;
}
}

HRESULT CreateNativeTiger(IUnknown* outer,REFIID iid,void** object) { return tiger::tsf::Service::CreateInstance(outer,iid,object); }
