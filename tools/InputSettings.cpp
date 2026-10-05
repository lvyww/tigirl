#define NOMINMAX
#include "InputSettings.h"
#include "SettingsSkin.h"
#include "SettingsControls.h"
#include "SettingsPaintBuffer.h"
#include <windowsx.h>
#include "SelectionSettings.h"
#include "SelectionKeys.h"
#include "ConfigStore.h"
#include "Settings.h"
#include "CandidateTheme.h"
#include "SentenceSettings.h"
#include "FontChooser.h"
#include "Text.h"
#include "tsf/CandidateRenderer.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <locale>
#include <cmath>
#include <commctrl.h>
#include <iomanip>
#include <thread>
#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"version.lib")
namespace {
constexpr int Pages=221, Mascot=228, Minimize=229, Close=230;
constexpr int Donation=223,Preview=226,Dirty=227,VersionInfo=231;
constexpr int AnimationEnabled=224,AnimationDuration=225;
struct Flag {const char16_t* key;bool tiger::Config::*member;};
const Flag flags[]={
    {u"默认中文",&tiger::Config::defaultChinese},{u"shift切换中英文",&tiger::Config::shiftToggle},
    {nullptr,nullptr},{u"中文状态下使用英文标点",&tiger::Config::englishPunctuation},
    {u"/输出顿号",&tiger::Config::slashDunhao},{u"回车清屏",&tiger::Config::enterClear},
    {u"TAB清屏",&tiger::Config::tabClear},{u"空码自动清屏",&tiger::Config::clearOnNoCode},
    {u"最大码长无重自动上屏",&tiger::Config::maxCodeAutoCommit},{u"中英文不限长混合输入",&tiger::Config::mixedInput},
    {u"`键拼音反查",&tiger::Config::reverseLookup},{u"分号次选",&tiger::Config::semicolonSecond},
    {u"引号三选",&tiger::Config::quoteThird},{u"显示注释",&tiger::Config::showComment},{u"显示拆分",&tiger::Config::showSplit}};
constexpr int MaxCode=200,PageSize=201,Save=202,Cancel=203,Notice=204,PageKeys=205,InputPage=206,AppearancePage=207,FontName=208,FontSize=209,Theme=210,ShortcutPage=211,AddEnabled=212,AddShortcut=213,RecentEnabled=214,RecentShortcut=215,SelectionEditor=216;
struct StyleFlag {const char16_t* key;bool tiger::CandidateStyle::*member;};
const StyleFlag styleFlags[]={{u"竖排候选",&tiger::CandidateStyle::vertical},{u"显示候选序号",&tiger::CandidateStyle::showIndex},{u"候选窗显示编码",&tiger::CandidateStyle::showCode},{u"隐藏候选",&tiger::CandidateStyle::hideCandidates}};
constexpr int CandidateDelay=218,AnnotationDelay=219,CodeMask=220;
constexpr int SentencePage=217,SentenceEnabled=400,SentenceAuto=401,SentenceDuplicate=402,SentenceCommon=403,SentenceRetained=404,SentenceWhitelist=405,SentenceLearning=406;
const char16_t* pageKeys[]={u"- =",u"[ ]",u"Shift Tab/Tab",u"PageUp/PageDown"};
const wchar_t* wide(const char16_t* s){return reinterpret_cast<const wchar_t*>(s);}
// Read the running settings executable so this label follows release resources.
std::wstring applicationVersion() {
    wchar_t executable[32768]{};
    const auto length=GetModuleFileNameW(nullptr,executable,static_cast<DWORD>(std::size(executable)));
    if(!length || length>=std::size(executable))return L"版本信息不可用";
    DWORD unused=0;const DWORD size=GetFileVersionInfoSizeW(executable,&unused);
    if(!size)return L"版本信息不可用";
    std::vector<BYTE> bytes(size);
    if(!GetFileVersionInfoW(executable,0,size,bytes.data()))return L"版本信息不可用";
    VS_FIXEDFILEINFO* version=nullptr;UINT versionSize=0;
    if(!VerQueryValueW(bytes.data(),L"\\",reinterpret_cast<void**>(&version),&versionSize) ||
       versionSize<sizeof(VS_FIXEDFILEINFO) || version->dwSignature!=0xfeef04bd)return L"版本信息不可用";
    return L"虎娘 · 版本 "+std::to_wstring(HIWORD(version->dwProductVersionMS))+L"."+
        std::to_wstring(LOWORD(version->dwProductVersionMS))+L"."+
        std::to_wstring(HIWORD(version->dwProductVersionLS))+L"."+
        std::to_wstring(LOWORD(version->dwProductVersionLS));
}
struct Dialog {
    SettingsSkin skin;
    SettingsControls modern{skin};
    HBRUSH background=CreateSolidBrush(RGB(255,243,217));
    inline static bool testHighContrast=false;
    static bool highContrast(){if(testHighContrast)return true;HIGHCONTRASTW value{sizeof(value)};return SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(value),&value,0) && (value.dwFlags&HCF_HIGHCONTRASTON);}
    // Pages share the parent surface; an opaque card must not cover the frame.
    HBRUSH backgroundBrush()const{return highContrast()?GetSysColorBrush(COLOR_BTNFACE):background;}
    static COLORREF textColor(bool help=false){return highContrast()?GetSysColor(COLOR_BTNTEXT):(help?RGB(125,107,93):RGB(58,42,32));}
    HWND tooltip=nullptr;std::map<HWND,std::wstring> tipTexts;
    std::map<HWND,bool> buttonHot;
    HWND window=nullptr,content=nullptr;HFONT font=nullptr,helpFont=nullptr,titleFont=nullptr;std::filesystem::path path;
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
        const int savedDc=SaveDC(draw.hDC);FillRect(draw.hDC,&draw.rcItem,backgroundBrush());
        const int width=donationInfo.bmiHeader.biWidth,height=-donationInfo.bmiHeader.biHeight;
        const int availableWidth=draw.rcItem.right-draw.rcItem.left,availableHeight=draw.rcItem.bottom-draw.rcItem.top;
        int w=availableWidth,h=MulDiv(height,w,width);if(h>availableHeight){h=availableHeight;w=MulDiv(width,h,height);}
        SetStretchBltMode(draw.hDC,HALFTONE);SetBrushOrgEx(draw.hDC,0,0,nullptr);
        StretchDIBits(draw.hDC,draw.rcItem.left+(availableWidth-w)/2,draw.rcItem.top+(availableHeight-h)/2,w,h,0,0,width,height,donationPixels.data(),&donationInfo,DIB_RGB_COLORS,SRCCOPY);
        RestoreDC(draw.hDC,savedDc);
    }
    UINT currentDpi=96;int scroll[5]{},errorField=0;bool ready=false,previewFailed=false;unsigned previewCount=0;std::vector<std::u16string> baseline;std::wstring originalFontSelection;
    struct Control {HWND window;int x,y,w,h,page;bool help;bool title=false;};std::vector<Control> controls;
    ~Dialog(){if(IsWindow(window))DestroyWindow(window);if(font)DeleteObject(font);if(helpFont)DeleteObject(helpFont);if(titleFont)DeleteObject(titleFont);if(background)DeleteObject(background);}
    void drawTab(const DRAWITEMSTRUCT& draw) {
        const int savedDc=SaveDC(draw.hDC);
        const bool selected=static_cast<int>(draw.itemID)==TabCtrl_GetCurSel(item(Pages));
        const bool contrast=highContrast();
        SetDCBrushColor(draw.hDC,contrast?GetSysColor(selected?COLOR_HIGHLIGHT:COLOR_BTNFACE):(selected?RGB(255,172,76):RGB(255,243,217)));
        SelectObject(draw.hDC,GetStockObject(DC_BRUSH));SelectObject(draw.hDC,GetStockObject(DC_PEN));
        SetDCPenColor(draw.hDC,contrast?GetSysColor(COLOR_WINDOWTEXT):RGB(235,193,135));
        RoundRect(draw.hDC,draw.rcItem.left,draw.rcItem.top,draw.rcItem.right,draw.rcItem.bottom,px(10),px(10));
        if(selected && !contrast){
            const auto& r=draw.rcItem;SetDCBrushColor(draw.hDC,RGB(91,56,29));SelectObject(draw.hDC,GetStockObject(DC_BRUSH));SelectObject(draw.hDC,GetStockObject(NULL_PEN));
            POINT left[]={{r.left,r.top},{r.left+px(16),r.top},{r.left,r.top+px(20)}};Polygon(draw.hDC,left,3);
            POINT right[]={{r.right,r.bottom},{r.right-px(16),r.bottom},{r.right,r.bottom-px(22)}};Polygon(draw.hDC,right,3);
        }
        wchar_t title[80]{};TCITEMW tab{};tab.mask=TCIF_TEXT;tab.pszText=title;tab.cchTextMax=80;TabCtrl_GetItem(item(Pages),draw.itemID,&tab);
        SelectObject(draw.hDC,font);SetBkMode(draw.hDC,TRANSPARENT);
        SetTextColor(draw.hDC,contrast && selected?GetSysColor(COLOR_HIGHLIGHTTEXT):textColor());
        auto bounds=draw.rcItem;DrawTextW(draw.hDC,title,-1,&bounds,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        RestoreDC(draw.hDC,savedDc);
    }
    HWND item(int id){for(auto c:controls)if(GetDlgCtrlID(c.window)==id)return c.window;return nullptr;}
    static LRESULT CALLBACK contentProcedure(HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data) {
        auto self=reinterpret_cast<Dialog*>(data);
        if(message==WM_VSCROLL){self->scrollPage(LOWORD(w),HIWORD(w));return 0;}
        if(message==WM_MOUSEWHEEL){self->scrollPage(GET_WHEEL_DELTA_WPARAM(w)>0?SB_LINEUP:SB_LINEDOWN,0);return 0;}
        switch(message) {
        case WM_COMMAND:case WM_NOTIFY:case WM_DRAWITEM:case WM_MEASUREITEM:
        case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORBTN:case WM_CTLCOLORLISTBOX:
            return SendMessageW(GetParent(hwnd),message,w,l);
        }
        return DefSubclassProc(hwnd,message,w,l);
    }
    static LRESULT CALLBACK tabProcedure(HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data) {
        auto self=reinterpret_cast<Dialog*>(data);
        if(message==WM_PAINT || message==WM_PRINTCLIENT || message==WM_PRINT){
            PAINTSTRUCT paint{};HDC dc=message==WM_PAINT?BeginPaint(hwnd,&paint):reinterpret_cast<HDC>(w);RECT bounds{};GetClientRect(hwnd,&bounds);FillRect(dc,&bounds,self->backgroundBrush());
            for(int i=0;i<TabCtrl_GetItemCount(hwnd);++i){DRAWITEMSTRUCT draw{};draw.CtlID=Pages;draw.itemID=i;draw.hwndItem=hwnd;draw.hDC=dc;TabCtrl_GetItemRect(hwnd,i,&draw.rcItem);
                if(GetFocus()==hwnd && i==TabCtrl_GetCurSel(hwnd))draw.itemState|=ODS_FOCUS;
                if(SendMessageW(hwnd,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)draw.itemState|=ODS_NOFOCUSRECT;
                self->drawTab(draw);
            }
            if(message==WM_PAINT)EndPaint(hwnd,&paint);return 0;
        }
        return DefSubclassProc(hwnd,message,w,l);
    }
    static LRESULT CALLBACK buttonProcedure(HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data) {
        auto self=reinterpret_cast<Dialog*>(data);
        if(!highContrast() && (message==WM_PAINT || message==WM_PRINTCLIENT || message==WM_PRINT)){
            PAINTSTRUCT paint{};HDC dc=message==WM_PAINT?BeginPaint(hwnd,&paint):reinterpret_cast<HDC>(w);
            NMCUSTOMDRAW draw{};draw.hdr.hwndFrom=hwnd;draw.hdr.idFrom=GetDlgCtrlID(hwnd);draw.dwDrawStage=CDDS_PREPAINT;draw.hdc=dc;GetClientRect(hwnd,&draw.rc);
            if(GetFocus()==hwnd)draw.uItemState|=CDIS_FOCUS;
            if(SendMessageW(hwnd,BM_GETSTATE,0,0)&BST_PUSHED)draw.uItemState|=CDIS_SELECTED;
            POINT point{};GetCursorPos(&point);ScreenToClient(hwnd,&point);if(PtInRect(&draw.rc,point))draw.uItemState|=CDIS_HOT;
            self->drawButton(draw);if(message==WM_PAINT)EndPaint(hwnd,&paint);return 0;
        }
        if(!highContrast() && message==WM_ERASEBKGND)return 1;
        if(message==WM_MOUSEMOVE && !self->buttonHot[hwnd]){
            self->buttonHot[hwnd]=true;
            TRACKMOUSEEVENT tracking{sizeof(tracking),TME_LEAVE,hwnd,0};TrackMouseEvent(&tracking);
            InvalidateRect(hwnd,nullptr,FALSE);
        }
        if(message==WM_MOUSELEAVE)self->buttonHot[hwnd]=false;
        if(message==WM_NCDESTROY)self->buttonHot.erase(hwnd);
        if(message==WM_MOUSELEAVE || message==WM_ENABLE || message==WM_SETFOCUS || message==WM_KILLFOCUS)InvalidateRect(hwnd,nullptr,FALSE);
        return DefSubclassProc(hwnd,message,w,l);
    }
    static LRESULT CALLBACK focusAppearanceProcedure(HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR) {
        if(message==WM_UPDATEUISTATE || message==WM_CHANGEUISTATE){
            if(LOWORD(w)==UIS_CLEAR)w=MAKEWPARAM(UIS_CLEAR,HIWORD(w)&~UISF_HIDEFOCUS);
        }
        return DefSubclassProc(hwnd,message,w,l);
    }
    static void centerEdit(HWND hwnd) {
        RECT bounds{};GetClientRect(hwnd,&bounds);if(bounds.right<=4 || bounds.bottom<=0)return;
        HDC dc=GetDC(hwnd);auto old=SelectObject(dc,reinterpret_cast<HFONT>(SendMessageW(hwnd,WM_GETFONT,0,0)));
        TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);SelectObject(dc,old);ReleaseDC(hwnd,dc);
        const int lineHeight=std::max(1L,metrics.tmHeight);
        bounds.left+=2;bounds.right-=2;
        // Establish wrapping at the full width before measuring the visual line count.
        SendMessageW(hwnd,EM_SETRECTNP,0,reinterpret_cast<LPARAM>(&bounds));
        const int lines=static_cast<int>(SendMessageW(hwnd,EM_GETLINECOUNT,0,0));
        bounds.top=std::max(0L,(bounds.bottom-lines*lineHeight)/2);
        SendMessageW(hwnd,EM_SETRECTNP,0,reinterpret_cast<LPARAM>(&bounds));
        InvalidateRect(hwnd,nullptr,TRUE);
    }
    static LRESULT CALLBACK editAlignmentProcedure(HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR multiline) {
        if(!multiline && message==WM_CHAR && (w==L'\r' || w==L'\n'))return 0;
        if(!multiline && message==WM_PASTE && OpenClipboard(hwnd)){
            auto handle=GetClipboardData(CF_UNICODETEXT);auto source=handle?static_cast<const wchar_t*>(GlobalLock(handle)):nullptr;
            std::wstring value=source?source:L"";if(source)GlobalUnlock(handle);CloseClipboard();
            value.erase(std::remove_if(value.begin(),value.end(),[](wchar_t c){return c==L'\r' || c==L'\n';}),value.end());
            if(source)SendMessageW(hwnd,EM_REPLACESEL,TRUE,reinterpret_cast<LPARAM>(value.c_str()));return 0;
        }
        auto result=DefSubclassProc(hwnd,message,w,l);
        if(message==WM_SIZE || message==WM_SETFONT || message==WM_SETTEXT)centerEdit(hwnd);
        return result;
    }
    static LRESULT CALLBACK hotkeyAlignmentProcedure(HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data) {
        auto self=reinterpret_cast<Dialog*>(data);
        if(message==WM_ERASEBKGND)return 1;
        if(message==WM_PAINT || message==WM_PRINTCLIENT || message==WM_PRINT){
            PAINTSTRUCT paint{};HDC dc=message==WM_PAINT?BeginPaint(hwnd,&paint):reinterpret_cast<HDC>(w);RECT r{};GetClientRect(hwnd,&r);SettingsPaintBuffer buffer(dc,r);dc=buffer.dc();const int saved=SaveDC(dc);
            self->modern.field(dc,r,hwnd);
            const WORD key=static_cast<WORD>(SendMessageW(hwnd,HKM_GETHOTKEY,0,0));std::wstring text;
            if(HIBYTE(key)&HOTKEYF_CONTROL)text+=L"Ctrl + ";if(HIBYTE(key)&HOTKEYF_ALT)text+=L"Alt + ";if(HIBYTE(key)&HOTKEYF_SHIFT)text+=L"Shift + ";
            if(LOBYTE(key)){wchar_t name[80]{};LONG scan=static_cast<LONG>(MapVirtualKeyW(LOBYTE(key),MAPVK_VK_TO_VSC)<<16);if(HIBYTE(key)&HOTKEYF_EXT)scan|=1<<24;
                GetKeyNameTextW(scan,name,80);text+=name;}else text=L"无";
            SelectObject(dc,reinterpret_cast<HFONT>(SendMessageW(hwnd,WM_GETFONT,0,0)));SetBkMode(dc,TRANSPARENT);
            SetTextColor(dc,IsWindowEnabled(hwnd)?self->textColor():GetSysColor(COLOR_GRAYTEXT));InflateRect(&r,-3,-2);DrawTextW(dc,text.c_str(),-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            RestoreDC(dc,saved);buffer.present();if(message==WM_PAINT)EndPaint(hwnd,&paint);return 0;
        }
        auto result=DefSubclassProc(hwnd,message,w,l);
        if(message==WM_SETFOCUS)HideCaret(hwnd);
        if(message==WM_KEYDOWN || message==WM_KEYUP || message==WM_SYSKEYDOWN || message==WM_SYSKEYUP || message==HKM_SETHOTKEY || message==WM_ENABLE)InvalidateRect(hwnd,nullptr,TRUE);
        return result;
    }
    void control(int id,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h,bool help=false) {

        const bool edit=std::wstring_view(type)==L"EDIT",multiline=(style&ES_MULTILINE)!=0;
        style &= ~WS_TABSTOP;
        if(edit)style|=ES_CENTER|ES_MULTILINE;
        auto child=CreateWindowExW(type==std::wstring_view(L"EDIT")?WS_EX_CLIENTEDGE:0,type,text,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|style,x,y,w,h,buildingPage>=0?content:window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        if(!child)throw std::runtime_error("Cannot create input settings control");
        SetWindowSubclass(child,focusAppearanceProcedure,2,0);
        SendMessageW(child,WM_UPDATEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);
        controls.push_back({child,x,y,w,h,buildingPage,help});
        if(edit){SetWindowSubclass(child,editAlignmentProcedure,3,multiline);modern.attach(child,SettingsControls::Kind::Edit);}
        if(std::wstring_view(type)==HOTKEY_CLASSW)modern.attach(child,SettingsControls::Kind::Hotkey);
        if(std::wstring_view(type)==L"COMBOBOX"){
            modern.attach(child,SettingsControls::Kind::Combo);COMBOBOXINFO info{sizeof(info)};
            if(GetComboBoxInfo(child,&info))modern.attach(info.hwndList,SettingsControls::Kind::Popup);
        }
        if(std::wstring_view(type)==HOTKEY_CLASSW)SetWindowSubclass(child,hotkeyAlignmentProcedure,3,reinterpret_cast<DWORD_PTR>(this));
        if(std::wstring_view(type)==L"BUTTON" && (style&BS_TYPEMASK)!=BS_OWNERDRAW)
            if(!SetWindowSubclass(child,buttonProcedure,1,reinterpret_cast<DWORD_PTR>(this)))throw std::runtime_error("Cannot style native button");
    }
    int px(int n)const{return MulDiv(n,currentDpi,96);}
    RECT placement(const Control& c,int width) const {
        (void)width;
        const int id=GetDlgCtrlID(c.window);
        const int h=(id==FontName || id==Theme || id==PageKeys)?28:c.h;
        return {px(c.x),px(c.y),px(c.x+c.w),px(c.y+h)};
    }
    void layout() {
        if(!content)return;
        modern.contrast(highContrast());
        // Suppress intermediate paints and pixel copies while sibling controls move.
        // Once all positions are final, repaint the entire hierarchy, including EDIT borders.
        auto move=[](HWND child,int x,int y,int w,int h){SetWindowPos(child,nullptr,x,y,w,h,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOCOPYBITS);};
        RECT client{};GetClientRect(window,&client);
        move(item(Minimize),px(524),px(28),px(28),px(24));
        move(item(Close),px(562),px(28),px(28),px(24));
        move(item(Pages),px(16),px(56),px(608),px(36));
        TabCtrl_SetItemSize(item(Pages),px(604)/5,px(32));
        move(content,px(18),px(94),px(604),px(error.empty()?324:282));
        ShowWindow(item(Notice),error.empty()?SW_HIDE:SW_SHOW);
        move(item(Notice),px(32),px(378),px(576),px(40));
        move(item(Dirty),px(112),px(435),px(240),px(18));
        move(item(Save),px(372),px(428),px(128),px(28));
        move(item(Cancel),px(516),px(428),px(84),px(28));
        RECT pane{};GetClientRect(content,&pane);
        const int selected=TabCtrl_GetCurSel(item(Pages));if(selected<0)return;
        int height=0;for(auto c:controls)if(c.page==selected)height=std::max<int>(height,placement(c,pane.right).bottom+px(16));
        scroll[selected]=std::clamp(scroll[selected],0,std::max<int>(0,height-pane.bottom));
        SCROLLINFO info{sizeof(info),SIF_RANGE|SIF_PAGE|SIF_POS,0,height-1,static_cast<UINT>(pane.bottom),scroll[selected]};SetScrollInfo(content,SB_VERT,&info,FALSE);
        GetClientRect(content,&pane);
        for(auto c:controls)if(c.page>=0){
            // Logical columns shrink together on narrow work areas; pages scroll vertically.
            auto r=placement(c,pane.right);
            move(c.window,r.left,r.top-scroll[c.page],r.right-r.left,px(c.h));
        }
        RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN|RDW_UPDATENOW);
    }
    void scale(UINT dpi) {
        currentDpi=dpi;modern.scale(dpi,highContrast());
        auto make=[&](int size,int weight){return CreateFontW(-px(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,0,L"SimHei");};
        HFONT next=make(14,FW_NORMAL),help=make(12,FW_NORMAL),title=make(18,FW_BOLD);
        if(!next || !help || !title){if(next)DeleteObject(next);if(help)DeleteObject(help);if(title)DeleteObject(title);throw std::runtime_error("无法创建设置窗口字体。");}
        for(auto c:controls)SendMessageW(c.window,WM_SETFONT,reinterpret_cast<WPARAM>(c.title?title:(c.help?help:next)),TRUE);
        if(font)DeleteObject(font);if(helpFont)DeleteObject(helpFont);if(titleFont)DeleteObject(titleFont);
        font=next;helpFont=help;titleFont=title;
        if(tooltip){SendMessageW(tooltip,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);SendMessageW(tooltip,TTM_SETMAXTIPWIDTH,0,px(360));}
        if(fonts){fonts->scale(dpi);SendMessageW(item(FontName),CB_SETDROPPEDWIDTH,px(360),0);}
        MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
        RECT old{};GetWindowRect(window,&old);
        // Never shrink logical size on a small work area; keep the title reachable.
        const int left=std::clamp(old.left,monitor.rcWork.left,std::max(monitor.rcWork.left,monitor.rcWork.right-px(640)));
        const int top=std::clamp(old.top,monitor.rcWork.top,std::max(monitor.rcWork.top,monitor.rcWork.bottom-px(56)));
        SetWindowPos(window,nullptr,left,top,px(640),px(480),SWP_NOZORDER|SWP_NOACTIVATE);
        SetWindowRgn(window,SettingsSkin::region(dpi),TRUE);
        layout();
    }
    void scrollPage(int action,int /*position*/) {
        int selected=TabCtrl_GetCurSel(item(Pages));RECT r{};GetClientRect(content,&r);
        auto& offset=scroll[selected];
        if(action==SB_LINEUP)offset-=px(40);if(action==SB_LINEDOWN)offset+=px(40);
        if(action==SB_PAGEUP)offset-=r.bottom;if(action==SB_PAGEDOWN)offset+=r.bottom;
        if(action==SB_TOP)offset=0;if(action==SB_BOTTOM)offset=INT_MAX;
        if(action==SB_THUMBTRACK || action==SB_THUMBPOSITION){SCROLLINFO info{sizeof(info),SIF_TRACKPOS};GetScrollInfo(content,SB_VERT,&info);offset=info.nTrackPos;}
        layout();
    }
    void page(int selected) {
        for(auto c:controls)if(c.window==GetFocus() && c.page>=0 && c.page!=selected)SetFocus(item(Pages));
        TabCtrl_SetCurSel(item(Pages),selected);
        for(auto c:controls)ShowWindow(c.window,c.page<0 || c.page==selected?SW_SHOW:SW_HIDE);
        layout();RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
    }
    void reveal(HWND target) {
        for(auto c:controls)if(c.window==target && c.page>=0){
            if(TabCtrl_GetCurSel(item(Pages))!=c.page)page(c.page);
            RECT r{};GetClientRect(content,&r);
            const int previous=scroll[c.page];
            auto bounds=placement(c,r.right);
            if(bounds.top<scroll[c.page])scroll[c.page]=bounds.top;
            if(bounds.bottom>scroll[c.page]+r.bottom)scroll[c.page]=bounds.bottom-r.bottom;
            if(scroll[c.page]!=previous)layout();break;
        }
    }
    bool checked(int id){return SendMessageW(item(id),BM_GETCHECK,0,0)==BST_CHECKED;}
    std::vector<std::u16string> values() {
        std::vector<std::u16string> result;
        for(auto c:controls){int id=GetDlgCtrlID(c.window);if(c.page<0 || id<=0 || id==Preview || id==Mascot || id==Donation || id==VersionInfo || id==SelectionEditor)continue;
            wchar_t type[32]{};GetClassNameW(c.window,type,32);
            if(std::wstring_view(type)==L"Button")result.push_back(checked(id)?u"1":u"0");
            else if(id==AddShortcut || id==RecentShortcut)result.push_back(std::u16string(1,static_cast<char16_t>(SendMessageW(c.window,HKM_GETHOTKEY,0,0))));
            else result.push_back(text(id));
        }return result;
    }
    void update() {
        if(!ready)return;
        if(!error.empty()){error.clear();SetWindowTextW(item(Notice),L"");layout();}
        EnableWindow(item(AnimationDuration),checked(AnimationEnabled));
        EnableWindow(item(SentenceRetained),checked(SentenceAuto));
        EnableWindow(item(AddShortcut),checked(AddEnabled));EnableWindow(item(RecentShortcut),checked(RecentEnabled));
        EnableWindow(item(AnnotationDelay),checked(113)||checked(114));
        SetWindowTextW(item(Dirty),values()==baseline?L"未修改":L"有未保存的更改");
        InvalidateRect(item(Preview),nullptr,TRUE);
    }
    void drawPreview(const DRAWITEMSTRUCT& original) {
        auto draw=original;
        const int savedDc=SaveDC(draw.hDC);
        FillRect(draw.hDC,&draw.rcItem,backgroundBrush());
        try {
            auto style=initialStyle;auto name=fonts->selected();style.font.assign(reinterpret_cast<const char16_t*>(name.data()),name.size());
            style.fontSize=14; // Stable sample size; the saved candidate size is independent.
            for(int i=0;i<4;++i)style.*styleFlags[i].member=checked(300+i);
            auto theme=SendMessageW(item(Theme),CB_GETCURSEL,0,0);if(theme>=0 && theme<static_cast<LRESULT>(themes.size()))style.theme=themes[theme];
            style.codeMask=text(CodeMask);style.candidateDelayMs=0;
            tiger::Snapshot sample;sample.raw=u"ab";
            constexpr int count=3;
            for(int i=0;i<count;++i){tiger::Candidate candidate;candidate.display=i==0?u"你好":i==1?u"世界":u"输入";
                if(checked(113))candidate.annotation=u"示例注释";
                if(checked(114))candidate.annotation+=u" 人 尔";
                sample.candidates.push_back(candidate);
            }
            auto presentation=tiger::presentCandidates(sample,style);
            wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);std::vector<std::filesystem::path> files;
            auto directory=std::filesystem::path(executable).parent_path()/L"字体";
            if(std::filesystem::exists(directory))for(auto& entry:std::filesystem::directory_iterator(directory))if(entry.path().extension()==L".ttf" || entry.path().extension()==L".otf")files.push_back(entry.path());
            const int w=draw.rcItem.right-draw.rcItem.left,h=draw.rcItem.bottom-draw.rcItem.top;
            tiger::tsf::CandidateRenderer renderer(style,files);renderer.layout(presentation,static_cast<float>(w)*96/currentDpi);
            const SIZE frame{w,h};std::vector<std::uint32_t> pixels;renderer.render(currentDpi,0,pixels,&frame);
            // Composite the renderer's premultiplied surface over the native page color.
            LOGBRUSH brush{};GetObjectW(backgroundBrush(),sizeof(brush),&brush);
            for(auto& pixel:pixels){unsigned a=255-(pixel>>24);unsigned r=((pixel>>16)&255)+GetRValue(brush.lbColor)*a/255,g=((pixel>>8)&255)+GetGValue(brush.lbColor)*a/255,b=(pixel&255)+GetBValue(brush.lbColor)*a/255;pixel=0xff000000|(r<<16)|(g<<8)|b;}
            BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),w,-h,1,32,BI_RGB};
            ++previewCount;
            SetStretchBltMode(draw.hDC,HALFTONE);SetBrushOrgEx(draw.hDC,0,0,nullptr);
            StretchDIBits(draw.hDC,draw.rcItem.left,draw.rcItem.top,w,h,0,0,w,h,pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);
        } catch(...) {previewFailed=true;auto r=draw.rcItem;DrawTextW(draw.hDC,L"预览暂不可用；请检查字体。",-1,&r,DT_WORDBREAK);}
        RestoreDC(draw.hDC,savedDc);
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
        if(!list)paintFrame(dc);
        if(list)SendMessageW(list,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND);
        else for(auto c:controls)if((c.page<0 || c.page==selectedPage) && (GetWindowLongPtrW(c.window,GWL_STYLE)&WS_VISIBLE)) {
            RECT rect{};GetWindowRect(c.window,&rect);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&rect),2);
            // EDIT's nonclient WM_PRINT may replace the caller's clip region. Render each
            // actual control into its own surface, then clip the copy to the page viewport.
            const int width=rect.right-rect.left,height=rect.bottom-rect.top;
            HDC childDc=CreateCompatibleDC(dc);HBITMAP childBitmap=CreateCompatibleBitmap(dc,width,height);
            if(!childDc || !childBitmap){if(childDc)DeleteDC(childDc);if(childBitmap)DeleteObject(childBitmap);throw std::runtime_error("Cannot capture settings control");}
            auto childPrevious=SelectObject(childDc,childBitmap);RECT local{0,0,width,height};FillRect(childDc,&local,backgroundBrush());
            SendMessageW(c.window,WM_PRINT,reinterpret_cast<WPARAM>(childDc),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND);
            int state=SaveDC(dc);
            if(c.page>=0){RECT pane{};GetClientRect(content,&pane);MapWindowPoints(content,window,reinterpret_cast<POINT*>(&pane),2);IntersectClipRect(dc,pane.left,pane.top,pane.right,pane.bottom);}
            BitBlt(dc,rect.left,rect.top,width,height,childDc,0,0,SRCCOPY);
            RestoreDC(dc,state);SelectObject(childDc,childPrevious);DeleteObject(childBitmap);DeleteDC(childDc);
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
        std::wostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(1)<<value;return out.str();
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
        errorField=id;const auto value=static_cast<WORD>(SendMessageW(item(id),HKM_GETHOTKEY,0,0));
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
        control(Pages,WC_TABCONTROLW,L"",WS_TABSTOP|WS_CLIPSIBLINGS|TCS_OWNERDRAWFIXED|TCS_FIXEDWIDTH,8,8,744,38);
        SetWindowSubclass(item(Pages),tabProcedure,1,reinterpret_cast<DWORD_PTR>(this));
        for(const auto title:{L"输入行为",L"候选外观",L"按键与快捷键",L"整句输入",L"赞赏"}){TCITEMW tab{};tab.mask=TCIF_TEXT;tab.pszText=const_cast<wchar_t*>(title);TabCtrl_InsertItem(item(Pages),TabCtrl_GetItemCount(item(Pages)),&tab);}
        control(222,L"STATIC",L"",WS_CLIPCHILDREN|WS_VSCROLL,8,48,744,468);content=item(222);modern.attach(content,SettingsControls::Kind::Scroll);
        SetWindowLongPtrW(content,GWL_EXSTYLE,WS_EX_CONTROLPARENT);
        if(!SetWindowSubclass(content,contentProcedure,1,reinterpret_cast<DWORD_PTR>(this)))throw std::runtime_error("无法初始化设置页面。");
        auto title=[&](const wchar_t* text,int y){control(0,L"STATIC",text,0,24,y,672,24);controls.back().title=true;};
        auto label=[&](const wchar_t* text,int y,bool help=false){control(0,L"STATIC",text,0,24,y,672,help?32:24,help);};
        auto check=[&](int id,const wchar_t* text,bool value,int y,int x=24,int width=672){control(id,L"BUTTON",text,BS_AUTOCHECKBOX|WS_TABSTOP,x,y,width,28);SendMessageW(item(id),BM_SETCHECK,value?BST_CHECKED:BST_UNCHECKED,0);};
        auto flag=[&](int index,const wchar_t* text,int y,int x=24,int width=672){check(100+index,text,initial.*flags[index].member,y,x,width);};
        auto edit=[&](int id,const wchar_t* caption,const std::wstring& value,int y){control(0,L"STATIC",caption,0,24,y+4,id==MaxCode?176:344,24);control(id,L"EDIT",value.c_str(),ES_AUTOHSCROLL|WS_TABSTOP,id==MaxCode?216:376,y,id==CodeMask?200:(id==SentenceCommon?112:88),28);};
        buildingPage=0;
        flag(0,L"默认中文",20,24,320);flag(9,L"中英文不限长混合输入",20,376,320);
        flag(3,L"中文状态下使用英文标点",56,24,320);flag(4,L"斜杠 / 输出顿号",56,376,320);
        edit(MaxCode,L"最大码长（1–16）",std::to_wstring(initial.maxCodeLength),96);
        flag(8,L"达到最大码长且无重码时自动上屏",132);
        flag(7,L"空码自动清屏",168,24,320);flag(5,L"回车清屏",168,376,320);flag(6,L"Tab 清屏",204);
        buildingPage=1;
        control(0,L"STATIC",L"字体",0,24,24,72,24);
        wchar_t executable[32768]{};const auto length=GetModuleFileNameW(nullptr,executable,32768);
        if(!length || length>=32768)throw std::runtime_error("Cannot locate bundled fonts");
        fonts=std::make_unique<FontChooser>(std::filesystem::path(executable).parent_path()/L"字体",14);
        control(FontName,L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP|WS_VSCROLL,104,20,340,300);
        fonts->attach(item(FontName),wide(initialStyle.font.c_str()));

        control(0,L"STATIC",L"字号",0,464,24,48,24);
        control(FontSize,L"EDIT",sizeText(initialStyle.fontSize).c_str(),ES_AUTOHSCROLL|WS_TABSTOP,520,20,72,28);
        control(0,L"STATIC",L"主题",0,24,64,72,24);
        control(Theme,L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP|WS_VSCROLL,104,60,160,220);
        int themeIndex=-1;
        for(auto name:tiger::candidateThemeNames){if(name==initialStyle.theme)themeIndex=static_cast<int>(themes.size());themes.emplace_back(name);}
        if(themeIndex<0){themeIndex=static_cast<int>(themes.size());themes.push_back(initialStyle.theme);}
        for(const auto& name:themes)SendMessageW(item(Theme),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(name.c_str())));
        SendMessageW(item(Theme),CB_SETCURSEL,themeIndex,0);

        control(Mascot,L"STATIC",L"",SS_OWNERDRAW,424,0,158,90);
        control(Preview,L"STATIC",L"",SS_OWNERDRAW,376,60,320,180);
        check(300,L"竖排候选",initialStyle.vertical,104,24,160);check(301,L"显示候选序号",initialStyle.showIndex,104,200,168);
        check(302,L"显示编码",initialStyle.showCode,140,24,160);check(303,L"隐藏候选",initialStyle.hideCandidates,140,200,168);
        flag(13,L"显示注释",176,24,160);flag(14,L"显示拆分",176,200,168);
        control(0,L"STATIC",L"每页候选（1–10）",0,24,216,176,24);
        control(PageSize,L"EDIT",std::to_wstring(initial.pageSize).c_str(),ES_AUTOHSCROLL|WS_TABSTOP,208,212,72,28);
        control(0,L"STATIC",L"候选延时（毫秒）",0,24,260,224,24);
        control(CandidateDelay,L"EDIT",std::to_wstring(initialStyle.candidateDelayMs).c_str(),ES_AUTOHSCROLL|WS_TABSTOP,256,256,80,28);
        control(0,L"STATIC",L"注释／拆分延时",0,376,260,224,24);
        control(AnnotationDelay,L"EDIT",std::to_wstring(initialStyle.annotationDelayMs).c_str(),ES_AUTOHSCROLL|WS_TABSTOP,616,256,80,28);
        check(AnimationEnabled,L"候选窗动效",initialStyle.animationEnabled,296,24,208);
        control(0,L"STATIC",L"动效时长（毫秒）",0,248,300,184,24);
        control(AnimationDuration,L"EDIT",std::to_wstring(initialStyle.animationDurationMs).c_str(),ES_AUTOHSCROLL|WS_TABSTOP,448,296,88,28);
        control(0,L"STATIC",L"编码伪装（留空关闭）",0,24,340,224,24);
        control(CodeMask,L"EDIT",wide(initialStyle.codeMask.c_str()),ES_AUTOHSCROLL|WS_TABSTOP,256,336,200,28);
        buildingPage=2;
        flag(1,L"Shift 切换中英文",20,24,280);flag(10,L"反引号 ` 键拼音反查",20,376,320);
        control(0,L"STATIC",L"翻页键（上一页／下一页）",0,24,132,224,24);
        control(PageKeys,L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP|WS_VSCROLL,256,128,240,160);
        for(auto keys:pageKeys)SendMessageW(item(PageKeys),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(keys)));SendMessageW(item(PageKeys),CB_SETCURSEL,initial.pageKeys,0);
        flag(11,L"分号次选",168,24,192);flag(12,L"引号三选",168,240,192);
        control(SelectionEditor,L"BUTTON",L"编辑选重键…",WS_TABSTOP,472,166,168,32);
        check(AddEnabled,L"启用手动加词",initial.addWordEnabled,252,24,224);control(AddShortcut,HOTKEY_CLASSW,L"",WS_TABSTOP,256,250,240,32);
        check(RecentEnabled,L"启用最近方案切换",initial.recentSchemaEnabled,292,24,224);control(RecentShortcut,HOTKEY_CLASSW,L"",WS_TABSTOP,256,290,240,32);
        SendMessageW(item(AddShortcut),HKM_SETHOTKEY,hotkey(initial.addWordShortcut),0);SendMessageW(item(RecentShortcut),HKM_SETHOTKEY,hotkey(initial.recentSchemaShortcut),0);
        buildingPage=3;
        check(SentenceEnabled,L"方案名称包含“整句”时自动启用整句模式",initialSentence.autoEnableBySchema,20);
        check(SentenceAuto,L"自动提前上屏已确定的句子前缀",initialSentence.autoCommit,56);
        edit(SentenceRetained,L"提前上屏后最少保留编码（0–32）",std::to_wstring(initialSentence.minimumRetainedRaw),96);
        check(SentenceDuplicate,L"允许单字重码组句",initialSentence.allowDuplicateSingleCharacters,136);
        edit(SentenceCommon,L"仅使用最优码组句的高频字数量",std::to_wstring(initialSentence.commonCharacterLimit),176);
        label(L"允许全码组句的例外字符（可留空）",248);
        control(SentenceWhitelist,L"EDIT",wide(initialSentence.fullCodeWhitelist.c_str()),ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|WS_TABSTOP,24,280,672,80);
        SendMessageW(item(SentenceWhitelist),EM_SETLIMITTEXT,4*1024*1024,0);
        check(SentenceLearning,L"Tab 锁重上屏后自学习（仅本方案、本地保存）",initialSentence.selfLearning,408);
        buildingPage=4;loadDonation();title(L"感谢你对虎娘的支持",24);
        control(Donation,L"STATIC",L"赞赏码",SS_OWNERDRAW,200,72,320,320);
        control(VersionInfo,L"STATIC",applicationVersion().c_str(),SS_CENTER|SS_CENTERIMAGE,16,298,560,24,true);
        buildingPage=-1;
        control(Notice,L"STATIC",L"",0,24,524,712,40,true);
        control(Dirty,L"STATIC",L"未修改",0,24,580,400,24,true);
        control(Save,L"BUTTON",L"保存并关闭",BS_DEFPUSHBUTTON|WS_TABSTOP,496,576,136,32);control(Cancel,L"BUTTON",L"取消",WS_TABSTOP,648,576,88,32);
        control(Minimize,L"BUTTON",L"最小化",BS_OWNERDRAW,548,22,30,28);
        control(Close,L"BUTTON",L"关闭",BS_OWNERDRAW,586,22,30,28);
        tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,
            CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,window,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!tooltip)throw std::runtime_error("Cannot create settings tips");
        modern.attach(tooltip,SettingsControls::Kind::Tip);
        SendMessageW(tooltip,TTM_SETMAXTIPWIDTH,0,420);
        auto tip=[&](int id,const wchar_t* text){
            HWND target=item(id);tipTexts[target]=text;
            if(id==Preview || id==Donation)SetWindowLongPtrW(target,GWL_STYLE,GetWindowLongPtrW(target,GWL_STYLE)|SS_NOTIFY);
            TOOLINFOW info{};info.cbSize=TTTOOLINFOW_V2_SIZE;info.uFlags=TTF_IDISHWND|TTF_SUBCLASS;info.hwnd=GetParent(target);
            info.uId=reinterpret_cast<UINT_PTR>(target);info.lpszText=tipTexts[target].data();
            if(!SendMessageW(tooltip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info)))throw std::runtime_error("Cannot register settings tip: "+std::to_string(id));
        };
        for(int id:{105,106,107})tip(id,L"清屏：清除未上屏编码，不删除已经输入的文字。");
        tip(FontSize,L"字号范围 3–200；保存后用于实际候选窗。此处预览固定字号，只展示字体样式。");
        tip(Preview,L"固定大小的字体、主题与排列示例，不随字号或每页候选数量缩放。");
        for(int id:{CandidateDelay,AnnotationDelay,AnimationEnabled,AnimationDuration})tip(id,L"延时和动效时长：0–60000 毫秒；延时为 0 立即显示。预览不播放延时和动效。");
        tip(CodeMask,L"编码伪装如填 ●，ab 显示为 ●●；不改变实际查码和上屏文字。留空关闭。");
        tip(SelectionEditor,L"选重键在独立窗口中单独保存，不受本窗口的取消影响。");
        for(int id:{AddEnabled,AddShortcut,RecentEnabled,RecentShortcut})tip(id,L"点击快捷键框后直接按键；组合键需包含 Ctrl 或 Alt。");
        tip(SentenceCommon,L"范围 0–2147483647；默认 1500；0 表示不按高频字限制全码。");
        tip(SentenceWhitelist,L"直接填写汉字，无需分隔；换行保存时自动合并为单行字符列表。");
        tip(SentenceLearning,L"Tab 锁重上屏后自学习；仅本方案、本地保存。");
        tip(Donation,L"微信扫码赞赏，自愿支持。");
        arrange();
        scale(GetDpiForWindow(window));page(0);baseline=values();originalFontSelection=fonts->selected();ready=true;update();
    }
    void arrange() {
        // The five pages use fixed logical coordinates; only their viewport scrolls.
        for(auto& c:controls)if(c.page>=0 && c.page!=1){c.x=MulDiv(c.x,584,720);c.w=MulDiv(c.w,584,720);}
        auto place=[&](int id,int x,int y,int w,int h){for(auto& c:controls)if(GetDlgCtrlID(c.window)==id){c.x=x;c.y=y;c.w=w;c.h=h;}};
        for(auto& c:controls)if(c.page==1){
            const int id=GetDlgCtrlID(c.window);
            if(id==0){
                if(c.y==24 && c.x==24){c.x=12;c.y=12;c.w=36;}
                else if(c.y==24 && c.x==464){c.x=220;c.y=12;c.w=32;}
                else if(c.y==24){c.x=254;c.y=42;c.w=60;}
                else if(c.y==64){c.x=12;c.y=48;c.w=36;}
                else if(c.y==216){c.x=12;c.y=192;c.w=130;}
                else if(c.y==260){c.x=c.x==24?12:308;c.y=234;c.w=130;}
                else if(c.y==300){c.x=144;c.y=272;c.w=108;SetWindowTextW(c.window,L"时长（毫秒）");}
                else if(c.y==340){c.x=342;c.y=272;c.w=130;SetWindowTextW(c.window,L"编码伪装");}
                else if(c.y==376){c.x=12;c.y=316;c.w=560;c.h=32;}
                else if(c.y==408){c.x=12;c.y=352;c.w=560;c.h=32;}
            }
        }
        for(auto& c:controls)if(c.page==2 && c.y>=128)c.y-=c.y>=250?96:64;
        place(FontName,52,8,160,240);place(FontSize,254,8,60,28);place(Theme,52,44,160,220);
        place(300,12,84,140,28);place(301,164,84,156,28);place(302,12,120,140,28);place(303,164,120,156,28);
        place(113,12,156,140,28);place(114,164,156,156,28);place(PageSize,158,188,60,28);
        place(Mascot,440,0,120,66);place(Preview,332,68,244,148);
        place(CandidateDelay,150,230,64,28);place(AnnotationDelay,452,230,64,28);
        place(AnimationEnabled,12,268,130,28);place(AnimationDuration,254,268,64,28);place(CodeMask,476,268,98,28);
        for(auto& c:controls)if(c.page==3 && c.w==MulDiv(344,584,720))c.w=342;
        place(SentenceRetained,368,96,80,28);place(SentenceCommon,368,176,112,28);
        place(Donation,177,38,250,250);
        place(VersionInfo,16,292,572,16);
        for(auto& c:controls)if(c.page==4 && GetDlgCtrlID(c.window)==0){c.y=c.title?8:298;c.x=16;c.w=560;c.h=24;}
    }
    void paintFrame(HDC dc) {
        RECT r{};GetClientRect(window,&r);skin.frame(dc,r,currentDpi,highContrast());
        const int state=SaveDC(dc);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,textColor());SelectObject(dc,titleFont?titleFont:font);
        RECT title{px(84),px(23),px(240),px(51)};DrawTextW(dc,L"虎娘 · 输入设置",-1,&title,DT_SINGLELINE|DT_VCENTER);
        DrawIconEx(dc,px(46),px(20),LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(12)),px(32),px(32),0,nullptr,DI_NORMAL);
        RestoreDC(dc,state);
    }
    LRESULT drawButton(NMCUSTOMDRAW draw) {
        if(draw.dwDrawStage!=CDDS_PREPAINT || highContrast())return CDRF_DODEFAULT;
        SettingsPaintBuffer buffer(draw.hdc,draw.rc);draw.hdc=buffer.dc();
        const HWND button=draw.hdr.hwndFrom;const int id=GetDlgCtrlID(button);
        const bool checkbox=(GetWindowLongPtrW(button,GWL_STYLE)&BS_TYPEMASK)==BS_AUTOCHECKBOX;
        const bool enabled=IsWindowEnabled(button)!=FALSE,down=(draw.uItemState&CDIS_SELECTED)!=0,hot=(draw.uItemState&CDIS_HOT)!=0;
        const int state=SaveDC(draw.hdc);
        FillRect(draw.hdc,&draw.rc,backgroundBrush());
        RECT box=draw.rc;if(checkbox){box.left+=px(1);box.top+=(box.bottom-box.top-px(20))/2;box.right=box.left+px(20);box.bottom=box.top+px(20);}
        const bool checkedState=SendMessageW(button,BM_GETCHECK,0,0)==BST_CHECKED;
        const COLORREF fill=!enabled?RGB(242,232,214):(checkbox?(checkedState?RGB(247,151,54):RGB(255,250,240)):(id==Save?(down?RGB(235,135,40):hot?RGB(255,173,72):RGB(250,158,56)):(hot?RGB(255,235,198):RGB(255,250,240))));
        skin.surface(draw.hdc,box,currentDpi,fill,!enabled?RGB(222,208,186):hot?RGB(235,148,49):RGB(210,175,129),checkbox?5.f:7.f,1.f);
        if(checkbox && checkedState){auto mark=CreatePen(PS_SOLID,px(2),enabled?RGB(255,255,255):GetSysColor(COLOR_GRAYTEXT));auto old=SelectObject(draw.hdc,mark);MoveToEx(draw.hdc,box.left+px(4),box.top+px(10),nullptr);LineTo(draw.hdc,box.left+px(8),box.top+px(14));LineTo(draw.hdc,box.left+px(16),box.top+px(5));SelectObject(draw.hdc,old);DeleteObject(mark);}
        RECT label=draw.rc;if(checkbox)label.left=box.right+px(10);
        wchar_t text[256]{};GetWindowTextW(button,text,256);SelectObject(draw.hdc,font);SetBkMode(draw.hdc,TRANSPARENT);SetTextColor(draw.hdc,enabled?textColor():GetSysColor(COLOR_GRAYTEXT));
        DrawTextW(draw.hdc,text,-1,&label,DT_SINGLELINE|DT_VCENTER|(checkbox?DT_LEFT:DT_CENTER));
        RestoreDC(draw.hdc,state);buffer.present();return CDRF_SKIPDEFAULT;
    }
    void drawCaptionButton(const DRAWITEMSTRUCT& draw) {
        const int state=SaveDC(draw.hDC);FillRect(draw.hDC,&draw.rcItem,backgroundBrush());
        SelectObject(draw.hDC,titleFont);SetBkMode(draw.hDC,TRANSPARENT);SetTextColor(draw.hDC,textColor());auto r=draw.rcItem;
        DrawTextW(draw.hDC,draw.CtlID==Close?L"×":L"−",-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        RestoreDC(draw.hDC,state);
    }
    int number(int id,int limit) {
        errorField=id;wchar_t value[32]{};const int n=GetWindowTextW(item(id),value,32);int result=0;
        if(!n || n>2)throw std::runtime_error((id==MaxCode?"最大码长必须为 1–16 的整数。":"每页候选数量必须为 1–10 的整数。"));
        for(int i=0;i<n;++i){if(value[i]<L'0'||value[i]>L'9')throw std::runtime_error("请输入整数。");result=result*10+value[i]-L'0';}
        if(result<1 || result>limit)throw std::runtime_error((id==MaxCode?"最大码长必须为 1–16 的整数。":"每页候选数量必须为 1–10 的整数。"));return result;
    }
    int delay(int id) {
        errorField=id;const auto value=text(id);if(value.empty())return 0;
        if(value.size()>5)throw std::runtime_error("延时必须为 0–60000 毫秒。");
        int result=0;for(auto c:value){if(c<u'0' || c>u'9')throw std::runtime_error("延时必须为整数。");result=result*10+c-u'0';}
        if(result>60000)throw std::runtime_error("延时必须为 0–60000 毫秒。");return result;
    }
    void save() {
        if(values()==baseline){saved=true;DestroyWindow(window);return;}
        std::vector<std::pair<std::u16string,std::u16string>> changes;
        for(int i=0;i<static_cast<int>(std::size(flags));++i) {
            if(!flags[i].member)continue;
            bool value=SendMessageW(item(100+i),BM_GETCHECK,0,0)==BST_CHECKED;
            if(value!=initial.*flags[i].member)changes.emplace_back(flags[i].key,value?u"是":u"否");
        }
        const int max=number(MaxCode,16),page=number(PageSize,10);
        auto numberText=[](int n){auto value=std::to_wstring(n);return std::u16string(reinterpret_cast<const char16_t*>(value.data()),value.size());};
        if(max!=initial.maxCodeLength)changes.emplace_back(u"最大码长",numberText(max));
        if(page!=initial.pageSize)changes.emplace_back(u"每页候选个数",numberText(page));
        errorField=PageKeys;const auto selected=SendMessageW(item(PageKeys),CB_GETCURSEL,0,0);
        if(selected<0 || selected>=static_cast<LRESULT>(std::size(pageKeys)))throw std::runtime_error("请选择翻页键。");
        if(selected!=initial.pageKeys)changes.emplace_back(u"翻页键",pageKeys[selected]);
        for(int i=0;i<4;++i) {
            const bool value=SendMessageW(item(300+i),BM_GETCHECK,0,0)==BST_CHECKED;
            if(value!=initialStyle.*styleFlags[i].member)changes.emplace_back(styleFlags[i].key,value?u"是":u"否");
        }
        const auto mask=text(CodeMask);
        if(mask!=initialStyle.codeMask)changes.emplace_back(u"编码伪装",mask);
        errorField=FontName;const auto selectedFont=fonts->selected();
        auto name=std::u16string(reinterpret_cast<const char16_t*>(selectedFont.data()),selectedFont.size());
        name=tiger::configurationValue(u"字体\t"+name,u"字体");
        if(name.empty())throw std::runtime_error("请选择候选字体。");
        if(selectedFont!=originalFontSelection && name!=initialStyle.font)changes.emplace_back(u"字体",name);
        errorField=FontSize;const auto sizeValue=text(FontSize);
        std::string ascii;for(auto c:sizeValue){if(c>127)throw std::runtime_error("字号必须为 3–200 的数字。");ascii+=static_cast<char>(c);}
        std::istringstream input(ascii);input.imbue(std::locale::classic());double size=0;
        if(!(input>>size) || input.peek()!=std::char_traits<char>::eof() || !std::isfinite(size) || size<3 || size>200)
            throw std::runtime_error("字号必须为 3–200 的数字。");
        const bool animation=SendMessageW(item(AnimationEnabled),BM_GETCHECK,0,0)==BST_CHECKED;
        const auto duration=text(AnimationDuration).empty()?200:delay(AnimationDuration);
        if(animation!=initialStyle.animationEnabled)changes.emplace_back(u"候选窗动效",animation?u"是":u"否");
        if(duration!=initialStyle.animationDurationMs)changes.emplace_back(u"候选窗动效时间(毫秒)",numberText(duration));
        const auto candidateDelay=delay(CandidateDelay),annotationDelay=delay(AnnotationDelay);
        if(candidateDelay!=initialStyle.candidateDelayMs)changes.emplace_back(u"延时显示候选(毫秒)",numberText(candidateDelay));
        if(annotationDelay!=initialStyle.annotationDelayMs)changes.emplace_back(u"延时展开注释和拆分(毫秒)",numberText(annotationDelay));
        if(sizeValue!=std::u16string(reinterpret_cast<const char16_t*>(sizeText(initialStyle.fontSize).c_str())) && size!=initialStyle.fontSize)changes.emplace_back(u"字体大小",sizeValue);
        errorField=Theme;const auto selectedTheme=SendMessageW(item(Theme),CB_GETCURSEL,0,0);
        if(selectedTheme<0 || static_cast<std::size_t>(selectedTheme)>=themes.size())throw std::runtime_error("请选择候选主题。");
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
                        std::make_pair(SentenceLearning,std::make_pair(u"整句Tab自学习",initialSentence.selfLearning)),
                        std::make_pair(SentenceDuplicate,std::make_pair(u"允许单字重码组句",initialSentence.allowDuplicateSingleCharacters))}) {
            const bool value=SendMessageW(item(entry.first),BM_GETCHECK,0,0)==BST_CHECKED;
            if(value!=entry.second.second)changes.emplace_back(entry.second.first,value?u"是":u"否");
        }
        auto nonnegative=[&](int id,unsigned limit) {
            errorField=id;const auto value=text(id);unsigned result=0;
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
        auto whitelist=text(SentenceWhitelist);
        whitelist.erase(std::remove_if(whitelist.begin(),whitelist.end(),[](char16_t c){return c==u'\r' || c==u'\n';}),whitelist.end());
        if(whitelist!=initialSentence.fullCodeWhitelist)changes.emplace_back(u"整句允许全码组句白名单",whitelist);
        errorField=AddShortcut;
        tiger::saveInputConfiguration(path,changes,[&](std::u16string_view updated) {
            const auto effective=tiger::parseEngineSettings(updated);
            errorField=(recent && !effective.recentSchemaEnabled && (!add || effective.addWordEnabled))?RecentShortcut:AddShortcut;
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
        if(message==WM_NCCALCSIZE)return 0;
        if(message==WM_NCHITTEST){POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(window,&point);
            if(point.y<self->px(56) && point.x<self->px(516))return HTCAPTION;return HTCLIENT;}
        if(message==WM_NCRBUTTONUP && w==HTCAPTION){auto command=TrackPopupMenu(GetSystemMenu(window,FALSE),TPM_RETURNCMD|TPM_RIGHTBUTTON,GET_X_LPARAM(l),GET_Y_LPARAM(l),0,window,nullptr);if(command)SendMessageW(window,WM_SYSCOMMAND,command,0);return 0;}
        if(message==WM_SYSCOMMAND && ((w&0xfff0)==SC_SIZE || (w&0xfff0)==SC_MAXIMIZE))return 0;
        if(message==WM_ERASEBKGND){self->paintFrame(reinterpret_cast<HDC>(w));return 1;}
        if(message==WM_PAINT){PAINTSTRUCT paint{};auto dc=BeginPaint(window,&paint);self->paintFrame(dc);EndPaint(window,&paint);return 0;}
        if(message==WM_PRINTCLIENT){self->paintFrame(reinterpret_cast<HDC>(w));return 0;}
        if(message==WM_NOTIFY && reinterpret_cast<NMHDR*>(l)->code==NM_CUSTOMDRAW && reinterpret_cast<NMHDR*>(l)->idFrom!=Pages && reinterpret_cast<NMHDR*>(l)->hwndFrom!=self->tooltip)
            return self->drawButton(*reinterpret_cast<NMCUSTOMDRAW*>(l));
        if(message==WM_DRAWITEM && (w==Minimize || w==Close)){self->drawCaptionButton(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}
        if(message==WM_DRAWITEM && w==Mascot){auto draw=reinterpret_cast<DRAWITEMSTRUCT*>(l);self->skin.mascot(draw->hDC,draw->rcItem,self->currentDpi,self->highContrast());return TRUE;}
        if(message==WM_COMMAND && LOWORD(w)==Minimize){ShowWindow(window,SW_MINIMIZE);return 0;}
        if(message==WM_COMMAND && LOWORD(w)==Close){DestroyWindow(window);return 0;}
        if(message==WM_SETTINGCHANGE || message==WM_SYSCOLORCHANGE || message==WM_THEMECHANGED){
            self->modern.contrast(self->highContrast());
            RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN);
        }
        if(message==WM_CTLCOLORSTATIC) {
            const auto dc=reinterpret_cast<HDC>(w);
            wchar_t childType[24]{};GetClassNameW(reinterpret_cast<HWND>(l),childType,24);
            if(std::wstring_view(childType)==L"Edit"){SetTextColor(dc,GetSysColor(COLOR_GRAYTEXT));SetBkColor(dc,self->modern.fieldColor(reinterpret_cast<HWND>(l)));return reinterpret_cast<LRESULT>(self->modern.fieldBrush(reinterpret_cast<HWND>(l)));}
            bool help=false;for(auto c:self->controls)if(c.window==reinterpret_cast<HWND>(l)){help=c.help;break;}
            const bool failure=reinterpret_cast<HWND>(l)==self->item(Notice) && !self->error.empty();
            const auto brush=self->backgroundBrush();LOGBRUSH colors{};GetObjectW(brush,sizeof(colors),&colors);
            SetTextColor(dc,IsWindowEnabled(reinterpret_cast<HWND>(l))?self->textColor(help && !failure):GetSysColor(COLOR_GRAYTEXT));SetBkColor(dc,colors.lbColor);
            return reinterpret_cast<LRESULT>(brush);
        }
        if(message==WM_CTLCOLOREDIT || message==WM_CTLCOLORLISTBOX){const auto dc=reinterpret_cast<HDC>(w);SetTextColor(dc,Dialog::highContrast()?GetSysColor(COLOR_WINDOWTEXT):self->textColor());SetBkColor(dc,self->modern.fieldColor(reinterpret_cast<HWND>(l)));return reinterpret_cast<LRESULT>(self->modern.fieldBrush(reinterpret_cast<HWND>(l)));}
        if(message==WM_NOTIFY && reinterpret_cast<NMHDR*>(l)->idFrom==Pages && reinterpret_cast<NMHDR*>(l)->code==TCN_SELCHANGE) {
            self->page(TabCtrl_GetCurSel(self->item(Pages)));return 0;
        }
        if(message==WM_DRAWITEM && w==Pages){self->drawTab(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}
        if(message==WM_DRAWITEM && w==Donation){self->drawDonation(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}
        if(message==WM_DRAWITEM && w==FontName && self->fonts){self->fonts->draw(*reinterpret_cast<DRAWITEMSTRUCT*>(l),self->highContrast());return TRUE;}
        if(message==WM_DRAWITEM && (w==Theme || w==PageKeys)){self->modern.comboItem(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}
        if(message==WM_MEASUREITEM && (w==Theme || w==PageKeys)){reinterpret_cast<MEASUREITEMSTRUCT*>(l)->itemHeight=self->px(28);return TRUE;}
        if(message==WM_MEASUREITEM && w==FontName && self->fonts){self->fonts->measure(*reinterpret_cast<MEASUREITEMSTRUCT*>(l));return TRUE;}
        if(message==WM_SIZE){self->layout();return 0;}
        if(message==WM_MOUSEWHEEL){self->scrollPage(GET_WHEEL_DELTA_WPARAM(w)>0?SB_LINEUP:SB_LINEDOWN,0);return 0;}
        if(message==WM_GETMINMAXINFO){auto limits=reinterpret_cast<MINMAXINFO*>(l);limits->ptMinTrackSize={self->px(640),self->px(480)};limits->ptMaxTrackSize=limits->ptMinTrackSize;return 0;}
        if(message==WM_DRAWITEM && w==Preview){self->drawPreview(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}
        if(message==WM_COMMAND && self->ready){
            if(HIWORD(w)==EN_SETFOCUS || HIWORD(w)==BN_SETFOCUS)self->reveal(reinterpret_cast<HWND>(l));
            if(HIWORD(w)==EN_CHANGE && l){wchar_t type[24]{};GetClassNameW(reinterpret_cast<HWND>(l),type,24);if(std::wstring_view(type)==L"Edit")self->centerEdit(reinterpret_cast<HWND>(l));}
            if(HIWORD(w)==BN_CLICKED || HIWORD(w)==EN_CHANGE || HIWORD(w)==CBN_SELCHANGE){self->update();}
        }
        if(message==WM_COMMAND && LOWORD(w)==SentencePage){self->page(3);return 0;}
        if(message==WM_COMMAND){if(LOWORD(w)==SelectionEditor){showSelectionSettings(window,self->path.parent_path()/L"自定义选重键.txt");return 0;}if(LOWORD(w)==ShortcutPage){self->page(2);return 0;}if(LOWORD(w)==InputPage){self->page(0);return 0;}if(LOWORD(w)==AppearancePage){self->page(1);return 0;}if(LOWORD(w)==Save || LOWORD(w)==IDOK)self->save();else if(LOWORD(w)==Cancel || LOWORD(w)==IDCANCEL)DestroyWindow(window);return 0;}
        if(message==WM_DPICHANGED){const auto r=reinterpret_cast<RECT*>(l);SetWindowPos(window,nullptr,r->left,r->top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);self->scale(HIWORD(w));return 0;}
        if(message==WM_CLOSE){DestroyWindow(window);return 0;}
    } catch(const std::exception& error) {
        const int length=MultiByteToWideChar(CP_UTF8,0,error.what(),-1,nullptr,0);
        std::wstring detail(static_cast<std::size_t>(length),L'\0');
        if(length>0)MultiByteToWideChar(CP_UTF8,0,error.what(),-1,detail.data(),length);
        if(!detail.empty())detail.pop_back();
        if(self->errorField){self->reveal(self->item(self->errorField));SetFocus(self->item(self->errorField));}
        self->error=L"保存失败："+detail;SendMessageW(self->item(Notice),WM_SETFONT,reinterpret_cast<WPARAM>(self->font),TRUE);SetWindowTextW(self->item(Notice),self->error.c_str());self->layout();
        if(self->errorField)self->reveal(self->item(self->errorField));
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
    if(!CreateWindowExW(WS_EX_CONTROLPARENT,type.lpszClassName,L"虎娘 · 输入设置",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,640,480,owner,nullptr,type.hInstance,&dialog))throw std::runtime_error("Cannot create input settings window");
    dialog.create();
    EnableMenuItem(GetSystemMenu(dialog.window,FALSE),SC_SIZE,MF_BYCOMMAND|MF_GRAYED);
    EnableMenuItem(GetSystemMenu(dialog.window,FALSE),SC_MAXIMIZE,MF_BYCOMMAND|MF_GRAYED);
    if(testMode) {
        const auto original=tiger::readConfiguration(path);
        if(testMode==9){
            for(UINT dpi:{96u,120u,144u,192u,96u}){dialog.scale(dpi);for(int selected=0;selected<5;++selected){dialog.page(selected);dialog.scrollPage(SB_BOTTOM,0);dialog.scrollPage(SB_TOP,0);}}
            DestroyWindow(dialog.window);return false;
        }
        if(testMode==7){
            if(dialog.text(FontSize)!=u"16.9")throw std::runtime_error("Font size did not display one decimal");
            const auto precise=tiger::configurationValue(original,u"字体大小");
            SetWindowTextW(dialog.item(MaxCode),L"6");SendMessageW(dialog.window,WM_COMMAND,Save,0);
            if(!dialog.saved || tiger::configurationValue(tiger::readConfiguration(path),u"字体大小")!=precise)throw std::runtime_error("Rounded display changed unedited font precision");
            return true;
        }
        if(testMode==6 || testMode==8){
            SendMessageW(dialog.tooltip,TTM_ACTIVATE,FALSE,0); // Keep hover overlays out of pixel comparisons.
            // Unlike WM_PRINT, screen pixels preserve defects left by incremental painting.
            ShowWindow(dialog.window,SW_SHOW);SetWindowPos(dialog.window,HWND_TOPMOST,40,40,dialog.px(640),dialog.px(480),SWP_SHOWWINDOW);SetFocus(dialog.item(Pages));
            auto settle=[&](){for(int pass=0;pass<5;++pass){MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}GdiFlush();Sleep(40);}};
            auto capture=[&](const std::filesystem::path& output){
                if(testMode==8)SetWindowPos(dialog.window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
                settle();RECT r{};GetClientRect(dialog.window,&r);POINT origin{};ClientToScreen(dialog.window,&origin);
                HRGN region=CreateRectRgn(0,0,0,0);GetWindowRgn(dialog.window,region);
                bool occluded=false;
                for(int y=12;y<r.bottom;y+=24)for(int x=12;x<r.right;x+=24)if(PtInRegion(region,x,y)){
                    POINT point{origin.x+x,origin.y+y};auto hit=WindowFromPoint(point);
                    if(GetAncestor(hit,GA_ROOT)!=dialog.window)occluded=true;
                }
                DeleteObject(region);if(occluded)throw std::runtime_error("Settings screen capture is covered by another window");
                HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),r.right,-r.bottom,1,32,BI_RGB};void* bits=nullptr;
                HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto previous=SelectObject(dc,bitmap);
                if(!BitBlt(dc,0,0,r.right,r.bottom,screen,origin.x,origin.y,SRCCOPY|CAPTUREBLT))throw std::runtime_error("Cannot capture active settings window");GdiFlush();
                std::vector<std::uint32_t> pixels(static_cast<std::uint32_t*>(bits),static_cast<std::uint32_t*>(bits)+static_cast<std::size_t>(r.right)*r.bottom);
                BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size()*4);
                std::ofstream file(output,std::ios::binary);file.write(reinterpret_cast<const char*>(&header),sizeof(header));file.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(info.bmiHeader));file.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);
                SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);return pixels;
            };
            if(testMode==8){
                dialog.page(0);SetFocus(dialog.item(100));bool was=dialog.checked(100);
                SendMessageW(dialog.item(100),WM_KEYDOWN,VK_SPACE,0);SendMessageW(dialog.item(100),WM_KEYUP,VK_SPACE,0);
                if(dialog.checked(100)==was)throw std::runtime_error("Space did not toggle native checkbox");
                SendMessageW(dialog.item(100),BM_SETCHECK,was?BST_CHECKED:BST_UNCHECKED,0);dialog.update();
                dialog.page(1);SendMessageW(dialog.item(Theme),CB_SHOWDROPDOWN,TRUE,0);settle();
                if(!SendMessageW(dialog.item(Theme),CB_GETDROPPEDSTATE,0,0))throw std::runtime_error("Theme dropdown did not open");
                SendMessageW(dialog.item(Theme),CB_SHOWDROPDOWN,FALSE,0);SetFocus(dialog.item(Pages));
                const auto style=GetWindowLongPtrW(dialog.window,GWL_STYLE);
                if(style&(WS_THICKFRAME|WS_MAXIMIZEBOX))throw std::runtime_error("Settings window is resizable");
                for(UINT dpi:{96u,120u,144u,192u}){
                    dialog.scale(dpi);RECT frame{};GetWindowRect(dialog.window,&frame);
                    if(frame.right-frame.left!=dialog.px(640) || frame.bottom-frame.top!=dialog.px(480))throw std::runtime_error("Wrong fixed DIP bounds");
                    if(SendMessageW(dialog.window,WM_NCHITTEST,0,MAKELPARAM(frame.left+dialog.px(360),frame.top+dialog.px(32)))!=HTCAPTION)throw std::runtime_error("Caption drag area missing");
                    if(GetMenuState(GetSystemMenu(dialog.window,FALSE),SC_MOVE,MF_BYCOMMAND)&MF_GRAYED)throw std::runtime_error("Keyboard move unavailable");
                    for(int selected=0;selected<5;++selected){dialog.page(selected);dialog.scrollPage(SB_TOP,0);capture(path.parent_path()/(L"screen-"+std::to_wstring(selected)+L"-"+std::to_wstring(dpi)+L".bmp"));}
                    const auto oldCode=dialog.text(MaxCode);SetWindowTextW(dialog.item(MaxCode),L"0");SendMessageW(dialog.window,WM_COMMAND,Save,0);
                    if(dialog.error.empty() || GetFocus()!=dialog.item(MaxCode))throw std::runtime_error("Validation error did not reveal and focus field");
                    capture(path.parent_path()/(L"screen-error-"+std::to_wstring(dpi)+L".bmp"));SetWindowTextW(dialog.item(MaxCode),wide(oldCode.c_str()));dialog.update();
                }
                dialog.scale(96);dialog.page(1);dialog.scrollPage(SB_TOP,0);
                const auto oldSize=dialog.text(FontSize),oldCount=dialog.text(PageSize);const bool oldVertical=dialog.checked(300),oldHidden=dialog.checked(303);
                dialog.captureSentence(path.parent_path()/L"preview-fixed-before.bmp",1);
                SetWindowTextW(dialog.item(FontSize),L"200");dialog.update();
                dialog.captureSentence(path.parent_path()/L"preview-fixed-after.bmp",1);
                SetWindowTextW(dialog.item(PageSize),L"10");SendMessageW(dialog.item(300),BM_SETCHECK,BST_UNCHECKED,0);dialog.update();
                capture(path.parent_path()/L"screen-preview-extreme.bmp");if(dialog.previewFailed)throw std::runtime_error("Large horizontal ten-candidate preview failed");
                SendMessageW(dialog.item(303),BM_SETCHECK,BST_CHECKED,0);dialog.update();capture(path.parent_path()/L"screen-preview-hidden.bmp");
                SetWindowTextW(dialog.item(FontSize),wide(oldSize.c_str()));SetWindowTextW(dialog.item(PageSize),wide(oldCount.c_str()));
                SendMessageW(dialog.item(300),BM_SETCHECK,oldVertical?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(dialog.item(303),BM_SETCHECK,oldHidden?BST_CHECKED:BST_UNCHECKED,0);dialog.update();
                SetForegroundWindow(dialog.window);
                RECT beforeMove{},afterMove{};GetWindowRect(dialog.window,&beforeMove);
                // Drive the real DefWindowProc keyboard-move modal loop.
                std::thread keys([&](){Sleep(150);PostMessageW(dialog.window,WM_KEYDOWN,VK_RIGHT,0);PostMessageW(dialog.window,WM_KEYUP,VK_RIGHT,0);PostMessageW(dialog.window,WM_KEYDOWN,VK_RETURN,0);PostMessageW(dialog.window,WM_KEYUP,VK_RETURN,0);});
                SendMessageW(dialog.window,WM_SYSCOMMAND,SC_MOVE,0);keys.join();settle();GetWindowRect(dialog.window,&afterMove);
                if(afterMove.left<=beforeMove.left)throw std::runtime_error("System keyboard move failed");
                POINT cursor{};GetCursorPos(&cursor);GetWindowRect(dialog.window,&beforeMove);
                const POINT start{beforeMove.left+360,beforeMove.top+32},finish{start.x+32,start.y+20};const BOOL cursorMoved=SetCursorPos(start.x,start.y);bool inMove=false;POINT observed{};
                INPUT press{};press.type=INPUT_MOUSE;press.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;const UINT injected=SendInput(1,&press,sizeof(press));Sleep(50);const SHORT mouseState=GetAsyncKeyState(VK_LBUTTON);
                std::thread drag([&](){Sleep(200);GUITHREADINFO state{sizeof(state)};if(GetGUIThreadInfo(GetWindowThreadProcessId(dialog.window,nullptr),&state))inMove=(state.flags&GUI_INMOVESIZE)!=0;SetCursorPos(start.x+16,start.y+10);Sleep(100);SetCursorPos(finish.x,finish.y);GetCursorPos(&observed);Sleep(100);INPUT release{};release.type=INPUT_MOUSE;release.mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(1,&release,sizeof(release));PostMessageW(dialog.window,WM_LBUTTONUP,0,0);});
                SendMessageW(dialog.window,WM_NCLBUTTONDOWN,HTCAPTION,MAKELPARAM(start.x,start.y));drag.join();settle();SetCursorPos(cursor.x,cursor.y);
                GetWindowRect(dialog.window,&afterMove);if(afterMove.left==beforeMove.left && afterMove.top==beforeMove.top)throw std::runtime_error("Caption mouse drag failed: cursor="+std::to_string(cursorMoved)+" injected="+std::to_string(injected)+" mouse="+std::to_string(mouseState)+" modal="+std::to_string(inMove)+" start="+std::to_string(start.x)+","+std::to_string(start.y)+" observed="+std::to_string(observed.x)+","+std::to_string(observed.y));
                bool systemMenuOpened=false;
                std::thread menu([&](){Sleep(150);GUITHREADINFO state{sizeof(state)};if(GetGUIThreadInfo(GetWindowThreadProcessId(dialog.window,nullptr),&state))systemMenuOpened=(state.flags&GUI_INMENUMODE)!=0;PostMessageW(dialog.window,WM_KEYDOWN,VK_ESCAPE,0);PostMessageW(dialog.window,WM_KEYUP,VK_ESCAPE,0);});
                SendMessageW(dialog.window,WM_SYSCOMMAND,SC_KEYMENU,VK_SPACE);menu.join();settle();
                if(!systemMenuOpened)throw std::runtime_error("Alt+Space system menu failed");

                SendMessageW(dialog.window,WM_SYSCOMMAND,SC_MINIMIZE,0);settle();if(!IsIconic(dialog.window))throw std::runtime_error("Minimize failed");
                SendMessageW(dialog.window,WM_SYSCOMMAND,SC_RESTORE,0);settle();if(IsIconic(dialog.window))throw std::runtime_error("Restore failed");
                Dialog::testHighContrast=true;dialog.layout();capture(path.parent_path()/L"screen-high-contrast.bmp");Dialog::testHighContrast=false;
                SendMessageW(dialog.window,WM_CLOSE,0,0);if(IsWindow(dialog.window) || dialog.saved)throw std::runtime_error("Close saved edits");
                return false;
            }
            bool mismatch=false;
            for(int selected:{3,1,2,0}){
                SetWindowPos(dialog.window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
                dialog.page(selected);settle();
                for(int direction:{SB_LINEDOWN,SB_LINEDOWN,SB_LINEUP,SB_PAGEDOWN,SB_PAGEUP}){SendMessageW(dialog.content,WM_VSCROLL,direction,0);settle();}
                auto before=capture(path.parent_path()/(L"scroll-"+std::to_wstring(selected)+L"-before.bmp"));
                RedrawWindow(dialog.window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN|RDW_UPDATENOW);
                auto after=capture(path.parent_path()/(L"scroll-"+std::to_wstring(selected)+L"-after.bmp"));
                // Exclude desktop pixels using the actual tiger-ear window region.
                RECT client{};GetClientRect(dialog.window,&client);std::size_t changed=0;
                HRGN owned=CreateRectRgn(0,0,0,0);GetWindowRgn(dialog.window,owned);
                for(int y=0;y<client.bottom;++y)for(int x=0;x<client.right;++x){
                    if(!PtInRegion(owned,x,y))continue;
                    auto i=static_cast<std::size_t>(y)*client.right+x;if((before[i]&0xffffff)!=(after[i]&0xffffff))++changed;
                }
                DeleteObject(owned);
                std::ofstream(path.parent_path()/(L"scroll-"+std::to_wstring(selected)+L".txt"))<<changed;
                mismatch=mismatch || changed!=0;
            }
            DestroyWindow(dialog.window);
            if(mismatch)throw std::runtime_error("Scrolling leaves stale screen pixels; full repaint changes the window");
            return false;
        }
        if(testMode==5){
            for(int i=0;i<static_cast<int>(std::size(flags));++i)if(flags[i].member)SendMessageW(dialog.item(100+i),BM_SETCHECK,dialog.initial.*flags[i].member?BST_UNCHECKED:BST_CHECKED,0);
            for(int i=0;i<4;++i)SendMessageW(dialog.item(300+i),BM_SETCHECK,dialog.initialStyle.*styleFlags[i].member?BST_UNCHECKED:BST_CHECKED,0);
            dialog.update();SendMessageW(dialog.window,WM_COMMAND,Save,0);
            if(!dialog.saved)throw std::runtime_error("Moved flags did not save");
            auto config=tiger::parseEngineSettings(tiger::readConfiguration(path));auto style=tiger::parseCandidateStyle(tiger::readConfiguration(path));
            for(int i=0;i<static_cast<int>(std::size(flags));++i)if(flags[i].member && config.*flags[i].member==dialog.initial.*flags[i].member)throw std::runtime_error("Moved input flag lost");
            for(int i=0;i<4;++i)if(style.*styleFlags[i].member==dialog.initialStyle.*styleFlags[i].member)throw std::runtime_error("Moved style flag lost");
            return true;
        }
        if(testMode==4){
            std::u16string longList;for(int i=0;i<500;++i)longList+=u"天地人";
            SetWindowTextW(dialog.item(SentenceWhitelist),wide((longList+u"\r\n甲\n乙").c_str()));
            if(dialog.text(SentenceWhitelist).find(u'\n')==std::u16string::npos)throw std::runtime_error("Multiline fixture missing line breaks");
            SendMessageW(dialog.window,WM_COMMAND,Save,0);
            if(!dialog.saved || tiger::parseSentenceSettings(tiger::readConfiguration(path)).fullCodeWhitelist!=longList+u"甲乙")throw std::runtime_error("Long multiline whitelist did not save as a single line");
            return true;
        }
        if(testMode==3){
            auto timestamp=std::filesystem::last_write_time(path);
            SendMessageW(dialog.window,WM_COMMAND,Save,0);
            if(!dialog.saved || tiger::readConfiguration(path)!=original || std::filesystem::last_write_time(path)!=timestamp)throw std::runtime_error("Unmodified save wrote configuration");
            return true;
        }
        // Each dependency must preserve its value across disable / enable.
        for(auto pair:{std::make_pair(AnimationEnabled,AnimationDuration),std::make_pair(SentenceAuto,SentenceRetained),std::make_pair(AddEnabled,AddShortcut),std::make_pair(RecentEnabled,RecentShortcut)}){
            bool old=dialog.checked(pair.first);auto before=dialog.values();
            SendMessageW(dialog.item(pair.first),BM_SETCHECK,BST_UNCHECKED,0);dialog.update();
            if(IsWindowEnabled(dialog.item(pair.second)))throw std::runtime_error("Dependency did not disable");
            SendMessageW(dialog.item(pair.first),BM_SETCHECK,BST_CHECKED,0);dialog.update();
            if(!IsWindowEnabled(dialog.item(pair.second)))throw std::runtime_error("Dependency did not enable");
            SendMessageW(dialog.item(pair.first),BM_SETCHECK,old?BST_CHECKED:BST_UNCHECKED,0);dialog.update();
            if(before!=dialog.values())throw std::runtime_error("Dependency changed value");
        }
        bool comment=dialog.checked(113),split=dialog.checked(114);
        SendMessageW(dialog.item(113),BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(dialog.item(114),BM_SETCHECK,BST_UNCHECKED,0);dialog.update();
        if(IsWindowEnabled(dialog.item(AnnotationDelay)))throw std::runtime_error("Annotation delay not disabled");
        SendMessageW(dialog.item(114),BM_SETCHECK,BST_CHECKED,0);dialog.update();if(!IsWindowEnabled(dialog.item(AnnotationDelay)))throw std::runtime_error("Split did not enable annotation delay");
        SendMessageW(dialog.item(113),BM_SETCHECK,comment?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(dialog.item(114),BM_SETCHECK,split?BST_CHECKED:BST_UNCHECKED,0);dialog.update();
        wchar_t desktopName[256]{},inputName[256]{};DWORD needed=0;
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktopName,sizeof(desktopName),&needed);
        const auto inputDesktop=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
        if(inputDesktop){GetUserObjectInformationW(inputDesktop,UOI_NAME,inputName,sizeof(inputName),&needed);CloseDesktop(inputDesktop);}
        const bool isolatedCapture=std::wstring_view(desktopName).find(L"NativeTigerSettingsCapture_")==0 &&
            inputName[0] && std::wstring_view(desktopName)!=inputName;
        if(isolatedCapture){ShowWindow(dialog.window,SW_SHOWNOACTIVATE);UpdateWindow(dialog.window);}
        if(testMode==1) {
            // Repeated movement inside a checkbox/button must not repaint its label.
            dialog.page(0);
            const auto countPaint=[](HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data)->LRESULT {
                if(message==WM_PAINT)++*reinterpret_cast<unsigned*>(data);
                return DefSubclassProc(hwnd,message,w,l);
            };
            for(int id:{100,105,Save,Cancel}){
                HWND button=dialog.item(id);unsigned paints=0;
                SetWindowSubclass(button,countPaint,31,reinterpret_cast<DWORD_PTR>(&paints));
                SendMessageW(button,WM_MOUSEMOVE,0,MAKELPARAM(10,10));UpdateWindow(button);paints=0;
                for(int x=11;x<111;++x){SendMessageW(button,WM_MOUSEMOVE,0,MAKELPARAM(x,10));UpdateWindow(button);}
                RemoveWindowSubclass(button,countPaint,31);
                SendMessageW(button,WM_MOUSELEAVE,0,0);
                if(paints)throw std::runtime_error("Movement within button caused redundant repaints");
            }
            const bool checked=dialog.checked(100);
            SendMessageW(dialog.item(100),BM_CLICK,0,0);
            if(dialog.checked(100)==checked)throw std::runtime_error("Buffered checkbox stopped toggling");
            SendMessageW(dialog.item(100),BM_CLICK,0,0);
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
        if(dialog.fonts->previewSize()!=14)throw std::runtime_error("Preview sample size is not fixed");
        if(testMode==1){
            const auto oldCode=dialog.text(MaxCode);auto edit=dialog.item(MaxCode);
            SendMessageW(edit,EM_SETSEL,0,-1);SendMessageW(edit,WM_CHAR,L'6',0);
            if(dialog.text(MaxCode)!=u"6")throw std::runtime_error("Styled edit lost native text entry");
            SendMessageW(edit,WM_CHAR,VK_BACK,0);if(!dialog.text(MaxCode).empty())throw std::runtime_error("Styled edit lost backspace");
            SendMessageW(edit,EM_UNDO,0,0);if(dialog.text(MaxCode)!=u"6")throw std::runtime_error("Styled edit lost undo");
            SetWindowTextW(edit,wide(oldCode.c_str()));dialog.update();
        }
        for(UINT dpi:{96u,120u,144u,192u}) {
            dialog.scale(dpi);RECT client{};GetClientRect(dialog.window,&client);
            for(auto c:dialog.controls){wchar_t controlType[24]{};GetClassNameW(c.window,controlType,24);if(std::wstring_view(controlType)!=L"Edit")continue;
                if(GetWindowLongPtrW(c.window,GWL_EXSTYLE)&WS_EX_CLIENTEDGE)throw std::runtime_error("Old sunken edit frame remains");
                if(!(GetWindowLongPtrW(c.window,GWL_STYLE)&ES_CENTER))throw std::runtime_error("Edit is not horizontally centered");
                RECT field{},format{};GetClientRect(c.window,&field);SendMessageW(c.window,EM_GETRECT,0,reinterpret_cast<LPARAM>(&format));
                if(GetDlgCtrlID(c.window)!=SentenceWhitelist && format.top<=0)throw std::runtime_error("Edit is not vertically centered");
            }
            for(int page=0;page<TabCtrl_GetItemCount(dialog.item(Pages));++page) {
                TabCtrl_SetCurSel(dialog.item(Pages),page);
                NMHDR notification{dialog.item(Pages),Pages,TCN_SELCHANGE};
                SendMessageW(dialog.window,WM_NOTIFY,Pages,reinterpret_cast<LPARAM>(&notification));
                for(auto control:dialog.controls)if(control.page>=0 &&
                    (((GetWindowLongPtrW(control.window,GWL_STYLE)&WS_VISIBLE)!=0)!=(control.page==page)))
                    throw std::runtime_error("Native tab did not select the requested page");
            }
            for(auto control:dialog.controls)if(control.page>=0){
                dialog.reveal(control.window);RECT bounds{},pane{};GetWindowRect(control.window,&bounds);MapWindowPoints(nullptr,dialog.content,reinterpret_cast<POINT*>(&bounds),2);GetClientRect(dialog.content,&pane);
                if(GetParent(control.window)!=dialog.content || bounds.left<0 || bounds.right>pane.right || bounds.bottom>pane.bottom || bounds.top<0)
                    throw std::runtime_error("Setting cannot be reached by scrolling: "+std::to_string(GetDlgCtrlID(control.window)));
            }
            for(auto& offset:dialog.scroll)offset=0;dialog.layout();
            if(testMode==1){
                dialog.page(0);UpdateWindow(dialog.window);dialog.captureSentence(path.parent_path()/(L"input-settings-"+std::to_wstring(dpi)+L".bmp"),0);
                if(dialog.fonts->items().size()<=3)throw std::runtime_error("System font catalog is incomplete");
                const auto originalFont=dialog.fonts->selected();
                if(!dialog.fonts->select(L"Segoe UI"))throw std::runtime_error("System font alias not recognized");
                if(dialog.fonts->selected()!=L"Segoe UI")throw std::runtime_error("System font alias resolves incorrectly");
                dialog.fonts->select(originalFont);
                dialog.page(1);UpdateWindow(dialog.window);dialog.captureSentence(path.parent_path()/(L"font-settings-"+std::to_wstring(dpi)+L".bmp"),1);
                if(isolatedCapture){SendMessageW(dialog.item(FontName),CB_SHOWDROPDOWN,TRUE,0);dialog.captureSentence(path.parent_path()/(L"font-dropdown-"+std::to_wstring(dpi)+L".bmp"),-2);SendMessageW(dialog.item(FontName),CB_SHOWDROPDOWN,FALSE,0);}
                for(int selected:{1,2,3}){
                    dialog.page(selected);dialog.scrollPage(SB_TOP,0);UpdateWindow(dialog.window);
                    dialog.captureSentence(path.parent_path()/(L"page-"+std::to_wstring(selected)+L"-top-"+std::to_wstring(dpi)+L".bmp"),selected);
                    dialog.scrollPage(SB_BOTTOM,0);UpdateWindow(dialog.window);
                    dialog.captureSentence(path.parent_path()/(L"page-"+std::to_wstring(selected)+L"-bottom-"+std::to_wstring(dpi)+L".bmp"),selected);
                    dialog.scrollPage(SB_TOP,0);
                }
                dialog.page(4);UpdateWindow(dialog.window);dialog.captureSentence(path.parent_path()/(L"donation-settings-"+std::to_wstring(dpi)+L".bmp"),4);
                dialog.page(3);UpdateWindow(dialog.window);dialog.captureSentence(path.parent_path()/(L"sentence-settings-"+std::to_wstring(dpi)+L".bmp"));dialog.page(0);}
        }
        // Keep the requested fixed size even when a smaller work area would require scrolling.
        SetWindowPos(dialog.window,nullptr,0,0,dialog.px(640),dialog.px(480),SWP_NOMOVE|SWP_NOZORDER);
        for(auto c:dialog.controls)if(GetWindowLongPtrW(c.window,GWL_STYLE)&WS_TABSTOP)
            throw std::runtime_error("Settings control still has Tab stop");
        if(dialog.item(102))throw std::runtime_error("Removed Ctrl+Space option still exists");
        if(SendMessageW(dialog.tooltip,TTM_GETTOOLCOUNT,0,0)!=static_cast<LRESULT>(dialog.tipTexts.size()) || dialog.tipTexts.size()<15)
            throw std::runtime_error("Settings tips are missing");
        if(testMode==1)for(int selected=0;selected<5;++selected){
            dialog.page(selected);dialog.scrollPage(SB_TOP,0);dialog.captureSentence(path.parent_path()/(L"small-"+std::to_wstring(selected)+L"-top.bmp"),selected);
            dialog.scrollPage(SB_BOTTOM,0);dialog.captureSentence(path.parent_path()/(L"small-"+std::to_wstring(selected)+L"-bottom.bmp"),selected);
        }
        dialog.scale(96);for(auto& offset:dialog.scroll)offset=0;
        if(testMode==1){
            Dialog::testHighContrast=true;
            for(int selected=0;selected<5;++selected){dialog.page(selected);dialog.scrollPage(SB_TOP,0);dialog.captureSentence(path.parent_path()/(L"system-colors-"+std::to_wstring(selected)+L".bmp"),selected);}
            Dialog::testHighContrast=false;
        }
        dialog.page(1);dialog.scrollPage(SB_TOP,0);
        auto priorSize=dialog.text(FontSize);SetWindowTextW(dialog.item(FontSize),L"200");dialog.update();
        if(testMode==1)dialog.captureSentence(path.parent_path()/L"preview-large.bmp",1);
        SetWindowTextW(dialog.item(FontSize),wide(priorSize.c_str()));dialog.update();
        if(testMode==1){
            auto before=dialog.values();bool vertical=dialog.checked(300),code=dialog.checked(302),index=dialog.checked(301),hidden=dialog.checked(303),commentVisible=dialog.checked(113),splitVisible=dialog.checked(114);
            auto oldTheme=SendMessageW(dialog.item(Theme),CB_GETCURSEL,0,0);
            SendMessageW(dialog.item(300),BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(dialog.item(302),BM_SETCHECK,BST_CHECKED,0);
            SendMessageW(dialog.item(Theme),CB_SETCURSEL,8,0);dialog.update();dialog.captureSentence(path.parent_path()/L"preview-horizontal.bmp",1);
            SendMessageW(dialog.item(301),BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(dialog.item(113),BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(dialog.item(114),BM_SETCHECK,BST_CHECKED,0);
            dialog.update();dialog.captureSentence(path.parent_path()/L"preview-split.bmp",1);
            SendMessageW(dialog.item(303),BM_SETCHECK,BST_CHECKED,0);dialog.update();dialog.captureSentence(path.parent_path()/L"preview-hidden.bmp",1);
            for(auto entry:{std::make_pair(300,vertical),std::make_pair(302,code),std::make_pair(301,index),std::make_pair(303,hidden),std::make_pair(113,commentVisible),std::make_pair(114,splitVisible)})SendMessageW(dialog.item(entry.first),BM_SETCHECK,entry.second?BST_CHECKED:BST_UNCHECKED,0);
            SendMessageW(dialog.item(Theme),CB_SETCURSEL,oldTheme,0);dialog.update();if(before!=dialog.values())throw std::runtime_error("Preview did not restore controls");
        }
        if(testMode==1 && (!dialog.previewCount || dialog.previewFailed))throw std::runtime_error("Candidate preview failed to render");
        if(tiger::readConfiguration(path)!=original)throw std::runtime_error("Preview changed configuration");
        if(testMode==2) {
            if(dialog.initialStyle.codeMask!=u"甲😀乙")throw std::runtime_error("Mask setting did not reopen");
            if(dialog.initialStyle.animationEnabled || dialog.initialStyle.animationDurationMs!=321)throw std::runtime_error("Animation settings did not reopen");
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
        if(TabCtrl_GetCurSel(dialog.item(Pages))!=1 || GetFocus()!=dialog.item(PageSize))throw std::runtime_error("Invalid number did not focus appearance field");
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
        SendMessageW(dialog.item(AnimationEnabled),BM_SETCHECK,BST_UNCHECKED,0);
        SetWindowTextW(dialog.item(AnimationDuration),L"321");
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
        if(message.message==WM_KEYDOWN && message.wParam==VK_TAB)continue;
        TranslateMessage(&message);DispatchMessageW(&message);
    }
    if(enabled){EnableWindow(owner,TRUE);SetActiveWindow(owner);}return dialog.saved;
}
