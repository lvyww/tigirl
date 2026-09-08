#define NOMINMAX
#include "AddWordUI.h"
#include "../Text.h"
#include <algorithm>
#include <stdexcept>
#include <imm.h>
#include <commctrl.h>
void DllAddRef();
void DllRelease();
namespace tiger::tsf {
namespace {
constexpr UINT Open=WM_APP+11;
constexpr int Word=101,Code=102,Status=103,More=104,Less=105,Keep=106;
LRESULT CALLBACK wordEdit(HWND window,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR) {
    auto result=DefSubclassProc(window,message,w,l);
    if(message==WM_GETDLGCODE && w==VK_TAB) result|=DLGC_WANTTAB;
    return result;
}
const wchar_t* wide(const std::u16string& s) { return reinterpret_cast<const wchar_t*>(s.c_str()); }
std::u16string read(HWND dialog,int id) {
    auto control=GetDlgItem(dialog,id); auto length=GetWindowTextLengthW(control);
    std::wstring text(static_cast<std::size_t>(length)+1,L'\0');
    length=GetWindowTextW(control,text.data(),static_cast<int>(text.size()));
    return std::u16string(reinterpret_cast<const char16_t*>(text.data()),static_cast<std::size_t>(length));
}
std::vector<WORD> layout() {
    std::vector<WORD> data;
    auto dword=[&](DWORD n) { data.push_back(static_cast<WORD>(n)); data.push_back(static_cast<WORD>(n>>16)); };
    auto text=[&](const wchar_t* s) { while(*s) data.push_back(static_cast<WORD>(*s++)); data.push_back(0); };
    dword(WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER|DS_SETFONT); dword(WS_EX_TOOLWINDOW);
    for(int n:{0,0,0,260,160}) data.push_back(static_cast<WORD>(n));
    data.push_back(0); data.push_back(0); text(L"原生虎码加词"); data.push_back(10); text(L"Microsoft YaHei UI");
    return data;
}
}
AddWordUI::AddWordUI(HINSTANCE module,UserStore store,std::shared_ptr<const Dictionary> dictionary,
    std::function<void(std::shared_ptr<const Lexicon>)> saved)
    :module_(module),store_(std::move(store)),dictionary_(std::move(dictionary)),saved_(std::move(saved)) {
    className_=L"NativeTiger.AddWord."+std::to_wstring(reinterpret_cast<std::uintptr_t>(this));
    WNDCLASSW type{}; type.hInstance=module_; type.lpfnWndProc=dispatch; type.lpszClassName=className_.c_str();
    if(!RegisterClassW(&type)) throw std::runtime_error("Cannot register add-word dispatcher");
    queue_=CreateWindowExW(0,className_.c_str(),L"",0,0,0,0,0,HWND_MESSAGE,nullptr,module_,this);
    if(!queue_) { UnregisterClassW(className_.c_str(),module_); throw std::runtime_error("Cannot create add-word dispatcher"); }
    DllAddRef();
}
std::shared_ptr<AddWordUI> AddWordUI::create(HINSTANCE module,UserStore store,std::shared_ptr<const Dictionary> dictionary,
    std::function<void(std::shared_ptr<const Lexicon>)> saved) {
    return std::shared_ptr<AddWordUI>(new AddWordUI(module,std::move(store),std::move(dictionary),std::move(saved)));
}
AddWordUI::~AddWordUI() { close(); if(font_) DeleteObject(font_); UnregisterClassW(className_.c_str(),module_); DllRelease(); }
void AddWordUI::close() {
    closed_=true; pending_=false; saved_={};
    if(dialog_) EndDialog(dialog_,IDCANCEL);
    if(queue_) { DestroyWindow(queue_); queue_=nullptr; }
}
bool AddWordUI::queueRequest() {
    if(closed_) return false;
    if(dialog_) { SetForegroundWindow(dialog_); return false; }
    if(pending_) return false;
    pending_=true;
    if(!PostMessageW(queue_,Open,0,0)) { pending_=false; throw std::runtime_error("Cannot queue add-word dialog"); }
    return true;
}
void AddWordUI::request(std::u16string historyText) {
    if(queueRequest()) { pendingHistory_=std::move(historyText); pendingElements_.clear(); }
}
void AddWordUI::request(std::vector<std::u16string> history) {
    if(queueRequest()) { pendingElements_=std::move(history); pendingHistory_.clear(); }
}
LRESULT CALLBACK AddWordUI::dispatch(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_NCCREATE) SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
    auto self=reinterpret_cast<AddWordUI*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(self && message==Open) {
        try {
            auto lifetime=self->shared_from_this();
            if(!self->pending_ || self->closed_) return 0;
            self->pending_=false;
            self->history_=self->pendingElements_.empty()?wordTextElements(self->pendingHistory_):std::move(self->pendingElements_);
            self->pendingHistory_.clear(); self->pendingElements_.clear();
            // Original GetSendHistoryCount/GetLastCi expose at most 20 text
            // elements to the add-word window, even when the engine has more.
            if(self->history_.size()>20) self->history_.erase(self->history_.begin(),self->history_.end()-20);
            self->historyLength_=std::min<std::size_t>(2,self->history_.size());
            auto data=layout();
            // No host owner: the original add-word window does not disable the
            // application. DialogBox still runs keyboard navigation for this UI.
            DialogBoxIndirectParamW(self->module_,reinterpret_cast<const DLGTEMPLATE*>(data.data()),nullptr,dialog,reinterpret_cast<LPARAM>(self));
            self->dialog_=nullptr;
            if(self->font_) { DeleteObject(self->font_); self->font_=nullptr; }
        } catch(...) { MessageBeep(MB_ICONERROR); }
        return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
INT_PTR CALLBACK AddWordUI::dialog(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto self=reinterpret_cast<AddWordUI*>(GetWindowLongPtrW(window,DWLP_USER));
    try {
        if(message==WM_INITDIALOG) {
            self=reinterpret_cast<AddWordUI*>(l); self->dialog_=window;
            SetWindowLongPtrW(window,DWLP_USER,reinterpret_cast<LONG_PTR>(self)); self->initialize(); return FALSE;
        }
        if(!self) return FALSE;
        if(message==WM_CLOSE) { EndDialog(window,IDCANCEL); return TRUE; }
        if(message==WM_COMMAND) {
            const auto id=LOWORD(w),event=HIWORD(w);
            if(id==Word && event==EN_CHANGE) self->changed();
            else if(id==IDOK) self->save(false);
            else if(id==Keep) self->save(true);
            else if(id==IDCANCEL) EndDialog(window,IDCANCEL);
            else if(id==More) self->history(1);
            else if(id==Less) self->history(-1);
            return TRUE;
        }
    } catch(const std::exception& e) {
        if(self && self->dialog_) SetDlgItemTextW(window,Status,wide(utf16(e.what())));
    } catch(...) { if(self && self->dialog_) SetDlgItemTextW(window,Status,L"操作失败，请重试。"); }
    return FALSE;
}
void AddWordUI::initialize() {
    const auto dpi=GetDpiForWindow(dialog_);
    auto scale=[&](int value) { return MulDiv(value,static_cast<int>(dpi),96); };
    RECT frame{0,0,scale(260),scale(160)},previous{};
    if(!AdjustWindowRectExForDpi(&frame,static_cast<DWORD>(GetWindowLongPtrW(dialog_,GWL_STYLE)),FALSE,
        static_cast<DWORD>(GetWindowLongPtrW(dialog_,GWL_EXSTYLE)),dpi)) throw std::runtime_error("Cannot size add-word dialog");
    GetWindowRect(dialog_,&previous);
    const int frameWidth=frame.right-frame.left,frameHeight=frame.bottom-frame.top;
    SetWindowPos(dialog_,nullptr,(previous.left+previous.right-frameWidth)/2,(previous.top+previous.bottom-frameHeight)/2,frameWidth,frameHeight,SWP_NOZORDER|SWP_NOACTIVATE);
    font_=CreateFontW(-MulDiv(10,static_cast<int>(dpi),72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    if(!font_) throw std::runtime_error("Cannot create add-word font");
    auto control=[&](int id,const wchar_t* cls,const wchar_t* title,DWORD style,int x,int y,int width,int height) {
        RECT bounds{scale(x),scale(y),scale(x+width),scale(y+height)};
        auto window=CreateWindowExW(cls==std::wstring(L"EDIT")?WS_EX_CLIENTEDGE:0,cls,title,WS_CHILD|WS_VISIBLE|style,
            bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,dialog_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),module_,nullptr);
        if(!window) throw std::runtime_error("Cannot create add-word control");
        SendMessageW(window,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE); return window;
    };
    control(0,L"STATIC",L"词条",0,10,15,28,14);
    control(More,L"BUTTON",L"<",WS_TABSTOP,40,15,18,20);
    auto word=control(Word,L"EDIT",L"",WS_TABSTOP|ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL|WS_VSCROLL,60,10,170,42);
    if(!SetWindowSubclass(word,wordEdit,1,0)) throw std::runtime_error("Cannot enable word editing keys");
    SendMessageW(word,EM_SETLIMITTEXT,1024*1024,0);
    control(Less,L"BUTTON",L">",WS_TABSTOP,232,15,18,20);
    control(0,L"STATIC",L"编码",0,10,61,28,14);
    auto code=control(Code,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,60,57,170,19);
    ImmAssociateContextEx(code,nullptr,0);
    control(Status,L"STATIC",L"",SS_CENTER,10,85,240,28);
    control(IDOK,L"BUTTON",L"添加",WS_TABSTOP|BS_DEFPUSHBUTTON,20,125,60,20);
    control(Keep,L"BUTTON",L"继续添加",WS_TABSTOP,96,125,68,20);
    control(IDCANCEL,L"BUTTON",L"取消",WS_TABSTOP,180,125,60,20);
    history(0);
}
void AddWordUI::history(int change) {
    if(change>0 && historyLength_<history_.size()) ++historyLength_;
    if(change<0 && historyLength_) --historyLength_;
    std::u16string text;
    for(auto i=history_.size()-historyLength_;i<history_.size();++i) text+=history_[i];
    SetDlgItemTextW(dialog_,Word,wide(text)); changed(); SetFocus(GetDlgItem(dialog_,Word));
    SendDlgItemMessageW(dialog_,Word,EM_SETSEL,0,-1);
}
void AddWordUI::changed() { SetDlgItemTextW(dialog_,Code,wide(constructWordCode(*dictionary_,read(dialog_,Word)))); }
void AddWordUI::save(bool keep) {
    if(closed_) return;
    const auto text=read(dialog_,Word);
    try {
        const auto code=normalizeAddedCode(read(dialog_,Code));
        if(code.empty()) {
            SetDlgItemTextW(dialog_,Status,L"编码为空。");
            SetFocus(GetDlgItem(dialog_,Code));
            return;
        }
        if(text.empty()) {
            SetDlgItemTextW(dialog_,Status,L"词条为空。");
            SetFocus(GetDlgItem(dialog_,Word));
            return;
        }
        auto change=prepareAddedWord(code,text);
        auto lexicon=store_.commit({change});
        if(saved_) saved_(std::move(lexicon));
        if(!keep) { EndDialog(dialog_,IDOK); return; }
        SetDlgItemTextW(dialog_,Status,wide(u"已添加："+text));
        historyLength_=0; SetDlgItemTextW(dialog_,Word,L""); SetDlgItemTextW(dialog_,Code,L"");
        SetFocus(GetDlgItem(dialog_,Word));
    } catch(const std::exception& e) { SetDlgItemTextW(dialog_,Status,wide(u"添加失败："+utf16(e.what()))); }
}
}
