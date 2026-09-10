#define NOMINMAX
#include "InputSettings.h"
#include "SelectionSettings.h"
#include "SelectionKeys.h"
#include "ConfigStore.h"
#include "Settings.h"
#include "CandidateTheme.h"
#include "SentenceSettings.h"
#include "FontChooser.h"
#include "Text.h"
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <locale>
#include <cmath>
#include <commctrl.h>
#include <iomanip>
#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib,"windowscodecs.lib")
namespace {
constexpr int Pages=221;
constexpr int Donation=223;
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
constexpr int CandidateDelay=218,AnnotationDelay=219,CodeMask=220;
constexpr int SentencePage=217,SentenceEnabled=400,SentenceAuto=401,SentenceDuplicate=402,SentenceCommon=403,SentenceRetained=404,SentenceWhitelist=405;
const char16_t* pageKeys[]={u"- =",u"[ ]",u"Shift Tab/Tab",u"PageUp/PageDown"};
const wchar_t* wide(const char16_t* s){return reinterpret_cast<const wchar_t*>(s);}
struct Dialog {
    HBRUSH background=CreateSolidBrush(RGB(246,242,234)),panel=CreateSolidBrush(RGB(255,253,248));
    static bool highContrast(){HIGHCONTRASTW value{sizeof(value)};return SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(value),&value,0) && (value.dwFlags&HCF_HIGHCONTRASTON);}
    HBRUSH backgroundBrush(bool inPage=false)const{return highContrast()?GetSysColorBrush(COLOR_BTNFACE):(inPage?panel:background);}
    static COLORREF textColor(bool help=false){return highContrast()?GetSysColor(COLOR_BTNTEXT):(help?RGB(125,107,93):RGB(58,42,32));}
    HWND window=nullptr,content=nullptr;HFONT font=nullptr,helpFont=nullptr;std::filesystem::path path;
    std::vector<std::u16string> themes;
    std::unique_ptr<FontChooser> fonts;
    tiger::Config initial;tiger::CandidateStyle initialStyle;int buildingPage=-1;bool saved=false;std::wstring error;
    tiger::SentenceSettings initialSentence;
    std::vector<BYTE> donationPixels;BITMAPINFO donationInfo{};
    void loadDonation() {
        using Microsoft::WRL::ComPtr;
        auto check=[](HRESULT result){if(FAILED(result))throw std::runtime_error("Cannot decode donation image");};
        const auto module=GetModuleHandleW(nullptr);auto resource=FindResourceW(module,MAKEINTRESOURCEW(13),RT_RCDATA);
        if(!resource)throw std::runtime_error("Donation image resource is missing");
        auto data=LoadResource(module,resource);auto bytes=static_cast<BYTE*>(LockResource(data));const auto length=SizeofResource(module,resource);
        if(!bytes || !length)throw std::runtime_error("Donation image is empty");
        ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
        ComPtr<IWICStream> stream;check(factory->CreateStream(&stream));check(stream->InitializeFromMemory(bytes,length));
        ComPtr<IWICBitmapDecoder> decoder;check(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder));
        ComPtr<IWICBitmapFrameDecode> frame;check(decoder->GetFrame(0,&frame));
        UINT width=0,height=0;check(frame->GetSize(&width,&height));
        if(!width || !height || width>4096 || height>4096)throw std::runtime_error("Invalid donation image size");
        ComPtr<IWICFormatConverter> converter;check(factory->CreateFormatConverter(&converter));
        check(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGR,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
        donationPixels.resize(static_cast<size_t>(width)*height*4);check(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(donationPixels.size()),donationPixels.data()));
        auto& info=donationInfo.bmiHeader;info.biSize=sizeof(info);info.biWidth=width;info.biHeight=-static_cast<LONG>(height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
    }
    void drawDonation(const DRAWITEMSTRUCT& draw) {
        const int savedDc=SaveDC(draw.hDC);FillRect(draw.hDC,&draw.rcItem,backgroundBrush(true));
        const int width=donationInfo.bmiHeader.biWidth,height=-donationInfo.bmiHeader.biHeight;
        const int availableWidth=draw.rcItem.right-draw.rcItem.left,availableHeight=draw.rcItem.bottom-draw.rcItem.top;
        int w=availableWidth,h=MulDiv(height,w,width);if(h>availableHeight){h=availableHeight;w=MulDiv(width,h,height);}
        SetStretchBltMode(draw.hDC,HALFTONE);SetBrushOrgEx(draw.hDC,0,0,nullptr);
        StretchDIBits(draw.hDC,draw.rcItem.left+(availableWidth-w)/2,draw.rcItem.top+(availableHeight-h)/2,w,h,0,0,width,height,donationPixels.data(),&donationInfo,DIB_RGB_COLORS,SRCCOPY);
        RestoreDC(draw.hDC,savedDc);
    }
    struct Control {HWND window;int x,y,w,h,page;bool help;};std::vector<Control> controls;
    ~Dialog(){if(IsWindow(window))DestroyWindow(window);if(font)DeleteObject(font);if(helpFont)DeleteObject(helpFont);if(background)DeleteObject(background);if(panel)DeleteObject(panel);}
    void drawTab(const DRAWITEMSTRUCT& draw) {
        const int savedDc=SaveDC(draw.hDC);
        const bool selected=static_cast<int>(draw.itemID)==TabCtrl_GetCurSel(item(Pages));
        const bool contrast=highContrast();
        SetDCBrushColor(draw.hDC,contrast?GetSysColor(selected?COLOR_HIGHLIGHT:COLOR_BTNFACE):(selected?RGB(242,199,157):RGB(246,242,234)));
        FillRect(draw.hDC,&draw.rcItem,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        if(selected && !contrast){auto stripe=draw.rcItem;stripe.top=stripe.bottom-MulDiv(2,GetDpiForWindow(window),96);SetDCBrushColor(draw.hDC,RGB(217,106,27));FillRect(draw.hDC,&stripe,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));}
        wchar_t title[80]{};TCITEMW tab{};tab.mask=TCIF_TEXT;tab.pszText=title;tab.cchTextMax=80;TabCtrl_GetItem(item(Pages),draw.itemID,&tab);
        SelectObject(draw.hDC,font);SetBkMode(draw.hDC,TRANSPARENT);
        SetTextColor(draw.hDC,contrast && selected?GetSysColor(COLOR_HIGHLIGHTTEXT):textColor());
        auto bounds=draw.rcItem;DrawTextW(draw.hDC,title,-1,&bounds,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        if((draw.itemState&ODS_FOCUS) && !(draw.itemState&ODS_NOFOCUSRECT)){InflateRect(&bounds,-3,-3);DrawFocusRect(draw.hDC,&bounds);}
        RestoreDC(draw.hDC,savedDc);
    }
    HWND item(int id){for(auto c:controls)if(GetDlgCtrlID(c.window)==id)return c.window;return nullptr;}
    static LRESULT CALLBACK contentProcedure(HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR) {
        switch(message) {
        case WM_COMMAND:case WM_NOTIFY:case WM_DRAWITEM:case WM_MEASUREITEM:
        case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORBTN:case WM_CTLCOLORLISTBOX:
            return SendMessageW(GetParent(hwnd),message,w,l);
        }
        return DefSubclassProc(hwnd,message,w,l);
    }
    void control(int id,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h,bool help=false) {
        if(buildingPage>=0)y+=40;
        auto child=CreateWindowExW(type==std::wstring_view(L"EDIT")?WS_EX_CLIENTEDGE:0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,buildingPage>=0?content:window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        if(!child)throw std::runtime_error("Cannot create input settings control");controls.push_back({child,x,y,w,h,buildingPage,help});
    }
    void scale(UINT dpi) {
        auto px=[&](int n){return MulDiv(n,static_cast<int>(dpi),96);};
        auto next=CreateFontW(-px(16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,0,L"DengXian");
        if(!next)throw std::runtime_error("Cannot create input settings font");
        auto nextHelp=CreateFontW(-px(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,0,L"DengXian");
        if(!nextHelp){DeleteObject(next);throw std::runtime_error("Cannot create settings description font");}
        RECT pane{};
        for(auto c:controls){
            SendMessageW(c.window,WM_SETFONT,reinterpret_cast<WPARAM>(c.help && !(GetDlgCtrlID(c.window)==Notice && !error.empty())?nextHelp:next),TRUE);
            if(c.window==content)MoveWindow(content,pane.left,pane.top,pane.right-pane.left,pane.bottom-pane.top,TRUE);
            else MoveWindow(c.window,px(c.x)-(c.page>=0?pane.left:0),px(c.y)-(c.page>=0?pane.top:0),px(c.w),px(c.h),TRUE);
            if(c.window==item(Pages)) {
                TabCtrl_SetItemSize(c.window,px(570/TabCtrl_GetItemCount(c.window)),px(26));
                GetClientRect(c.window,&pane);TabCtrl_AdjustRect(c.window,FALSE,&pane);
                MapWindowPoints(c.window,window,reinterpret_cast<POINT*>(&pane),2);
            }
        }
        if(font)DeleteObject(font);font=next;
        if(helpFont)DeleteObject(helpFont);helpFont=nextHelp;
        if(fonts)fonts->scale(dpi);
        RECT bounds{0,0,px(620),px(520)};
        if(!AdjustWindowRectExForDpi(&bounds,static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE)),FALSE,WS_EX_CONTROLPARENT,GetDpiForWindow(window)))throw std::runtime_error("Cannot size input settings window");
        SetWindowPos(window,nullptr,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
    void page(int selected) {
        TabCtrl_SetCurSel(item(Pages),selected);
        for(auto c:controls)ShowWindow(c.window,c.page<0 || c.page==selected?SW_SHOW:SW_HIDE);
        RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
    }
    // Render the real controls without showing or activating the test window.
    void captureSentence(const std::filesystem::path& destination,int selectedPage=3) {
        HWND list=nullptr;
        if(selectedPage==-2){COMBOBOXINFO info{sizeof(info)};if(!GetComboBoxInfo(item(FontName),&info))throw std::runtime_error("Cannot inspect font dropdown");list=info.hwndList;}
        RECT client{};GetClientRect(list?list:window,&client);
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=client.right;info.bmiHeader.biHeight=-client.bottom;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* pixels=nullptr;HDC dc=CreateCompatibleDC(nullptr);
        HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        if(!dc || !bitmap){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);throw std::runtime_error("Cannot create settings capture");}
        const auto previous=SelectObject(dc,bitmap);FillRect(dc,&client,backgroundBrush());
        if(list)SendMessageW(list,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND);
        else for(auto c:controls)if(c.page<0 || c.page==selectedPage) {
            RECT rect{};GetWindowRect(c.window,&rect);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&rect),2);
            SetViewportOrgEx(dc,rect.left,rect.top,nullptr);
            SendMessageW(c.window,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND);
        }
        GdiFlush();
        BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
        const auto bytes=static_cast<DWORD>(client.right*client.bottom*4);header.bfSize=header.bfOffBits+bytes;
        std::ofstream file(destination,std::ios::binary);
        file.write(reinterpret_cast<const char*>(&header),sizeof(header));file.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(info.bmiHeader));
        file.write(static_cast<const char*>(pixels),bytes);const bool ok=file.good();
        SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);
        if(!ok)throw std::runtime_error("Cannot write settings capture");
    }
    static std::wstring sizeText(double value) {
        std::wostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)<<value;return out.str();
    }
    std::u16string text(int id) {
        const auto control=item(id);const int length=GetWindowTextLengthW(control);
        if(length>4*1024*1024)throw std::runtime_error("Setting is too long");
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
        if(!background || !panel)throw std::runtime_error("Cannot create settings colors");
        control(Pages,WC_TABCONTROLW,L"",WS_TABSTOP|WS_CLIPSIBLINGS|TCS_OWNERDRAWFIXED|TCS_FIXEDWIDTH,8,8,604,408);
        for(const auto title:{L"输入行为",L"候选外观",L"操作快捷键",L"整句输入",L"赞赏"}) {
            TCITEMW tab{};tab.mask=TCIF_TEXT;tab.pszText=const_cast<wchar_t*>(title);
            TabCtrl_InsertItem(item(Pages),TabCtrl_GetItemCount(item(Pages)),&tab);
        }
        SetWindowPos(item(Pages),HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        control(222,L"STATIC",L"",WS_CLIPCHILDREN,12,40,596,372);
        content=item(222);
        SetWindowLongPtrW(content,GWL_EXSTYLE,GetWindowLongPtrW(content,GWL_EXSTYLE)|WS_EX_CONTROLPARENT);
        if(!SetWindowSubclass(content,contentProcedure,1,0))throw std::runtime_error("Cannot initialize settings page container");
        buildingPage=0;
        for(int i=0;i<static_cast<int>(std::size(flags));++i) {
            control(100+i,L"BUTTON",wide(flags[i].key),BS_AUTOCHECKBOX|WS_TABSTOP,18+(i/8)*302,18+(i%8)*32,280,27);
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
        control(0,L"STATIC",L"候选字体（内置字体在前，列表按字体本身预览）",0,18,20,570,25);
        wchar_t executable[32768]{};const auto length=GetModuleFileNameW(nullptr,executable,32768);
        if(!length || length>=32768)throw std::runtime_error("Cannot locate bundled fonts");
        fonts=std::make_unique<FontChooser>(std::filesystem::path(executable).parent_path()/L"字体",initialStyle.fontSize);
        control(FontName,L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP|WS_VSCROLL,18,55,570,300);
        fonts->attach(item(FontName),wide(initialStyle.font.c_str()));
        control(0,L"STATIC",L"字号（3–200，支持小数）",0,18,105,310,25);
        control(FontSize,L"EDIT",sizeText(initialStyle.fontSize).c_str(),ES_AUTOHSCROLL|WS_TABSTOP,340,101,130,28);
        for(int i=0;i<4;++i) {
            control(300+i,L"BUTTON",wide(styleFlags[i].key),BS_AUTOCHECKBOX|WS_TABSTOP,18+(i%2)*300,145+(i/2)*30,280,28);
            SendMessageW(item(300+i),BM_SETCHECK,initialStyle.*styleFlags[i].member?BST_CHECKED:BST_UNCHECKED,0);
        }
        control(0,L"STATIC",L"候选主题",0,18,215,170,25);
        control(Theme,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,205,211,385,220);
        int themeIndex=-1;
        for(auto name:tiger::candidateThemeNames){if(name==initialStyle.theme)themeIndex=static_cast<int>(themes.size());themes.emplace_back(name);}
        if(themeIndex<0){themeIndex=static_cast<int>(themes.size());themes.push_back(initialStyle.theme);}
        for(const auto& name:themes)SendMessageW(item(Theme),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(name.c_str())));
        SendMessageW(item(Theme),CB_SETCURSEL,themeIndex,0);
        control(0,L"STATIC",L"候选延时（毫秒）",0,18,251,170,25);
        control(CandidateDelay,L"EDIT",std::to_wstring(initialStyle.candidateDelayMs).c_str(),ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,190,247,85,28);
        control(0,L"STATIC",L"注释／拆分延时",0,300,251,175,25);
        control(AnnotationDelay,L"EDIT",std::to_wstring(initialStyle.annotationDelayMs).c_str(),ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,490,247,100,28);
        control(0,L"STATIC",L"范围 0–60000；0 为立即显示，从本次输入开始分别计时。",0,18,281,580,20,true);
        control(0,L"STATIC",L"编码伪装（留空关闭）",0,18,311,190,25);
        control(CodeMask,L"EDIT",wide(initialStyle.codeMask.c_str()),ES_AUTOHSCROLL|WS_TABSTOP,215,307,375,28);
        control(0,L"STATIC",L"如填 ●，ab 显示为 ●●；不改变实际查码和上屏文字。",0,18,341,580,20,true);
        buildingPage=2;
        control(AddEnabled,L"BUTTON",L"启用手动加词",BS_AUTOCHECKBOX|WS_TABSTOP,18,20,570,28);
        control(AddShortcut,HOTKEY_CLASSW,L"",WS_TABSTOP,18,60,570,32);
        control(RecentEnabled,L"BUTTON",L"启用最近方案切换",BS_AUTOCHECKBOX|WS_TABSTOP,18,125,570,28);
        control(RecentShortcut,HOTKEY_CLASSW,L"",WS_TABSTOP,18,165,570,32);
        control(0,L"STATIC",L"点击快捷键框后直接按键；组合键需包含 Ctrl 或 Alt。",0,18,220,570,50,true);
        control(SelectionEditor,L"BUTTON",L"编辑选重键…",WS_TABSTOP,18,290,220,32);
        control(0,L"STATIC",L"选重键在独立窗口中保存。",0,260,295,320,25,true);
        SendMessageW(item(AddEnabled),BM_SETCHECK,initial.addWordEnabled?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(item(RecentEnabled),BM_SETCHECK,initial.recentSchemaEnabled?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(item(AddShortcut),HKM_SETHOTKEY,hotkey(initial.addWordShortcut),0);
        SendMessageW(item(RecentShortcut),HKM_SETHOTKEY,hotkey(initial.recentSchemaShortcut),0);
        buildingPage=3;
        control(SentenceEnabled,L"BUTTON",L"方案名称包含“整句”时自动启用整句模式",BS_AUTOCHECKBOX|WS_TABSTOP,18,16,580,28);
        control(SentenceAuto,L"BUTTON",L"自动提前上屏已确定的句子前缀",BS_AUTOCHECKBOX|WS_TABSTOP,18,52,580,28);
        control(SentenceDuplicate,L"BUTTON",L"允许单字重码组句",BS_AUTOCHECKBOX|WS_TABSTOP,18,88,580,28);
        SendMessageW(item(SentenceEnabled),BM_SETCHECK,initialSentence.autoEnableBySchema?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(item(SentenceAuto),BM_SETCHECK,initialSentence.autoCommit?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(item(SentenceDuplicate),BM_SETCHECK,initialSentence.allowDuplicateSingleCharacters?BST_CHECKED:BST_UNCHECKED,0);
        control(0,L"STATIC",L"提前上屏后最少保留编码（0–32）",0,18,134,380,28);
        control(SentenceRetained,L"EDIT",std::to_wstring(initialSentence.minimumRetainedRaw).c_str(),ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,430,130,155,28);
        control(0,L"STATIC",L"仅使用最优码组句的高频字数量",0,18,180,390,28);
        control(SentenceCommon,L"EDIT",std::to_wstring(initialSentence.commonCharacterLimit).c_str(),ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,430,176,155,28);
        control(0,L"STATIC",L"默认 1500；设为 0 时不按高频字限制全码。",0,18,214,580,28,true);
        control(0,L"STATIC",L"允许全码组句的例外字符（可留空）",0,18,258,580,28);
        control(SentenceWhitelist,L"EDIT",wide(initialSentence.fullCodeWhitelist.c_str()),ES_AUTOHSCROLL|WS_TABSTOP,18,292,567,30);
        SendMessageW(item(SentenceWhitelist),EM_SETLIMITTEXT,4*1024*1024,0);
        control(0,L"STATIC",L"直接填写汉字，无需分隔；这些字不受上方的最优码限制。",0,18,332,580,28,true);
        buildingPage=4;
        loadDonation();
        control(0,L"STATIC",L"感谢你对虎娘的支持",SS_CENTER,18,8,580,25);
        control(Donation,L"STATIC",L"赞赏码",SS_OWNERDRAW,160,48,300,300);
        control(0,L"STATIC",L"微信扫码赞赏，自愿支持。",SS_CENTER,18,352,580,18,true);
        buildingPage=-1;
        control(Notice,L"STATIC",L"保存更改后自动更新设置，清除未完成编码，并应用默认中英文模式。",0,18,424,582,36,true);
        control(Save,L"BUTTON",L"保存",BS_DEFPUSHBUTTON|WS_TABSTOP,390,480,95,30);
        control(Cancel,L"BUTTON",L"取消",WS_TABSTOP,500,480,95,30);
        scale(GetDpiForWindow(window));page(0);
    }
    int number(int id,int limit) {
        wchar_t value[32]{};const int n=GetWindowTextW(item(id),value,32);int result=0;
        if(!n || n>2)throw std::runtime_error("数字超出允许范围。");
        for(int i=0;i<n;++i){if(value[i]<L'0'||value[i]>L'9')throw std::runtime_error("请输入整数。");result=result*10+value[i]-L'0';}
        if(result<1 || result>limit)throw std::runtime_error("数字超出允许范围。");return result;
    }
    int delay(int id) {
        const auto value=text(id);if(value.empty())return 0;
        if(value.size()>5)throw std::runtime_error("延时必须为 0–60000 毫秒。");
        int result=0;for(auto c:value){if(c<u'0' || c>u'9')throw std::runtime_error("延时必须为整数。");result=result*10+c-u'0';}
        if(result>60000)throw std::runtime_error("延时必须为 0–60000 毫秒。");return result;
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
        const auto mask=text(CodeMask);
        if(mask!=initialStyle.codeMask)changes.emplace_back(u"编码伪装",mask);
        const auto selectedFont=fonts->selected();
        auto name=std::u16string(reinterpret_cast<const char16_t*>(selectedFont.data()),selectedFont.size());
        name=tiger::configurationValue(u"字体\t"+name,u"字体");
        if(name.empty())throw std::runtime_error("Font name is empty");
        if(name!=initialStyle.font)changes.emplace_back(u"字体",name);
        const auto sizeValue=text(FontSize);
        std::string ascii;for(auto c:sizeValue){if(c>127)throw std::runtime_error("Invalid font size");ascii+=static_cast<char>(c);}
        std::istringstream input(ascii);input.imbue(std::locale::classic());double size=0;
        if(!(input>>size) || input.peek()!=std::char_traits<char>::eof() || !std::isfinite(size) || size<3 || size>200)
            throw std::runtime_error("Font size must be between 3 and 200");
        const auto candidateDelay=delay(CandidateDelay),annotationDelay=delay(AnnotationDelay);
        if(candidateDelay!=initialStyle.candidateDelayMs)changes.emplace_back(u"延时显示候选(毫秒)",numberText(candidateDelay));
        if(annotationDelay!=initialStyle.annotationDelayMs)changes.emplace_back(u"延时展开注释和拆分(毫秒)",numberText(annotationDelay));
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
        for(auto entry:{std::make_pair(SentenceEnabled,std::make_pair(u"自动启用整句模式",initialSentence.autoEnableBySchema)),
                        std::make_pair(SentenceAuto,std::make_pair(u"整句自动提前上屏",initialSentence.autoCommit)),
                        std::make_pair(SentenceDuplicate,std::make_pair(u"允许单字重码组句",initialSentence.allowDuplicateSingleCharacters))}) {
            const bool value=SendMessageW(item(entry.first),BM_GETCHECK,0,0)==BST_CHECKED;
            if(value!=entry.second.second)changes.emplace_back(entry.second.first,value?u"是":u"否");
        }
        auto nonnegative=[&](int id,unsigned limit) {
            const auto value=text(id);unsigned result=0;
            if(value.empty())throw std::runtime_error("整句参数不能为空，请输入非负整数。");
            for(auto ch:value) {
                if(ch<u'0' || ch>u'9' || result>limit/10 || (result==limit/10 && static_cast<unsigned>(ch-u'0')>limit%10))
                    throw std::runtime_error("整句参数超出范围：保留编码为 0–32，高频字数量为 0–2147483647。");
                result=result*10+ch-u'0';
            }
            return static_cast<int>(result);
        };
        const auto retained=nonnegative(SentenceRetained,32),common=nonnegative(SentenceCommon,2147483647);
        if(retained!=initialSentence.minimumRetainedRaw)changes.emplace_back(u"保留最少编码数量",numberText(retained));
        if(common!=initialSentence.commonCharacterLimit)changes.emplace_back(u"高频字仅使用最优码组句",numberText(common));
        const auto whitelist=text(SentenceWhitelist);
        if(whitelist!=initialSentence.fullCodeWhitelist)changes.emplace_back(u"整句允许全码组句白名单",whitelist);
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
        if(message==WM_ERASEBKGND){RECT client{};GetClientRect(window,&client);FillRect(reinterpret_cast<HDC>(w),&client,self->backgroundBrush());return 1;}
        if(message==WM_SETTINGCHANGE || message==WM_SYSCOLORCHANGE || message==WM_THEMECHANGED)
            RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
        if(message==WM_CTLCOLORSTATIC) {
            const auto dc=reinterpret_cast<HDC>(w);
            bool help=false;for(auto c:self->controls)if(c.window==reinterpret_cast<HWND>(l)){help=c.help;break;}
            const bool failure=reinterpret_cast<HWND>(l)==self->item(Notice) && !self->error.empty();
            const bool inPage=reinterpret_cast<HWND>(l)==self->content || GetParent(reinterpret_cast<HWND>(l))==self->content;
            const auto brush=self->backgroundBrush(inPage);LOGBRUSH colors{};GetObjectW(brush,sizeof(colors),&colors);
            SetTextColor(dc,self->textColor(help && !failure));SetBkColor(dc,colors.lbColor);
            return reinterpret_cast<LRESULT>(brush);
        }
        if(message==WM_CTLCOLOREDIT || message==WM_CTLCOLORLISTBOX){const auto dc=reinterpret_cast<HDC>(w);SetTextColor(dc,Dialog::highContrast()?GetSysColor(COLOR_WINDOWTEXT):self->textColor());SetBkColor(dc,GetSysColor(COLOR_WINDOW));return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));}
        if(message==WM_NOTIFY && reinterpret_cast<NMHDR*>(l)->idFrom==Pages && reinterpret_cast<NMHDR*>(l)->code==TCN_SELCHANGE) {
            self->page(TabCtrl_GetCurSel(self->item(Pages)));return 0;
        }
        if(message==WM_DRAWITEM && w==Pages){self->drawTab(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}
        if(message==WM_DRAWITEM && w==Donation){self->drawDonation(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}
        if(message==WM_DRAWITEM && w==FontName && self->fonts){self->fonts->draw(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}
        if(message==WM_MEASUREITEM && w==FontName && self->fonts){self->fonts->measure(*reinterpret_cast<MEASUREITEMSTRUCT*>(l));return TRUE;}
        if(message==WM_COMMAND && LOWORD(w)==FontName && HIWORD(w)==CBN_SELCHANGE){InvalidateRect(self->item(FontName),nullptr,TRUE);return 0;}
        if(message==WM_COMMAND && LOWORD(w)==SentencePage){self->page(3);return 0;}
        if(message==WM_COMMAND){if(LOWORD(w)==SelectionEditor){showSelectionSettings(window,self->path.parent_path()/L"自定义选重键.txt");return 0;}if(LOWORD(w)==ShortcutPage){self->page(2);return 0;}if(LOWORD(w)==InputPage){self->page(0);return 0;}if(LOWORD(w)==AppearancePage){self->page(1);return 0;}if(LOWORD(w)==Save || LOWORD(w)==IDOK)self->save();else if(LOWORD(w)==Cancel || LOWORD(w)==IDCANCEL)DestroyWindow(window);return 0;}
        if(message==WM_DPICHANGED){const auto r=reinterpret_cast<RECT*>(l);SetWindowPos(window,nullptr,r->left,r->top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);self->scale(HIWORD(w));return 0;}
        if(message==WM_CLOSE){DestroyWindow(window);return 0;}
    } catch(const std::exception& error) {
        const int length=MultiByteToWideChar(CP_UTF8,0,error.what(),-1,nullptr,0);
        std::wstring detail(static_cast<std::size_t>(length),L'\0');
        if(length>0)MultiByteToWideChar(CP_UTF8,0,error.what(),-1,detail.data(),length);
        if(!detail.empty())detail.pop_back();
        self->error=L"保存失败："+detail;SendMessageW(self->item(Notice),WM_SETFONT,reinterpret_cast<WPARAM>(self->font),TRUE);SetWindowTextW(self->item(Notice),self->error.c_str());
    }
    return DefWindowProcW(window,message,w,l);
}
}
bool showInputSettings(HWND owner,const std::filesystem::path& path,int testMode) {
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_HOTKEY_CLASS|ICC_TAB_CLASSES};if(!InitCommonControlsEx(&common))throw std::runtime_error("Cannot initialize settings controls");
    Dialog dialog;dialog.path=path;
    const auto settings=tiger::readConfiguration(path);
    dialog.initial=tiger::parseEngineSettings(settings);dialog.initialStyle=tiger::parseCandidateStyle(settings);
    dialog.initialSentence=tiger::parseSentenceSettings(settings);
    WNDCLASSW type{};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=procedure;type.lpszClassName=L"NativeTigerInputSettings";type.hIcon=LoadIconW(type.hInstance,MAKEINTRESOURCEW(12));type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
    if(!RegisterClassW(&type) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Cannot register input settings window");
    if(!CreateWindowExW(WS_EX_CONTROLPARENT,type.lpszClassName,L"虎娘 · 输入设置",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,640,460,owner,nullptr,type.hInstance,&dialog))throw std::runtime_error("Cannot create input settings window");
    dialog.create();
    if(testMode) {
        const auto original=tiger::readConfiguration(path);
        wchar_t desktopName[256]{},inputName[256]{};DWORD needed=0;
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktopName,sizeof(desktopName),&needed);
        const auto inputDesktop=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
        if(inputDesktop){GetUserObjectInformationW(inputDesktop,UOI_NAME,inputName,sizeof(inputName),&needed);CloseDesktop(inputDesktop);}
        const bool isolatedCapture=std::wstring_view(desktopName).find(L"NativeTigerSettingsCapture_")==0 &&
            inputName[0] && std::wstring_view(desktopName)!=inputName;
        if(isolatedCapture){ShowWindow(dialog.window,SW_SHOWNOACTIVATE);UpdateWindow(dialog.window);}
        if(testMode==1) {
            const auto originalFont=dialog.fonts->selected();
            bool systemSeen=false;std::vector<std::wstring> names;
            std::ofstream catalog(path.parent_path()/L"font-catalog.tsv",std::ios::binary);
            for(const auto& item:dialog.fonts->items()) {
                if(item.bundled && systemSeen)throw std::runtime_error("Bundled fonts are not first");
                systemSeen=systemSeen || !item.bundled;
                for(const auto& alias:item.aliases) {
                    if(!dialog.fonts->select(alias) || dialog.fonts->selected()!=item.label)throw std::runtime_error("Font alias mismatch");
                }
                for(const auto& prior:names)if(CompareStringOrdinal(prior.c_str(),-1,item.label.c_str(),-1,TRUE)==CSTR_EQUAL)throw std::runtime_error("Duplicate font label");
                names.push_back(item.label);
                catalog<<tiger::utf8(std::u16string(reinterpret_cast<const char16_t*>(item.label.data()),item.label.size()))<<'\t'<<item.bundled<<'\n';
            }
            dialog.fonts->attach(dialog.item(FontName),L"NativeTiger missing font fixture");
            if(dialog.fonts->selected()!=dialog.fonts->items().front().label)throw std::runtime_error("Missing font did not fall back to first choice");
            dialog.fonts->select(originalFont);
        }
        if(dialog.fonts->previewSize()!=dialog.initialStyle.fontSize)throw std::runtime_error("Preview did not use saved font size");
        for(UINT dpi:{96u,144u,192u}) {
            dialog.scale(dpi);RECT client{};GetClientRect(dialog.window,&client);
            for(int page=0;page<TabCtrl_GetItemCount(dialog.item(Pages));++page) {
                TabCtrl_SetCurSel(dialog.item(Pages),page);
                NMHDR notification{dialog.item(Pages),Pages,TCN_SELCHANGE};
                SendMessageW(dialog.window,WM_NOTIFY,Pages,reinterpret_cast<LPARAM>(&notification));
                for(auto control:dialog.controls)if(control.page>=0 &&
                    (((GetWindowLongPtrW(control.window,GWL_STYLE)&WS_VISIBLE)!=0)!=(control.page==page)))
                    throw std::runtime_error("Native tab did not select the requested page");
            }
            for(auto control:dialog.controls) {
                RECT bounds{};GetWindowRect(control.window,&bounds);
                MapWindowPoints(nullptr,dialog.window,reinterpret_cast<POINT*>(&bounds),2);
                if(bounds.left<0 || bounds.top<0 || bounds.right>client.right || bounds.bottom>client.bottom)
                    throw std::runtime_error("Input settings control exceeds client bounds");
                if(control.page>=0) {
                    RECT pageBounds{};GetClientRect(dialog.content,&pageBounds);
                    MapWindowPoints(dialog.window,dialog.content,reinterpret_cast<POINT*>(&bounds),2);
                    if(GetParent(control.window)!=dialog.content || bounds.left<0 || bounds.top<0 ||
                        bounds.right>pageBounds.right || bounds.bottom>pageBounds.bottom)
                        throw std::runtime_error("Setting is outside its tab page container: "+std::to_string(GetDlgCtrlID(control.window)));
                }
            }
            if(testMode==1){
                dialog.page(0);UpdateWindow(dialog.window);dialog.captureSentence(path.parent_path()/(L"input-settings-"+std::to_wstring(dpi)+L".bmp"),0);
                if(dialog.fonts->items().size()<=3)throw std::runtime_error("System font catalog is incomplete");
                const auto originalFont=dialog.fonts->selected();
                if(!dialog.fonts->select(L"Segoe UI"))throw std::runtime_error("System font alias not recognized");
                if(dialog.fonts->selected()!=L"Segoe UI")throw std::runtime_error("System font alias resolves incorrectly");
                dialog.fonts->select(originalFont);
                dialog.page(1);UpdateWindow(dialog.window);dialog.captureSentence(path.parent_path()/(L"font-settings-"+std::to_wstring(dpi)+L".bmp"),1);
                if(isolatedCapture){SendMessageW(dialog.item(FontName),CB_SHOWDROPDOWN,TRUE,0);dialog.captureSentence(path.parent_path()/(L"font-dropdown-"+std::to_wstring(dpi)+L".bmp"),-2);SendMessageW(dialog.item(FontName),CB_SHOWDROPDOWN,FALSE,0);}
                dialog.page(4);UpdateWindow(dialog.window);dialog.captureSentence(path.parent_path()/(L"donation-settings-"+std::to_wstring(dpi)+L".bmp"),4);
                dialog.page(3);UpdateWindow(dialog.window);dialog.captureSentence(path.parent_path()/(L"sentence-settings-"+std::to_wstring(dpi)+L".bmp"));dialog.page(0);}
        }
        if(testMode==2) {
            if(dialog.initialStyle.codeMask!=u"甲😀乙")throw std::runtime_error("Mask setting did not reopen");
            SetWindowTextW(dialog.item(CodeMask),L"");
            if(dialog.initialStyle.candidateDelayMs!=250 || dialog.initialStyle.annotationDelayMs!=60000)
                throw std::runtime_error("Saved reveal delays did not reopen");
            SetWindowTextW(dialog.item(CandidateDelay),L"0");SetWindowTextW(dialog.item(AnnotationDelay),L"0");
            if(dialog.initialSentence.autoEnableBySchema || !dialog.initialSentence.autoCommit || dialog.initialSentence.allowDuplicateSingleCharacters ||
               dialog.text(SentenceRetained)!=u"32" || dialog.text(SentenceCommon)!=u"0" || !dialog.text(SentenceWhitelist).empty())
                throw std::runtime_error("Saved sentence settings did not reopen correctly");
            SetWindowTextW(dialog.item(SentenceRetained),L"5");SetWindowTextW(dialog.item(SentenceWhitelist),L"测试");
            SendMessageW(dialog.item(SentenceAuto),BM_SETCHECK,BST_UNCHECKED,0);
            SendMessageW(dialog.item(100),BM_SETCHECK,dialog.initial.defaultChinese?BST_UNCHECKED:BST_CHECKED,0);
            SetWindowTextW(dialog.item(MaxCode),L"2");
            SendMessageW(dialog.item(AddShortcut),HKM_SETHOTKEY,MAKEWORD('Z',HOTKEYF_CONTROL|HOTKEYF_ALT),0);
            dialog.fonts->select(dialog.fonts->items().front().label);SetWindowTextW(dialog.item(FontSize),L"20");
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
        SendMessageW(dialog.window,WM_COMMAND,SentencePage,0);
        if((GetWindowLongPtrW(dialog.item(MaxCode),GWL_STYLE)&WS_VISIBLE) || !(GetWindowLongPtrW(dialog.item(SentenceCommon),GWL_STYLE)&WS_VISIBLE))
            throw std::runtime_error("Sentence settings page switch failed");
        for(auto invalid:{L"33",L"-1",L"",L"999999999999999999999"}) {
            SetWindowTextW(dialog.item(SentenceRetained),invalid);SendMessageW(dialog.window,WM_COMMAND,Save,0);
            if(dialog.saved || !IsWindow(dialog.window) || tiger::readConfiguration(path)!=before)throw std::runtime_error("Invalid sentence retained raw saved");
        }
        SetWindowTextW(dialog.item(SentenceRetained),L"32");
        for(auto invalid:{L"2147483648",L"-1",L"",L"1.5"}) {
            SetWindowTextW(dialog.item(SentenceCommon),invalid);SendMessageW(dialog.window,WM_COMMAND,Save,0);
            if(dialog.saved || !IsWindow(dialog.window) || tiger::readConfiguration(path)!=before)throw std::runtime_error("Invalid sentence common limit saved");
        }
        SetWindowTextW(dialog.item(SentenceCommon),L"0");SetWindowTextW(dialog.item(SentenceWhitelist),L"");
        SendMessageW(dialog.item(SentenceEnabled),BM_SETCHECK,BST_UNCHECKED,0);
        SendMessageW(dialog.item(SentenceAuto),BM_SETCHECK,BST_CHECKED,0);
        SendMessageW(dialog.item(SentenceDuplicate),BM_SETCHECK,BST_UNCHECKED,0);
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
        SetWindowTextW(dialog.item(FontSize),L"17.5");
        for(int id:{CandidateDelay,AnnotationDelay}) {
            for(auto invalid:{L"60001",L"-1",L"1.5",L"999999999999"}) {
                SetWindowTextW(dialog.item(id),invalid);SendMessageW(dialog.window,WM_COMMAND,Save,0);
                if(dialog.saved || !IsWindow(dialog.window) || tiger::readConfiguration(path)!=before)throw std::runtime_error("Invalid reveal delay saved");
            }
            SetWindowTextW(dialog.item(id),L"");if(dialog.delay(id)!=0)throw std::runtime_error("Blank delay is not immediate");
            SetWindowTextW(dialog.item(id),L"0");if(dialog.delay(id)!=0)throw std::runtime_error("Zero delay is not immediate");
        }
        SetWindowTextW(dialog.item(CodeMask),L"甲😀乙");
        SetWindowTextW(dialog.item(CandidateDelay),L"250");SetWindowTextW(dialog.item(AnnotationDelay),L"60000");
        SetWindowTextW(dialog.item(FontSize),L"17.5");if(!dialog.fonts->select(L"Segoe UI"))throw std::runtime_error("System font missing from picker");
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
        if(message.message==WM_KEYDOWN && message.wParam==VK_TAB && (GetKeyState(VK_CONTROL)&0x8000)) {
            const int count=TabCtrl_GetItemCount(dialog.item(Pages));
            const int delta=(GetKeyState(VK_SHIFT)&0x8000)?count-1:1;
            dialog.page((TabCtrl_GetCurSel(dialog.item(Pages))+delta)%count);SetFocus(dialog.item(Pages));continue;
        }
        if(!IsDialogMessageW(dialog.window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
    }
    if(enabled){EnableWindow(owner,TRUE);SetActiveWindow(owner);}return dialog.saved;
}
