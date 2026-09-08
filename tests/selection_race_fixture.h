#include <wrl.h>
namespace selection_race_fixture {
class SelectionRecord final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,ITfEditRecord> {
public:
    STDMETHODIMP GetSelectionStatus(BOOL* changed) override {*changed=TRUE;return S_OK;}
    STDMETHODIMP GetTextAndPropertyUpdates(DWORD,const GUID**,ULONG,IEnumTfRanges**) override {return E_NOTIMPL;}
};
class ReadSession final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,ITfEditSession> {
public:
    std::function<HRESULT(TfEditCookie)> action;
    STDMETHODIMP DoEditSession(TfEditCookie cookie) override {return action(cookie);}
};
inline void run(ITfTextInputProcessorEx* service,TfClientId client,Document& doc,Document& other,UISink* ui) {
    ComPtr<ITfKeyEventSink> keys;check(service->QueryInterface(IID_PPV_ARGS(&keys)));
    ComPtr<ITfThreadMgrEventSink> focus;check(service->QueryInterface(IID_PPV_ARGS(&focus)));
    ComPtr<ITfTextEditSink> edits;check(service->QueryInterface(IID_PPV_ARGS(&edits)));
    check(focus->OnSetFocus(doc.manager.Get(),nullptr));
    struct Keyboard {BYTE saved[256];Keyboard(){require(GetKeyboardState(saved)!=FALSE,"Save keyboard state");BYTE empty[256]{};SetKeyboardState(empty);}~Keyboard(){SetKeyboardState(saved);}} keyboard;
    auto tapTo=[&](Document& target,WPARAM vk) {
        BOOL eaten=FALSE;check(keys->OnTestKeyDown(target.context.Get(),vk,1,&eaten));require(eaten,"Selection fixture preview");
        check(keys->OnKeyDown(target.context.Get(),vk,1,&eaten));require(eaten,"Selection fixture dispatch");
        check(keys->OnTestKeyUp(target.context.Get(),vk,1,&eaten));if(eaten)check(keys->OnKeyUp(target.context.Get(),vk,1,&eaten));
        pump();
    };
    auto tap=[&](WPARAM vk){tapTo(doc,vk);};
    tap('A');tap('B');tap(VK_SPACE);tap('A');
    require(doc.store->text==L"交a","Selection fixture initial composition");
    const auto ends=doc.store->compositionEnds;
    const auto starts=doc.store->compositionStarts;
    require(starts==ends+1,"Selection fixture requires one active composition");
    auto queuedSelection=[&](bool returnInside,std::function<void()> beforeGrant={}) {
    const auto caret=static_cast<LONG>(doc.store->text.size());
    doc.store->selection={0,0,{TS_AE_END,FALSE}};
    auto record=Microsoft::WRL::Make<SelectionRecord>();
    auto session=Microsoft::WRL::Make<ReadSession>();
    session->action=[&](TfEditCookie cookie) {
        doc.store->deferLocks=true;
        return edits->OnEndEdit(doc.context.Get(),cookie,record.Get());
    };
    HRESULT inner=E_FAIL;
    check(doc.context->RequestEditSession(client,session.Get(),TF_ES_SYNC|TF_ES_READ,&inner));check(inner);
    const auto deadline=GetTickCount64()+2000;
    while(!doc.store->pendingLock && GetTickCount64()<deadline){pump();Sleep(5);}
    require(doc.store->pendingLock!=0,"Selection termination did not queue an edit lock");
    if(returnInside)doc.store->selection={caret,caret,{TS_AE_END,FALSE}};
    if(beforeGrant)beforeGrant();
    doc.store->deferLocks=false;check(doc.store->grantPendingLock());pump();
    };
    queuedSelection(true);
    require(doc.store->compositionEnds==ends,"Stale outside-selection request ended the returned composition");
    tap('B');tap(VK_SPACE);
    require(doc.store->text==L"交交","Returned composition did not continue normally");
    tap('A');
    const auto beforeOutside=doc.store->compositionEnds;
    queuedSelection(false);
    require(doc.store->compositionEnds==beforeOutside+1,"Persistently outside selection did not terminate composition");
    require(doc.store->text==L"交交a","Outside termination changed document text");
    const auto caret=static_cast<LONG>(doc.store->text.size());
    doc.store->selection={caret,caret,{TS_AE_END,FALSE}};tap('A');
    DWORD otherUi=TF_INVALID_UIELEMENTID;
    queuedSelection(false,[&] {
        check(focus->OnSetFocus(other.manager.Get(),doc.manager.Get()));
        tapTo(other,'A');otherUi=ui->id;
        require(otherUi!=TF_INVALID_UIELEMENTID,"Other context did not show candidates");
    });
    require(ui->id==otherUi,"Old context termination hid the new context candidate UI");
    tapTo(other,'B');tapTo(other,VK_SPACE);
    require(other.store->text==L"交","Other context could not continue input");
}
}
