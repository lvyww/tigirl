#define NOMINMAX
#include "InputSettings.h"
#include "SelectionSettings.h"
#include "SelectionKeys.h"
#include "ConfigStore.h"
#include "Settings.h"
#include "CandidateTheme.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <locale>
#include <cmath>
#include <commctrl.h>
#include <iomanip>
namespace {
struct Flag {const char16_t* key;bool tiger::Config::*member;};
const Flag flags[]={
    {u"默认中文",&tiger::Config::defaultChinese},{u"shift切换中英文",&tiger::Config::shiftToggle},
    {u"Ctrl+空格切换中英文",&tiger::Config::ctrlSpaceToggle},{u"中文状态下使用英文标点",&tiger::Config::englishPunctuation},
    {u"/输出顿号",&tiger::Config::slashDunhao},{u"回车清屏",&tiger::Config::enterClear},
    {u"TAB清屏",&tiger::Config::tabClear},{u"空码自动清屏",&tiger::Config::clearOnNoCode},
    {u"最大码长无重自动上屏",&tiger::Config::maxCodeAutoCommit},{u"中英文不限长混合输入",&tiger::Config::mixedInput},
    {u"`键拼音反查",&tiger::Config::reverseLookup},{u"分号次选",&tiger::Config::semicolonSecond},
    {u"引号三选",&tiger::Config::quoteThird},{u"显示注释",&tiger::Config::showComment},{u"显示拆分",&tiger::Config::showSplit}};
constexpr int MaxCode=200,PageSize=201,Save=202,Cancel=203,Notice=204,PageKeys=205,InputPage=206,AppearancePage=207,FontName=208,FontSize=209,Theme=210,ShortcutPage=211,AddEnabled=212,AddShortcut=213,RecentEnabled=214,RecentShortcut=215,SelectionEditor=216;
struct StyleFlag {const char16_t* key;bool tiger::CandidateStyle::*member;};
const StyleFlag styleFlags[]={{u"竖排候选",&tiger::CandidateStyle::vertical},{u"显示候选序号",&tiger::CandidateStyle::showIndex},{u"候选窗显示编码",&tiger::CandidateStyle::showCode},{u"隐藏候选",&tiger::CandidateStyle::hideCandidates}};
constexpr wchar_t bundledFontLabel[]=L"内置：霞鹜文楷 GB 屏幕阅读版";
const char16_t* pageKeys[]={u"- =",u"[ ]",u"Shift Tab/Tab",u"PageUp/PageDown"};
const wchar_t* wide(const char16_t* s){return reinterpret_cast<const wchar_t*>(s);}
struct Dialog {
    HWND window=nullptr;HFONT font=nullptr;std::filesystem::path path;
    std::vector<std::u16string> themes;
    tiger::Config initial;tiger::CandidateStyle initialStyle;int buildingPage=-1;bool saved=false;std::wstring error;
    struct Control {HWND window;int x,y,w,h,page;};std::vector<Control> controls;
    ~Dialog(){if(IsWindow(window))DestroyWindow(window);if(font)DeleteObject(font);}
    HWND item(int id){return GetDlgItem(window,id);}
    void control(int id,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
        if(buildingPage>=0)y+=40;
        auto child=CreateWindowExW(type==std::wstring_view(L"EDIT")?WS_EX_CLIENTEDGE:0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        if(!child)throw std::runtime_error("Cannot create input settings control");controls.push_back({child,x,y,w,h,buildingPage});
    }
    void scale(UINT dpi) {
        auto px=[&](int n){return MulDiv(n,static_cast<int>(dpi),96);};
        auto next=CreateFontW(-px(16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,0,L"Segoe UI");
        if(!next)throw std::runtime_error("Cannot create input settings font");
        for(auto c:controls){SendMessageW(c.window,WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);MoveWindow(c.window,px(c.x),px(c.y),px(c.w),px(c.h),TRUE);}
        if(font)DeleteObject(font);font=next;
        RECT bounds{0,0,px(620),px(500)};
        if(!AdjustWindowRectExForDpi(&bounds,static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE)),FALSE,WS_EX_CONTROLPARENT,GetDpiForWindow(window)))throw std::runtime_error("Cannot size input settings window");
        SetWindowPos(window,nullptr,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
    void page(int selected) {
        for(auto c:controls)ShowWindow(c.window,c.page<0 || c.page==selected?SW_SHOW:SW_HIDE);
    }
    static std::wstring sizeText(double value) {
        std::wostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)<<value;return out.str();
    }
    std::u16string text(int id) {
        const auto control=item(id);const int length=GetWindowTextLengthW(control);
        if(length>256)throw std::runtime_error("Setting is too long");
        std::wstring value(static_cast<std::size_t>(length)+1,L'\0');
        value.resize(GetWindowTextW(control,value.data(),length+1));
        return {reinterpret_cast<const char16_t*>(value.data()),value.size()};
    }
    static WORD hotkey(tiger::ActionShortcut value) {
        return MAKEWORD(value.vk,(value.ctrl?HOTKEYF_CONTROL:0)|(value.alt?HOTKEYF_ALT:0)|(value.shift?HOTKEYF_SHIFT:0));
    }
    std::u16string shortcut(int id) {
        const auto value=static_cast<WORD>(SendMessageW(item(id),HKM_GETHOTKEY,0,0));
        const auto modifiers=HIBYTE(value);const auto vk=LOBYTE(value);
        if(!vk || !(modifiers&(HOTKEYF_CONTROL|HOTKEYF_ALT)))throw std::runtime_error("快捷键必须包含 Ctrl 或 Alt。");
        std::u16string result;
        if(modifiers&HOTKEYF_CONTROL)result+=u"Ctrl+";
        if(modifiers&HOTKEYF_ALT)result+=u"Alt+";
        if(modifiers&HOTKEYF_SHIFT)result+=u"Shift+";
        const char16_t digits[]=u"0123456789ABCDEF";
        result+=u"0X";result+=digits[vk>>4];result+=digits[vk&15];return result;
    }
    void create() {
        control(InputPage,L"BUTTON",L"输入行为",WS_TABSTOP,18,10,140,30);
        control(AppearancePage,L"BUTTON",L"候选外观",WS_TABSTOP,170,10,140,30);
        control(ShortcutPage,L"BUTTON",L"操作快捷键",WS_TABSTOP,322,10,140,30);
        buildingPage=0;
        for(int i=0;i<static_cast<int>(std::size(flags));++i) {
            control(100+i,L"BUTTON",wide(flags[i].key),BS_AUTOCHECKBOX|WS_TABSTOP,18+(i/8)*302,18+(i%8)*32,290,27);
            SendMessageW(item(100+i),BM_SETCHECK,initial.*flags[i].member?BST_CHECKED:BST_UNCHECKED,0);
        }
        control(0,L"STATIC",L"最大码长（1–16）",0,18,284,180,25);
        control(MaxCode,L"EDIT",std::to_wstring(initial.maxCodeLength).c_str(),ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,205,280,70,28);
        control(0,L"STATIC",L"每页候选（1–10）",0,320,284,180,25);
        control(PageSize,L"EDIT",std::to_wstring(initial.pageSize).c_str(),ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,520,280,70,28);
        control(0,L"STATIC",L"翻页键（上一页／下一页）",0,18,324,245,25);
        control(PageKeys,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,280,320,310,130);
        for(auto keys:pageKeys)SendMessageW(item(PageKeys),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(keys)));
        SendMessageW(item(PageKeys),CB_SETCURSEL,initial.pageKeys,0);
        buildingPage=1;
        control(0,L"STATIC",L"候选字体（也可输入已安装的字体名称）",0,18,20,570,25);
        const auto displayFont=initialStyle.font==tiger::CandidateStyle{}.font?std::wstring(bundledFontLabel):std::wstring(wide(initialStyle.font.c_str()));
        control(FontName,L"COMBOBOX",displayFont.c_str(),CBS_DROPDOWN|WS_TABSTOP|WS_VSCROLL,18,55,570,150);
        for(auto name:{bundledFontLabel,L"Microsoft YaHei UI",L"Segoe UI"})SendMessageW(item(FontName),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
        SetWindowTextW(item(FontName),displayFont.c_str());
        control(0,L"STATIC",L"字号（3–200，支持小数）",0,18,105,310,25);
        control(FontSize,L"EDIT",sizeText(initialStyle.fontSize).c_str(),ES_AUTOHSCROLL|WS_TABSTOP,340,101,130,28);
        for(int i=0;i<4;++i) {
            control(300+i,L"BUTTON",wide(styleFlags[i].key),BS_AUTOCHECKBOX|WS_TABSTOP,18,155+i*35,570,28);
            SendMessageW(item(300+i),BM_SETCHECK,initialStyle.*styleFlags[i].member?BST_CHECKED:BST_UNCHECKED,0);
        }
        control(0,L"STATIC",L"候选主题",0,18,319,170,25);
        control(Theme,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,205,315,385,220);
        int themeIndex=-1;
        for(auto name:tiger::candidateThemeNames){if(name==initialStyle.theme)themeIndex=static_cast<int>(themes.size());themes.emplace_back(name);}
        if(themeIndex<0){themeIndex=static_cast<int>(themes.size());themes.push_back(initialStyle.theme);}
        for(const auto& name:themes)SendMessageW(item(Theme),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(name.c_str())));
        SendMessageW(item(Theme),CB_SETCURSEL,themeIndex,0);
        buildingPage=2;
        control(AddEnabled,L"BUTTON",L"启用手动加词",BS_AUTOCHECKBOX|WS_TABSTOP,18,20,570,28);
        control(AddShortcut,HOTKEY_CLASSW,L"",WS_TABSTOP,18,60,570,32);
        control(RecentEnabled,L"BUTTON",L"启用最近方案切换",BS_AUTOCHECKBOX|WS_TABSTOP,18,125,570,28);
        control(RecentShortcut,HOTKEY_CLASSW,L"",WS_TABSTOP,18,165,570,32);
        control(0,L"STATIC",L"点击快捷键框后直接按键；组合键需包含 Ctrl 或 Alt。",0,18,220,570,50);
        control(SelectionEditor,L"BUTTON",L"编辑选重键…",WS_TABSTOP,18,290,220,32);
        control(0,L"STATIC",L"选重键在独立窗口中保存。",0,260,295,320,25);
        SendMessageW(item(AddEnabled),BM_SETCHECK,initial.addWordEnabled?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(item(RecentEnabled),BM_SETCHECK,initial.recentSchemaEnabled?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(item(AddShortcut),HKM_SETHOTKEY,hotkey(initial.addWordShortcut),0);
        SendMessageW(item(RecentShortcut),HKM_SETHOTKEY,hotkey(initial.recentSchemaShortcut),0);
        buildingPage=-1;
        control(Notice,L"STATIC",L"保存更改后自动更新设置，清除未完成编码，并应用默认中英文模式。",0,18,403,582,40);
        control(Save,L"BUTTON",L"保存",BS_DEFPUSHBUTTON|WS_TABSTOP,390,452,95,30);
        control(Cancel,L"BUTTON",L"取消",WS_TABSTOP,500,452,95,30);
        scale(GetDpiForWindow(window));page(0);
    }
    int number(int id,int limit) {
        wchar_t value[32]{};const int n=GetWindowTextW(item(id),value,32);int result=0;
        if(!n || n>2)throw std::runtime_error("数字超出允许范围。");
        for(int i=0;i<n;++i){if(value[i]<L'0'||value[i]>L'9')throw std::runtime_error("请输入整数。");result=result*10+value[i]-L'0';}
        if(result<1 || result>limit)throw std::runtime_error("数字超出允许范围。");return result;
    }
    void save() {
        std::vector<std::pair<std::u16string,std::u16string>> changes;
        for(int i=0;i<static_cast<int>(std::size(flags));++i) {
            bool value=SendMessageW(item(100+i),BM_GETCHECK,0,0)==BST_CHECKED;
            if(value!=initial.*flags[i].member)changes.emplace_back(flags[i].key,value?u"是":u"否");
        }
        const int max=number(MaxCode,16),page=number(PageSize,10);
        auto numberText=[](int n){auto value=std::to_wstring(n);return std::u16string(reinterpret_cast<const char16_t*>(value.data()),value.size());};
        if(max!=initial.maxCodeLength)changes.emplace_back(u"最大码长",numberText(max));
        if(page!=initial.pageSize)changes.emplace_back(u"每页候选个数",numberText(page));
        const auto selected=SendMessageW(item(PageKeys),CB_GETCURSEL,0,0);
        if(selected<0 || selected>=static_cast<LRESULT>(std::size(pageKeys)))throw std::runtime_error("请选择翻页键。");
        if(selected!=initial.pageKeys)changes.emplace_back(u"翻页键",pageKeys[selected]);
        for(int i=0;i<4;++i) {
            const bool value=SendMessageW(item(300+i),BM_GETCHECK,0,0)==BST_CHECKED;
            if(value!=initialStyle.*styleFlags[i].member)changes.emplace_back(styleFlags[i].key,value?u"是":u"否");
        }
        auto name=text(FontName);
        name=tiger::configurationValue(u"字体\t"+name,u"字体");
        if(name.empty())throw std::runtime_error("Font name is empty");
        if(name==std::u16string(reinterpret_cast<const char16_t*>(bundledFontLabel)))name=tiger::CandidateStyle{}.font;
        if(name!=initialStyle.font)changes.emplace_back(u"字体",name);
        const auto sizeValue=text(FontSize);
        std::string ascii;for(auto c:sizeValue){if(c>127)throw std::runtime_error("Invalid font size");ascii+=static_cast<char>(c);}
        std::istringstream input(ascii);input.imbue(std::locale::classic());double size=0;
        if(!(input>>size) || input.peek()!=std::char_traits<char>::eof() || !std::isfinite(size) || size<3 || size>200)
            throw std::runtime_error("Font size must be between 3 and 200");
        if(size!=initialStyle.fontSize)changes.emplace_back(u"字体大小",sizeValue);
        const auto selectedTheme=SendMessageW(item(Theme),CB_GETCURSEL,0,0);
        if(selectedTheme<0 || static_cast<std::size_t>(selectedTheme)>=themes.size())throw std::runtime_error("Select a theme");
        if(themes[selectedTheme]!=initialStyle.theme)changes.emplace_back(u"主题",themes[selectedTheme]);
        const bool add=SendMessageW(item(AddEnabled),BM_GETCHECK,0,0)==BST_CHECKED;
        const bool recent=SendMessageW(item(RecentEnabled),BM_GETCHECK,0,0)==BST_CHECKED;
        const auto addValue=static_cast<WORD>(SendMessageW(item(AddShortcut),HKM_GETHOTKEY,0,0));
        const auto recentValue=static_cast<WORD>(SendMessageW(item(RecentShortcut),HKM_GETHOTKEY,0,0));
        if(add!=initial.addWordEnabled)changes.emplace_back(u"Ctrl+等号手动加词",add?u"是":u"否");
        if(recent!=initial.recentSchemaEnabled)changes.emplace_back(u"Ctrl+m切换最近码表",recent?u"是":u"否");
        if(addValue!=hotkey(initial.addWordShortcut))changes.emplace_back(u"手动加词快捷键",shortcut(AddShortcut));
        if(recentValue!=hotkey(initial.recentSchemaShortcut))changes.emplace_back(u"切换最近码表快捷键",shortcut(RecentShortcut));
        tiger::saveInputConfiguration(path,changes,[&](std::u16string_view updated) {
            const auto effective=tiger::parseEngineSettings(updated);
            if((add && !effective.addWordEnabled) || (recent && !effective.recentSchemaEnabled))
                throw std::runtime_error("快捷键重复或与保留操作冲突，请重新设置。");
            if((addValue!=hotkey(initial.addWordShortcut) && hotkey(effective.addWordShortcut)!=(addValue&~(HOTKEYF_EXT<<8))) ||
               (recentValue!=hotkey(initial.recentSchemaShortcut) && hotkey(effective.recentSchemaShortcut)!=(recentValue&~(HOTKEYF_EXT<<8))))
                throw std::runtime_error("快捷键不受支持，请重新设置。");
        });saved=true;DestroyWindow(window);
    }
};
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto self=reinterpret_cast<Dialog*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){self=static_cast<Dialog*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(window,message,w,l);
    try {
        if(message==WM_COMMAND){if(LOWORD(w)==SelectionEditor){showSelectionSettings(window,self->path.parent_path()/L"自定义选重键.txt");return 0;}if(LOWORD(w)==ShortcutPage){self->page(2);return 0;}if(LOWORD(w)==InputPage){self->page(0);return 0;}if(LOWORD(w)==AppearancePage){self->page(1);return 0;}if(LOWORD(w)==Save || LOWORD(w)==IDOK)self->save();else if(LOWORD(w)==Cancel || LOWORD(w)==IDCANCEL)DestroyWindow(window);return 0;}
        if(message==WM_DPICHANGED){const auto r=reinterpret_cast<RECT*>(l);SetWindowPos(window,nullptr,r->left,r->top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);self->scale(HIWORD(w));return 0;}
        if(message==WM_CLOSE){DestroyWindow(window);return 0;}
    } catch(const std::exception& error) {
        const int length=MultiByteToWideChar(CP_UTF8,0,error.what(),-1,nullptr,0);
        std::wstring detail(static_cast<std::size_t>(length),L'\0');
        if(length>0)MultiByteToWideChar(CP_UTF8,0,error.what(),-1,detail.data(),length);
        if(!detail.empty())detail.pop_back();
        self->error=L"保存失败："+detail;SetWindowTextW(self->item(Notice),self->error.c_str());
    }
    return DefWindowProcW(window,message,w,l);
}
}
bool showInputSettings(HWND owner,const std::filesystem::path& path,int testMode) {
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_HOTKEY_CLASS};if(!InitCommonControlsEx(&common))throw std::runtime_error("Cannot initialize shortcut controls");
    Dialog dialog;dialog.path=path;
    const auto settings=tiger::readConfiguration(path);
    dialog.initial=tiger::parseEngineSettings(settings);dialog.initialStyle=tiger::parseCandidateStyle(settings);
    WNDCLASSW type{};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=procedure;type.lpszClassName=L"NativeTigerInputSettings";type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassW(&type) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Cannot register input settings window");
    if(!CreateWindowExW(WS_EX_CONTROLPARENT,type.lpszClassName,L"原生虎码 · 输入设置",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,640,460,owner,nullptr,type.hInstance,&dialog))throw std::runtime_error("Cannot create input settings window");
    dialog.create();
    if(testMode) {
        const auto original=tiger::readConfiguration(path);
        for(UINT dpi:{96u,144u,192u}) {
            dialog.scale(dpi);RECT client{};GetClientRect(dialog.window,&client);
            for(auto control:dialog.controls) {
                RECT bounds{};GetWindowRect(control.window,&bounds);
                MapWindowPoints(nullptr,dialog.window,reinterpret_cast<POINT*>(&bounds),2);
                if(bounds.left<0 || bounds.top<0 || bounds.right>client.right || bounds.bottom>client.bottom)
                    throw std::runtime_error("Input settings control exceeds client bounds");
            }
        }
        if(testMode==2) {
            SendMessageW(dialog.item(100),BM_SETCHECK,dialog.initial.defaultChinese?BST_UNCHECKED:BST_CHECKED,0);
            SetWindowTextW(dialog.item(MaxCode),L"2");
            SendMessageW(dialog.item(AddShortcut),HKM_SETHOTKEY,MAKEWORD('Z',HOTKEYF_CONTROL|HOTKEYF_ALT),0);
            SetWindowTextW(dialog.item(FontName),bundledFontLabel);SetWindowTextW(dialog.item(FontSize),L"20");
            SendMessageW(dialog.window,WM_COMMAND,Cancel,0);
            if(dialog.saved || IsWindow(dialog.window) || tiger::readConfiguration(path)!=original)
                throw std::runtime_error("Cancel changed input settings");
            return false;
        }
        showSelectionSettings(dialog.window,path.parent_path()/L"自定义选重键.txt",1);
        showSelectionSettings(dialog.window,path.parent_path()/L"自定义选重键.txt",2);
        const auto restorePath=path.parent_path()/L"恢复选重键测试.txt";
        tiger::updateSelectionKeys(restorePath,tiger::SelectionKeys{},tiger::SelectionKeys::load(path.parent_path()/L"自定义选重键.txt"));
        showSelectionSettings(dialog.window,restorePath,3);
        const auto repairPath=path.parent_path()/L"损坏选重键测试.txt";
        tiger::updateConfigurationValues(repairPath,{{u"无效标签",u"原始内容"}});
        showSelectionSettings(dialog.window,repairPath,2);
        showSelectionSettings(dialog.window,repairPath,5);
        showSelectionSettings(dialog.window,repairPath,4);
        const std::vector<std::string> brokenEncodings={std::string("\xc3\x28",2),std::string("\xff\xfe\x31",3),
            std::string("\xff\xfe\x00\xd8",4),std::string("\xff\xfe\x00\x00\x00\x00\x11\x00",8)};
        for(std::size_t i=0;i<brokenEncodings.size();++i) {
            const auto broken=path.parent_path()/(L"encoding-selection-"+std::to_wstring(i)+L".txt");
            {std::ofstream file(broken,std::ios::binary);file.write(brokenEncodings[i].data(),brokenEncodings[i].size());if(!file.good())throw std::runtime_error("Cannot write encoding fixture");}
            // An unreadable file must not be mistaken for malformed Unicode.
            HANDLE denied=CreateFileW(broken.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(denied==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot lock encoding fixture");
            bool rejected=false;
            try{showSelectionSettings(dialog.window,broken,2);}catch(const std::exception&){rejected=true;}
            CloseHandle(denied);
            if(!rejected)throw std::runtime_error("Unreadable selection file opened as recoverable");
            showSelectionSettings(dialog.window,broken,2);
            showSelectionSettings(dialog.window,broken,6);
            showSelectionSettings(dialog.window,broken,4);
        }
        const auto before=tiger::readConfiguration(path);
        SetWindowTextW(dialog.item(PageSize),L"11");SendMessageW(dialog.window,WM_COMMAND,Save,0);
        if(dialog.saved || !IsWindow(dialog.window) || tiger::readConfiguration(path)!=before)throw std::runtime_error("Invalid settings were saved");
        SetWindowTextW(dialog.item(PageSize),L"10");
        SendMessageW(dialog.window,WM_COMMAND,AppearancePage,0);
        if((GetWindowLongPtrW(dialog.item(MaxCode),GWL_STYLE)&WS_VISIBLE) || !(GetWindowLongPtrW(dialog.item(FontName),GWL_STYLE)&WS_VISIBLE))
            throw std::runtime_error("Settings page switch failed");
        for(auto invalid:{L"201",L"NaN",L"2.5"}) {
            SetWindowTextW(dialog.item(FontSize),invalid);SendMessageW(dialog.window,WM_COMMAND,Save,0);
            if(dialog.saved || !IsWindow(dialog.window) || tiger::readConfiguration(path)!=before)throw std::runtime_error("Invalid font size was saved");
        }
        SetWindowTextW(dialog.item(FontSize),L"17.5");SetWindowTextW(dialog.item(FontName),L"Microsoft YaHei UI");
        SendMessageW(dialog.item(300),BM_SETCHECK,BST_UNCHECKED,0);
        SendMessageW(dialog.item(302),BM_SETCHECK,BST_CHECKED,0);
        for(std::size_t i=0;i<tiger::candidateThemeNames.size();++i) {
            SendMessageW(dialog.item(Theme),CB_SETCURSEL,i,0);
            const auto value=dialog.text(Theme);
            if(value!=tiger::candidateThemeNames[i] || tiger::parseCandidateStyle(u"主题\t"+value).theme!=value)
                throw std::runtime_error("Theme choice does not match renderer names");
        }
        // Every displayed choice must map to the engine's existing mode.
        for(int i=0;i<4;++i) {
            SendMessageW(dialog.item(PageKeys),CB_SETCURSEL,i,0);
            wchar_t label[64]{};SendMessageW(dialog.item(PageKeys),CB_GETLBTEXT,i,reinterpret_cast<LPARAM>(label));
            const auto value=std::u16string(reinterpret_cast<const char16_t*>(label));
            if(tiger::parseEngineSettings(u"翻页键\t"+value).pageKeys!=i)
                throw std::runtime_error("Paging choice does not match engine semantics");
        }
        SendMessageW(dialog.window,WM_COMMAND,ShortcutPage,0);
        SendMessageW(dialog.item(AddEnabled),BM_SETCHECK,BST_CHECKED,0);
        SendMessageW(dialog.item(RecentEnabled),BM_SETCHECK,BST_CHECKED,0);
        SendMessageW(dialog.item(AddShortcut),HKM_SETHOTKEY,MAKEWORD('K',HOTKEYF_CONTROL|HOTKEYF_ALT),0);
        SendMessageW(dialog.item(RecentShortcut),HKM_SETHOTKEY,MAKEWORD('K',HOTKEYF_CONTROL|HOTKEYF_ALT),0);
        SendMessageW(dialog.window,WM_COMMAND,Save,0);
        if(dialog.saved || !IsWindow(dialog.window) || tiger::readConfiguration(path)!=before)throw std::runtime_error("Conflicting shortcuts were saved");
        SendMessageW(dialog.item(RecentShortcut),HKM_SETHOTKEY,MAKEWORD('J',HOTKEYF_CONTROL|HOTKEYF_ALT),0);
        for(WORD invalid:{MAKEWORD('K',0),MAKEWORD(VK_SPACE,HOTKEYF_CONTROL)}) {
            SendMessageW(dialog.item(AddShortcut),HKM_SETHOTKEY,invalid,0);
            SendMessageW(dialog.window,WM_COMMAND,Save,0);
            if(dialog.saved || !IsWindow(dialog.window) || tiger::readConfiguration(path)!=before)
                throw std::runtime_error("Invalid or reserved shortcut was saved");
        }
        SendMessageW(dialog.item(AddShortcut),HKM_SETHOTKEY,MAKEWORD('K',HOTKEYF_CONTROL|HOTKEYF_ALT),0);
        // Simulate another process switching schemas while the dialog is open.
        tiger::updateConfigurationValues(path,{{u"当前码表",u"并发方案"}});
        SetWindowTextW(dialog.item(PageSize),L"10");SetWindowTextW(dialog.item(MaxCode),L"6");
        SendMessageW(dialog.item(100),BM_SETCHECK,dialog.initial.defaultChinese?BST_UNCHECKED:BST_CHECKED,0);
        SendMessageW(dialog.window,WM_COMMAND,Save,0);
        if(!dialog.saved)throw std::runtime_error("Settings dialog save failed");return true;
    }
    const bool enabled=owner && IsWindowEnabled(owner);if(enabled)EnableWindow(owner,FALSE);
    ShowWindow(dialog.window,SW_SHOW);SetFocus(dialog.item(100));
    MSG message{};while(IsWindow(dialog.window)) {
        const auto result=GetMessageW(&message,nullptr,0,0);if(result<=0){if(!result)PostQuitMessage(static_cast<int>(message.wParam));break;}
        if(!IsDialogMessageW(dialog.window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
    }
    if(enabled){EnableWindow(owner,TRUE);SetActiveWindow(owner);}return dialog.saved;
}
