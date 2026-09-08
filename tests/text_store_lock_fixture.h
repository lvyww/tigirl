// Exercise the test application's lock contract without loading an IME or UI.
class LockFixtureSink final:public ITextStoreACPSink {
    LONG refs_=1;
public:
    TextStore* store=nullptr;
    unsigned grants=0;
    bool valid=true;
    STDMETHODIMP QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(iid!=IID_IUnknown && iid!=IID_ITextStoreACPSink)return E_NOINTERFACE;
        *out=static_cast<ITextStoreACPSink*>(this);AddRef();return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override{return ++refs_;}
    STDMETHODIMP_(ULONG) Release() override{const auto n=--refs_;if(!n)delete this;return n;}
    STDMETHODIMP OnTextChange(DWORD,const TS_TEXTCHANGE*) override{return S_OK;}
    STDMETHODIMP OnSelectionChange() override{return S_OK;}
    STDMETHODIMP OnLayoutChange(TsLayoutCode,TsViewCookie) override{return S_OK;}
    STDMETHODIMP OnStatusChange(DWORD) override{return S_OK;}
    STDMETHODIMP OnAttrsChange(LONG,LONG,ULONG,const TS_ATTRID*) override{return S_OK;}
    STDMETHODIMP OnStartEditTransaction() override{return S_OK;}
    STDMETHODIMP OnEndEditTransaction() override{return S_OK;}
    STDMETHODIMP OnLockGranted(DWORD flags) override {
        ++grants;
        if(grants==1) {
            valid=valid && (flags&TS_LF_READWRITE)==TS_LF_READ;
            HRESULT session=E_FAIL;
            valid=valid && store->RequestLock(TS_LF_READ,&session)==S_OK && session==TS_S_ASYNC;
            valid=valid && store->RequestLock(TS_LF_READWRITE,&session)==S_OK && session==TS_S_ASYNC;
            valid=valid && store->RequestLock(TS_LF_READWRITE|TS_LF_SYNC,&session)==S_OK && session==TS_E_SYNCHRONOUS;
            valid=valid && grants==1;
        }else valid=valid && grants==2 && flags==TS_LF_READWRITE && store->lock==TS_LF_READWRITE;
        return S_OK;
    }
};
static void text_store_lock_fixture() {
    ComPtr<TextStore> store;store.Attach(new TextStore(nullptr));
    ComPtr<LockFixtureSink> sink;sink.Attach(new LockFixtureSink);sink->store=store.Get();
    check(store->AdviseSink(IID_ITextStoreACPSink,sink.Get(),0));
    HRESULT session=E_FAIL;check(store->RequestLock(TS_LF_READ|TS_LF_SYNC,&session));
    require(session==S_OK && sink->valid && sink->grants==2 && store->lock==0,"Text store lost asynchronous lock upgrade");
    check(store->UnadviseSink(sink.Get()));
    std::cout<<"{\"status\":\"passed\",\"async_lock_coalescing\":true,\"failed_sync_preserves_pending\":true,\"grants\":2,\"ime_loaded\":false}\n";
}
