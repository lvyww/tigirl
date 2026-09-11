#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <msctf.h>
#include <wrl/client.h>
#include <functional>
#include <map>
#include "../Engine.h"
#include "../CandidatePresentation.h"
#include "PrivateFonts.h"
#include "ModeCompartments.h"
#include "LanguageBar.h"
#include "DirectoryChanges.h"
#include "SentenceWorker.h"
#include "../SentenceSettings.h"
#include "../UserStore.h"

namespace tiger::tsf {
template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
// Input/model changes and caret/layout notifications have different ownership.
enum class CandidateUpdate { Content, Layout };
class CandidateUI;
class AddWordUI;
class ManualTimer;
struct Context {
    Context(ITfContext* value,std::shared_ptr<const Lexicon> lexicon,Config config) : context(value),engine(std::move(lexicon),std::move(config)) {}
    ComPtr<ITfContext> context;
    ComPtr<ITfComposition> composition;
    Engine engine;
    std::shared_ptr<SentenceDecoder> sentenceDecoder;
    std::uint64_t sentenceResourceRevision=0,sentenceQueuedGeneration=0,sentenceQueuedIdentity=0;
    DWORD editCookie=TF_INVALID_COOKIE,layoutCookie=TF_INVALID_COOKIE;
    bool editing=false,observed=false,dataEditQueued=false;
    WPARAM observedVk=0;
    LPARAM observedFlags=0;
    LONG observedTime=0;
    bool observedDown=false;
    bool controlSpaceConsumed=false;
    std::uint64_t revision=0;
};

class Service final : public ITfTextInputProcessorEx, public ITfKeyEventSink,
    public ITfCompositionSink, public ITfThreadMgrEventSink, public ITfThreadFocusSink,
    public ITfTextEditSink, public ITfTextLayoutSink, public ITfDisplayAttributeProvider {
public:
    Service();
    ~Service();
    static HRESULT CreateInstance(IUnknown* outer,REFIID iid,void** object);
    STDMETHODIMP QueryInterface(REFIID iid,void** object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP Activate(ITfThreadMgr* mgr,TfClientId client) override { return ActivateEx(mgr,client,0); }
    STDMETHODIMP ActivateEx(ITfThreadMgr* mgr,TfClientId client,DWORD flags) override;
    STDMETHODIMP Deactivate() override;
    STDMETHODIMP OnSetFocus(BOOL foreground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* c,WPARAM w,LPARAM l,BOOL* eaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* c,WPARAM w,LPARAM l,BOOL* eaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* c,WPARAM w,LPARAM l,BOOL* eaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* c,WPARAM w,LPARAM l,BOOL* eaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext*,REFGUID,BOOL* eaten) override;
    STDMETHODIMP OnCompositionTerminated(TfEditCookie cookie,ITfComposition* composition) override;
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* doc) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* doc,ITfDocumentMgr*) override;
    STDMETHODIMP OnPushContext(ITfContext*) override { return S_OK; }
    STDMETHODIMP OnPopContext(ITfContext* context) override;
    STDMETHODIMP OnSetThreadFocus() override;
    STDMETHODIMP OnKillThreadFocus() override;
    STDMETHODIMP OnEndEdit(ITfContext* context,TfEditCookie cookie,ITfEditRecord* record) override;
    STDMETHODIMP OnLayoutChange(ITfContext* context,TfLayoutCode code,ITfContextView*) override;
    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** values) override;
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid,ITfDisplayAttributeInfo** value) override;
    HRESULT candidateMenu(POINT point,HWND window);
    HRESULT candidateWheel(int delta);
    HRESULT candidateCycle();
    HRESULT choose(const std::shared_ptr<Context>& state,UINT index,bool abort);
private:
    HRESULT key(ITfContext*,WPARAM,LPARAM,BOOL*,bool down,bool test);
    std::shared_ptr<Context> state(ITfContext*,bool create);
    HRESULT edit(const std::shared_ptr<Context>&,DWORD flags,std::function<HRESULT(TfEditCookie)> fn);
    HRESULT apply(const std::shared_ptr<Context>&,Engine next,KeyResult result,TfEditCookie cookie);
    HRESULT end(const std::shared_ptr<Context>&,TfEditCookie cookie);
    void updateUI(const std::shared_ptr<Context>&,TfEditCookie cookie,CandidateUpdate update=CandidateUpdate::Content);
    void hideUI();
    void forget(const std::shared_ptr<Context>&);
    void refreshFocus(ITfContext*);
    void modeChanged(bool chinese);
    void publishMode(const std::shared_ptr<Context>&);
    void reloadSettings();
    bool userDataRootAvailable() const;
    void pollDataChanges();
    void synchronizeEngine(Engine&);
    void refreshSentenceResources(std::u16string_view settings);
    void queueSentence(const std::shared_ptr<Context>&);
    void pollSentence();
    void completeSentenceNow(const std::shared_ptr<Context>&,Engine&);
    void reloadSchema(std::u16string name);
    struct PreparedSchema {
        std::u16string name;
        std::unique_ptr<UserStore> store;
        std::shared_ptr<const Lexicon> lexicon;
    };
    PreparedSchema prepareSchema(std::u16string name);
    void publishSchema(PreparedSchema prepared);
    void report(const char* message) const;
    LONG refs_=1;
    ComPtr<ITfThreadMgr> manager_;
    ComPtr<ITfSource> source_;
    ComPtr<ITfUIElementMgr> uiManager_;
    ComPtr<CandidateUI> ui_;
    DWORD uiId_=TF_INVALID_UIELEMENTID;
    DWORD mgrCookie_=TF_INVALID_COOKIE,focusCookie_=TF_INVALID_COOKIE;
    TfClientId client_=TF_CLIENTID_NULL;
    TfGuidAtom attribute_=TF_INVALID_GUIDATOM;
    bool active_=false,foreground_=true,secure_=false;
    ModeCompartments modes_;
    ComPtr<LanguageBar> languageBar_;
    bool horizontalCode_=false,verticalCode_=false;
    bool chinese_=true;
    std::uint64_t modeRevision_=0;
    ComPtr<ITfContext> focused_;
    std::shared_ptr<const Lexicon> lexicon_;
    std::unique_ptr<UserStore> store_;
    std::shared_ptr<AddWordUI> addWordUI_;
    std::shared_ptr<ManualTimer> dataTimer_;
    DirectoryChanges dataChanges_;
    ULONGLONG nextDataWatch_=0;
    bool dataDirty_=false,refreshingData_=false;
    unsigned keyDepth_=0;
    std::unique_ptr<SentenceWorker> sentenceWorker_;
    std::shared_ptr<ManualTimer> sentenceTimer_;
    std::shared_ptr<const SentenceResources> sentenceResources_;
    std::shared_ptr<const Lexicon> sentenceRequestedSource_,sentenceLoadedSource_;
    std::u16string sentenceSignature_;
    SentenceSettings sentenceSettings_;
    std::uint64_t sentenceRevision_=0;
    std::uint64_t sentenceLoadedRevision_=0;
    Config config_;
    CandidateStyle candidateStyle_;
    std::shared_ptr<PrivateFonts> fonts_;
    std::filesystem::path fontDirectory_;
    std::filesystem::path selectionPath_;
    std::filesystem::path settingsPath_;
    std::filesystem::path dictionaryPath_,userRoot_;
    std::u16string schema_;
    std::map<IUnknown*,std::shared_ptr<Context>> contexts_;
};
}
