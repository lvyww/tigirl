#define NOMINMAX
#include "SelectionSettings.h"
#include "SelectionKeys.h"
#include "ConfigStore.h"
#include "Text.h"
#include <algorithm>
#include <stdexcept>
#include <fstream>
namespace {
constexpr int Save=401,Cancel=402,Defaults=403,Notice=404;
struct Dialog {
    HWND window=nullptr;HFONT font=nullptr;std::filesystem::path path;
    tiger::SelectionKeys initial,current;int capture=-1;WPARAM consumed=0;bool saved=false;
    bool recovering=false;std::string damagedBytes;
    struct Control{HWND handle;int x,y,w,h;};std::vector<Control> controls;
    ~Dialog(){if(IsWindow(window))DestroyWindow(window);if(font)DeleteObject(font);}
    HWND item(int id){return GetDlgItem(window,id);}
    void control(int id,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
        auto child=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        if(!child)throw std::runtime_error("Cannot create selection-key control");controls.push_back({child,x,y,w,h});
    }
    void refresh() {
        for(int i=0;i<10;++i) {
            const auto previous=SendMessageW(item(100+i),CB_GETCURSEL,0,0);
            SendMessageW(item(100+i),CB_RESETCONTENT,0,0);
            for(int vk:current.bindings[i]) {
                wchar_t name[128]{};
                if(vk>0 && vk<256) {
                    const auto scan=MapVirtualKeyW(vk,MAPVK_VK_TO_VSC_EX);
                    LPARAM flags=static_cast<LPARAM>((scan&255)<<16);if(scan&0xff00)flags|=1<<24;
                    GetKeyNameTextW(static_cast<LONG>(flags),name,128);
                }
                const auto text=*name?std::wstring(name):L"键值 "+std::to_wstring(vk);
                SendMessageW(item(100+i),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));
            }
            const auto count=current.bindings[i].size();
            if(!count)SendMessageW(item(100+i),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"未绑定"));
            SendMessageW(item(100+i),CB_SETCURSEL,count?std::clamp<LRESULT>(previous,0,static_cast<LRESULT>(count)-1):0,0);
            EnableWindow(item(500+i),count!=0);
            SetWindowTextW(item(200+i),capture==i?L"取消录入":L"添加按键");
        }
        SetWindowTextW(item(Notice),capture>=0?L"请按下要添加的单个按键（包括修饰键）；点击“取消录入”可退出。":recovering?
            L"原文件编码或格式错误，当前显示默认按键，可修改后保存。保存时会在同目录保留 .invalid. 备份；取消不修改原文件。":
            L"每个候选可绑定多个按键；重复绑定时，靠前的候选优先。\r\n保存后自动生效。");
    }
    void scale(UINT dpi) {
        auto px=[&](int n){return MulDiv(n,static_cast<int>(dpi),96);};
        auto next=CreateFontW(-px(16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,0,L"Segoe UI");
        if(!next)throw std::runtime_error("Cannot create selection-key font");
        for(auto c:controls){SendMessageW(c.handle,WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);MoveWindow(c.handle,px(c.x),px(c.y),px(c.w),px(c.h),TRUE);}
        for(int i=0;i<10;++i)SendMessageW(item(100+i),CB_SETDROPPEDWIDTH,px(300),0);
        if(font)DeleteObject(font);font=next;
        RECT bounds{0,0,px(620),px(440)};
        if(!AdjustWindowRectExForDpi(&bounds,static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE)),FALSE,WS_EX_CONTROLPARENT,GetDpiForWindow(window)))throw std::runtime_error("Cannot size selection-key window");
        SetWindowPos(window,nullptr,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
    void create() {
        for(int i=0;i<10;++i) {
            control(0,L"STATIC",(L"第 "+std::to_wstring(i+1)+L" 候选").c_str(),0,18,20+i*31,85,25);
            control(100+i,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,108,17+i*31,160,180);
            control(200+i,L"BUTTON",L"添加按键",WS_TABSTOP,278,17+i*31,80,27);
            control(500+i,L"BUTTON",L"删除按键",WS_TABSTOP,368,17+i*31,80,27);
            control(600+i,L"BUTTON",L"本行默认",WS_TABSTOP,458,17+i*31,80,27);
            control(300+i,L"BUTTON",L"清空",WS_TABSTOP,548,17+i*31,60,27);
        }
        control(Notice,L"STATIC",L"",0,18,335,582,48);
        control(Defaults,L"BUTTON",L"恢复默认",WS_TABSTOP,18,395,120,30);
        control(Save,L"BUTTON",L"保存",BS_DEFPUSHBUTTON|WS_TABSTOP,390,395,95,30);
        control(Cancel,L"BUTTON",L"取消",WS_TABSTOP,505,395,95,30);
        refresh();scale(GetDpiForWindow(window));
    }
    void key(WPARAM key,LPARAM flags) {
        if(capture<0)return;
        int vk=static_cast<int>(key);
        if(vk==VK_SHIFT)vk=static_cast<int>(MapVirtualKeyW((flags>>16)&255,MAPVK_VSC_TO_VK_EX));
        if(vk==VK_CONTROL)vk=(flags&(1<<24))?VK_RCONTROL:VK_LCONTROL;
        if(vk==VK_MENU)vk=(flags&(1<<24))?VK_RMENU:VK_LMENU;
        if(vk<=0 || vk>255)return;
        auto& keys=current.bindings[capture];if(std::find(keys.begin(),keys.end(),vk)==keys.end())keys.push_back(vk);
        consumed=key;capture=-1;refresh();
    }
    void action(int id) {
        if(id>=200 && id<210){capture=capture==id-200?-1:id-200;refresh();SetFocus(window);return;}
        if(id>=300 && id<310){current.bindings[id-300].clear();capture=-1;refresh();return;}
        if(id>=500 && id<510) {
            const auto row=id-500,selected=static_cast<int>(SendMessageW(item(100+row),CB_GETCURSEL,0,0));
            auto& keys=current.bindings[row];
            if(selected>=0 && static_cast<std::size_t>(selected)<keys.size())keys.erase(keys.begin()+selected);
            capture=-1;refresh();return;
        }
        if(id>=600 && id<610){current.bindings[id-600]=tiger::SelectionKeys{}.bindings[id-600];capture=-1;refresh();return;}
        if(id==Defaults){current=tiger::SelectionKeys{};capture=-1;refresh();return;}
        if(id==Save || id==IDOK){
            if(recovering)tiger::repairSelectionKeys(path,damagedBytes,current);
            else tiger::updateSelectionKeys(path,initial,current);
            saved=true;DestroyWindow(window);
        }
        if(id==Cancel || id==IDCANCEL)DestroyWindow(window);
    }
};
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto self=reinterpret_cast<Dialog*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){self=static_cast<Dialog*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(window,message,w,l);
    try {
        if((message==WM_KEYUP || message==WM_SYSKEYUP) && self->consumed==w){self->consumed=0;return 0;}
        if((message==WM_KEYDOWN || message==WM_SYSKEYDOWN) && self->consumed==w)return 0;
        if((message==WM_KEYDOWN || message==WM_SYSKEYDOWN) && self->capture>=0){self->key(w,l);return 0;}
        if(message==WM_COMMAND){self->action(LOWORD(w));return 0;}
        if(message==WM_CLOSE){DestroyWindow(window);return 0;}
        if(message==WM_DPICHANGED){const auto r=reinterpret_cast<RECT*>(l);SetWindowPos(window,nullptr,r->left,r->top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);self->scale(HIWORD(w));return 0;}
    }catch(const std::exception& error){
        std::u16string detail=u"请检查文件格式和写入权限。";
        try {detail=tiger::utf16(error.what());}catch(...) {}
        const auto failureMessage=L"保存失败，修改仍保留。"+std::wstring(reinterpret_cast<const wchar_t*>(detail.data()),detail.size());
        SetWindowTextW(self->item(Notice),failureMessage.c_str());
    }
    return DefWindowProcW(window,message,w,l);
}
}
bool showSelectionSettings(HWND owner,const std::filesystem::path& path,int testMode) {
    Dialog dialog;dialog.path=path;
    const auto bytes=tiger::readConfigurationBytes(path);std::u16string error,text;
    try{text=tiger::decodeUnicodeText(bytes);}catch(const std::runtime_error&){dialog.recovering=true;}catch(const std::invalid_argument&){dialog.recovering=true;}
    if(dialog.recovering || !tiger::SelectionKeys::parse(text,dialog.initial,error)) {dialog.recovering=true;dialog.damagedBytes=bytes;}
    dialog.current=dialog.initial;
    WNDCLASSW type{};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=procedure;type.lpszClassName=L"NativeTigerSelectionSettings";type.hIcon=LoadIconW(type.hInstance,MAKEINTRESOURCEW(12));type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassW(&type) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Cannot register selection-key window");
    if(!CreateWindowExW(WS_EX_CONTROLPARENT,type.lpszClassName,L"虎娘 · 选重键",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,640,460,owner,nullptr,type.hInstance,&dialog))throw std::runtime_error("Cannot create selection-key window");
    dialog.create();
    if(testMode) {
        const auto before=tiger::readConfigurationBytes(path);
        for(UINT dpi:{96u,144u,192u}) {
            dialog.scale(dpi);RECT client{};GetClientRect(dialog.window,&client);
            for(auto c:dialog.controls){RECT r{};GetWindowRect(c.handle,&r);MapWindowPoints(nullptr,dialog.window,reinterpret_cast<POINT*>(&r),2);if(r.left<0 || r.top<0 || r.right>client.right || r.bottom>client.bottom)throw std::runtime_error("Selection control exceeds client bounds");}
        }
        if(testMode==5) {
            if(!dialog.recovering)throw std::runtime_error("Conflict fixture must be malformed");
            tiger::updateConfigurationValues(path,{{u"并发修改",u"保留"}});
            const auto concurrent=tiger::readConfiguration(path);
            SendMessageW(dialog.window,WM_COMMAND,Save,0);
            if(dialog.saved || !IsWindow(dialog.window) || tiger::readConfiguration(path)!=concurrent)
                throw std::runtime_error("Repair overwrote concurrent changes");
            SendMessageW(dialog.window,WM_COMMAND,Cancel,0);return false;
        }
        if(testMode==6) {
            if(!dialog.recovering)throw std::runtime_error("Encoding conflict fixture must be malformed");
            {std::ofstream changed(path,std::ios::binary|std::ios::app);changed.put('\0');if(!changed.good())throw std::runtime_error("Cannot change encoding fixture");}
            const auto concurrent=tiger::readConfigurationBytes(path);
            SendMessageW(dialog.window,WM_COMMAND,Save,0);
            if(dialog.saved || !IsWindow(dialog.window) || tiger::readConfigurationBytes(path)!=concurrent)
                throw std::runtime_error("Encoding repair overwrote concurrent bytes");
            SendMessageW(dialog.window,WM_COMMAND,Cancel,0);return false;
        }
        if(testMode==3 || testMode==4) {
            if(testMode==4 && !dialog.recovering)throw std::runtime_error("Recovery fixture must be malformed");
            SendMessageW(dialog.window,WM_COMMAND,Defaults,0);SendMessageW(dialog.window,WM_COMMAND,Save,0);
            if(!dialog.saved || tiger::SelectionKeys::load(path).bindings!=tiger::SelectionKeys{}.bindings)
                throw std::runtime_error("Default selection bindings were not persisted");
            return true;
        }
        SendMessageW(dialog.window,WM_COMMAND,200,0);SendMessageW(dialog.window,WM_KEYDOWN,testMode==2?'R':'Q',1);
        SendMessageW(dialog.window,WM_KEYUP,testMode==2?'R':'Q',1);
        if(testMode==1) {
            const auto otherRows=dialog.current.bindings;
            SendMessageW(dialog.item(100),CB_SETCURSEL,0,0);
            SendMessageW(dialog.window,WM_COMMAND,500,0);
            if(dialog.current.bindings[0]!=std::vector<int>{'Q'} || SendMessageW(dialog.item(100),CB_GETCOUNT,0,0)!=1)
                throw std::runtime_error("Individual binding removal cleared the wrong keys");
            SendMessageW(dialog.window,WM_COMMAND,600,0);
            if(dialog.current.bindings[0]!=tiger::SelectionKeys{}.bindings[0])
                throw std::runtime_error("Per-row defaults did not restore numeric binding");
            for(int i=1;i<10;++i)if(dialog.current.bindings[i]!=otherRows[i])
                throw std::runtime_error("Per-row operation changed another candidate");
            SendMessageW(dialog.window,WM_COMMAND,200,0);
            SendMessageW(dialog.window,WM_KEYDOWN,'Q',1);SendMessageW(dialog.window,WM_KEYUP,'Q',1);
        }
        SendMessageW(dialog.window,WM_COMMAND,301,0);
        if(IsWindowEnabled(dialog.item(501)))throw std::runtime_error("Empty candidate still enables binding removal");
        SendMessageW(dialog.window,WM_COMMAND,501,0);
        if(!dialog.current.bindings[1].empty())throw std::runtime_error("Empty binding deletion changed the row");
        if(testMode==2){SendMessageW(dialog.window,WM_COMMAND,Cancel,0);if(tiger::readConfigurationBytes(path)!=before || dialog.saved)throw std::runtime_error("Selection cancel wrote file");return false;}
        auto concurrent=dialog.initial;concurrent.bindings[8]={VK_F9};
        tiger::updateSelectionKeys(path,dialog.initial,concurrent);
        SendMessageW(dialog.window,WM_COMMAND,Save,0);
        if(!dialog.saved)throw std::runtime_error("Selection save failed");
        auto saved=tiger::SelectionKeys::load(path);
        if(saved.bindings[0]!=dialog.current.bindings[0] || !saved.bindings[1].empty() || saved.bindings[8]!=concurrent.bindings[8])throw std::runtime_error("Selection row merge failed");
        return true;
    }
    const bool enabled=owner && IsWindowEnabled(owner);if(enabled)EnableWindow(owner,FALSE);
    ShowWindow(dialog.window,SW_SHOW);SetFocus(dialog.item(200));
    MSG message{};while(IsWindow(dialog.window)) {
        const auto result=GetMessageW(&message,nullptr,0,0);if(result<=0){if(!result)PostQuitMessage(static_cast<int>(message.wParam));break;}
        if((dialog.capture>=0 && (message.message==WM_KEYDOWN || message.message==WM_SYSKEYDOWN)) ||
           (dialog.consumed==message.wParam && (message.message==WM_KEYDOWN || message.message==WM_SYSKEYDOWN || message.message==WM_KEYUP || message.message==WM_SYSKEYUP))){SendMessageW(dialog.window,message.message,message.wParam,message.lParam);continue;}
        if(!IsDialogMessageW(dialog.window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
    }
    if(enabled){EnableWindow(owner,TRUE);SetActiveWindow(owner);}return dialog.saved;
}
