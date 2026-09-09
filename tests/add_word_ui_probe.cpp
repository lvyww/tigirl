#define NOMINMAX
#include "tsf/AddWordUI.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
static LONG leases=0;
void DllAddRef() { ++leases; }
void DllRelease() { --leases; }
namespace {
std::shared_ptr<tiger::tsf::AddWordUI> ui;
std::shared_ptr<const tiger::Dictionary> dictionary;
std::unique_ptr<tiger::UserStore> store;
std::filesystem::path capturePath;
int phase=0,saves=0,ticks=0; bool done=false; std::string error;
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
std::wstring text(HWND window,int id) {
    auto item=GetDlgItem(window,id); std::wstring result(GetWindowTextLengthW(item)+1,L'\0');
    auto n=GetWindowTextW(item,result.data(),static_cast<int>(result.size())); result.resize(n); return result;
}
BOOL CALLBACK find(HWND window,LPARAM output) {
    wchar_t title[64];GetWindowTextW(window,title,64);
    if(std::wstring(title)==L"虎娘加词") *reinterpret_cast<HWND*>(output)=window;
    return TRUE;
}
void setWord(HWND window,const wchar_t* word) {
    SendDlgItemMessageW(window,101,EM_SETSEL,0,-1);
    SendDlgItemMessageW(window,101,EM_REPLACESEL,TRUE,reinterpret_cast<LPARAM>(word));
}
void capture(HWND window) {
    if(capturePath.empty()) return;
    RECT rect{}; GetClientRect(window,&rect);
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=rect.right; info.bmiHeader.biHeight=-rect.bottom;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    HDC screen=GetDC(window),memory=CreateCompatibleDC(screen); void* pixels=nullptr;
    auto bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    require(bitmap && pixels,"Cannot allocate dialog capture"); auto old=SelectObject(memory,bitmap);
    UpdateWindow(window); require(PrintWindow(window,memory,PW_CLIENTONLY)!=FALSE,"Cannot render dialog capture"); GdiFlush();
    BITMAPFILEHEADER header{}; header.bfType=0x4d42; header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
    header.bfSize=header.bfOffBits+static_cast<DWORD>(rect.right*rect.bottom*4);
    std::ofstream file(capturePath,std::ios::binary);
    file.write(reinterpret_cast<const char*>(&header),sizeof(header));
    file.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(info.bmiHeader));
    file.write(static_cast<const char*>(pixels),static_cast<std::streamsize>(rect.right)*rect.bottom*4);
    SelectObject(memory,old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(window,screen);
    require(file.good(),"Cannot save dialog capture");
}
void CALLBACK tick(HWND,UINT,UINT_PTR,DWORD) {
    try {
        if(++ticks>300) throw std::runtime_error("Dialog test timed out");
        HWND window=nullptr; EnumThreadWindows(GetCurrentThreadId(),find,reinterpret_cast<LPARAM>(&window));
        if(phase==0 && window) {
            require(text(window,101)==L"人民","Default recent word");
            require(!text(window,102).empty(),"Initial phrase code missing");
            require(SendDlgItemMessageW(window,101,WM_GETDLGCODE,VK_TAB,0)&DLGC_WANTTAB,"Word edit cannot accept tabs");
            capture(window);
            SendMessageW(window,WM_COMMAND,104,0); require(text(window,101)==L"国人民","Expand recent word");
            SendMessageW(window,WM_COMMAND,105,0); SendMessageW(window,WM_COMMAND,105,0); require(text(window,101)==L"民","Shrink recent word");
            setWord(window,L"中国");
            auto expected=tiger::constructWordCode(*dictionary,u"中国");
            require(text(window,102)==reinterpret_cast<const wchar_t*>(expected.c_str()),"Automatic phrase code");
            SetDlgItemTextW(window,102,L" \t ");
            SetFocus(GetDlgItem(window,101));
            SendMessageW(window,WM_COMMAND,IDOK,0);
            require(saves==0 && text(window,103)==L"编码为空。" && GetFocus()==GetDlgItem(window,102),"Empty code validation/focus");
            setWord(window,L""); SetDlgItemTextW(window,102,L"zzzzzy");
            SetFocus(GetDlgItem(window,102));
            SendMessageW(window,WM_COMMAND,106,0);
            require(saves==0 && text(window,103)==L"词条为空。" && GetFocus()==GetDlgItem(window,101),"Empty word validation/focus");
            SetDlgItemTextW(window,102,L"");
            SendMessageW(window,WM_COMMAND,106,0);
            require(saves==0 && text(window,103)==L"编码为空。" && GetFocus()==GetDlgItem(window,102),"Empty fields validation priority");
            setWord(window,L"   "); SetDlgItemTextW(window,102,L"zzzzzy");
            SendMessageW(window,WM_COMMAND,106,0); require(saves==0 && !text(window,103).empty(),"Invalid word was saved");
            setWord(window,L"测试=>上屏\\n内容"); SetDlgItemTextW(window,102,L" ZzZzZy ");
            SendMessageW(window,WM_COMMAND,106,0);
            require(saves==1 && text(window,101).empty() && text(window,102).empty(),"Keep-adding did not save and clear");
            require(GetFocus()==GetDlgItem(window,101),"Keep-adding word focus");
            auto lexicon=store->refresh(); auto entry=lexicon->find(tiger::Section::Main,u"zzzzzy");
            require(entry.count==1 && lexicon->value(entry,0)==u"测试\x1e上屏\r\n内容","Alias/escape journal persistence");
            setWord(window,L"第二条"); SetDlgItemTextW(window,102,L"zzzzzz");
            SendMessageW(window,WM_COMMAND,IDOK,0); require(saves==2,"Add-and-close callback"); phase=1;
        } else if(phase==1 && !window) { ui->request(u"取消测试"); phase=2; }
        else if(phase==2 && window) { SendMessageW(window,WM_COMMAND,IDCANCEL,0); phase=3; }
        else if(phase==3 && !window) {
            require(saves==2,"Cancel wrote a word");
            auto history=tiger::wordTextElements(u"abcdeabcdefghijklmnopqrs😀");
            history.push_back(u"a"); history.push_back(u"\u0301");
            ui->request(std::move(history)); phase=4;
        }
        else if(phase==4 && window) {
            require(text(window,101)==L"a\u0301","Long history initial suffix");
            SendMessageW(window,WM_COMMAND,105,0);
            require(text(window,101)==L"\u0301","Dialog merged separate commit boundaries");
            SendMessageW(window,WM_COMMAND,104,0);
            for(int i=0;i<30;++i) SendMessageW(window,WM_COMMAND,104,0);
            require(text(window,101)==L"cdefghijklmnopqrs😀a\u0301","Add-word history exceeded original 20-element limit");
            for(int i=0;i<30;++i) SendMessageW(window,WM_COMMAND,105,0);
            require(text(window,101).empty() && text(window,102).empty(),"History shrink did not stop at empty");
            ui->close(); ui.reset(); phase=5;
        }
        else if(phase==5 && !window) { require(leases==0,"Dialog retained DLL after close"); done=true; }
    } catch(const std::exception& e) { error=e.what(); if(ui) ui->close(); done=true; }
}
}
int main(int argc,char** argv) {
    try {
        require(argc==3 || argc==4,"add_word_ui_probe <dictionary> <new isolated journal> [capture.bmp]");
        if(argc==4) capturePath=std::filesystem::u8path(argv[3]);
        const auto journal=std::filesystem::u8path(argv[2]); require(!std::filesystem::exists(journal),"Refuse existing journal");
        dictionary=tiger::Dictionary::Open(std::filesystem::u8path(argv[1]));
        store=std::make_unique<tiger::UserStore>(dictionary,journal);
        ui=tiger::tsf::AddWordUI::create(GetModuleHandleW(nullptr),*store,dictionary,[](auto){++saves;});
        ui->request(u"中国人民"); require(saves==0,"Queued UI already saved a word");
        auto timer=SetTimer(nullptr,0,20,tick); require(timer!=0,"No test timer");
        MSG message; while(!done && GetMessageW(&message,nullptr,0,0)>0) { TranslateMessage(&message); DispatchMessageW(&message); }
        KillTimer(nullptr,timer); if(ui) ui->close(); ui.reset();
        if(!error.empty()) throw std::runtime_error(error);
        require(done && saves==2 && leases==0,"Incomplete dialog test");
        auto lexicon=store->refresh(); require(lexicon->find(tiger::Section::Main,u"zzzzzz").count==1,"Second word missing");
        std::cout<<"{\"status\":\"passed\",\"saved_words\":2,\"dialogs\":3,\"focus_checks\":4,\"dll_leases\":0,\"physical_keys\":0}\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
