#pragma once
// Link-time stand-in for the TSF host, used only by candidate_ui_presentation_probe.
// No service activation, COM document editing, user data, registration or threads.
// CandidateUI/Engine/DirectWrite/Direct2D/GDI remain the production implementations.
// The two choice paths advance Engine and detach UI, as Service::hideUI does.
namespace candidate_probe {
inline tiger::tsf::CandidateUI* currentUI=nullptr;
inline std::u16string committed;
inline long dllRefs=0;
}
void DllAddRef() { ++candidate_probe::dllRefs; }
void DllRelease() { --candidate_probe::dllRefs; }
namespace Global {
HINSTANCE dllInstanceHandle=GetModuleHandleW(nullptr);
const GUID SampleIMEGuidCandUIElement={0x91d07347,0x1830,0x4f37,{0xad,0x5f,0x1c,0x14,0x19,0x56,0xb3,0x71}};
}
namespace tiger::tsf {
Service::Service()=default;
Service::~Service()=default;
ULONG Service::AddRef() { return InterlockedIncrement(&refs_); }
ULONG Service::Release() { const auto refs=InterlockedDecrement(&refs_);if(!refs)delete this;return refs; }
HRESULT Service::QueryInterface(REFIID iid,void** value) {
    if(!value)return E_POINTER;
    *value=nullptr;
    if(iid!=IID_IUnknown && iid!=IID_ITfTextInputProcessorEx)return E_NOINTERFACE;
    *value=static_cast<ITfTextInputProcessorEx*>(this);AddRef();return S_OK;
}
HRESULT Service::choose(const std::shared_ptr<Context>& state,UINT index,bool abort) {
    auto next=state->engine;
    if(abort)next.cancel();
    else {
        const auto result=next.selectCandidate(index);
        if(!result)return E_INVALIDARG;
        candidate_probe::committed+=result->commit;
    }
    state->engine=std::move(next);
    if(candidate_probe::currentUI)candidate_probe::currentUI->detach();
    return S_OK;
}
// These collaborators are never opened/created by this unactivated host.
ModeCompartments::~ModeCompartments()=default;
SentenceWorker::~SentenceWorker()=default;
ULONG LanguageBar::Release() { std::terminate(); }
#define UNUSED_HOST_METHOD(signature) HRESULT Service::signature { return E_NOTIMPL; }
UNUSED_HOST_METHOD(ActivateEx(ITfThreadMgr*,TfClientId,DWORD))
UNUSED_HOST_METHOD(Deactivate())
UNUSED_HOST_METHOD(OnSetFocus(BOOL))
UNUSED_HOST_METHOD(OnTestKeyDown(ITfContext*,WPARAM,LPARAM,BOOL*))
UNUSED_HOST_METHOD(OnKeyDown(ITfContext*,WPARAM,LPARAM,BOOL*))
UNUSED_HOST_METHOD(OnTestKeyUp(ITfContext*,WPARAM,LPARAM,BOOL*))
UNUSED_HOST_METHOD(OnKeyUp(ITfContext*,WPARAM,LPARAM,BOOL*))
UNUSED_HOST_METHOD(OnPreservedKey(ITfContext*,REFGUID,BOOL*))
UNUSED_HOST_METHOD(OnCompositionTerminated(TfEditCookie,ITfComposition*))
UNUSED_HOST_METHOD(OnUninitDocumentMgr(ITfDocumentMgr*))
UNUSED_HOST_METHOD(OnSetFocus(ITfDocumentMgr*,ITfDocumentMgr*))
UNUSED_HOST_METHOD(OnPopContext(ITfContext*))
UNUSED_HOST_METHOD(OnSetThreadFocus())
UNUSED_HOST_METHOD(OnKillThreadFocus())
UNUSED_HOST_METHOD(OnEndEdit(ITfContext*,TfEditCookie,ITfEditRecord*))
UNUSED_HOST_METHOD(OnLayoutChange(ITfContext*,TfLayoutCode,ITfContextView*))
UNUSED_HOST_METHOD(EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo**))
UNUSED_HOST_METHOD(GetDisplayAttributeInfo(REFGUID,ITfDisplayAttributeInfo**))
UNUSED_HOST_METHOD(candidateMenu(POINT,HWND))
UNUSED_HOST_METHOD(candidateWheel(int))
UNUSED_HOST_METHOD(candidateCycle())
#undef UNUSED_HOST_METHOD
}
