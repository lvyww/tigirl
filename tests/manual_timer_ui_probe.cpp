#define NOMINMAX
#include "tsf/ManualTimer.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
static LONG leases=0;
static std::atomic<bool> popup{false},popupText{false};
static std::atomic<HWND> popupWindow{nullptr};
static std::atomic<HWND> popupButton{nullptr};
static std::atomic<bool> popupClosed{false};
static BOOL CALLBACK inspectText(HWND window,LPARAM) {
    wchar_t text[64]{};GetWindowTextW(window,text,64);
    wchar_t kind[64]{};GetClassNameW(window,kind,64);
    if(std::wstring(kind)==L"Button" && IsWindowEnabled(window)) popupButton=window;
    if(std::wstring(text)==L"时间差不多咯！") popupText=true;
    return TRUE;
}
static BOOL CALLBACK dismissPopup(HWND window,LPARAM) {
    DWORD process=0;GetWindowThreadProcessId(window,&process);
    if(process!=GetCurrentProcessId()) return TRUE;
    wchar_t title[64]{};GetWindowTextW(window,title,64);
    if(std::wstring(title)==L"计时器") {
        EnumChildWindows(window,inspectText,0);
        popup=true;
        popupWindow=window;
        if(popupButton) PostMessageW(popupButton,BM_CLICK,0,0);
    }
    return TRUE;
}
void DllAddRef() { ++leases; }
void DllRelease() { --leases; }
static void require(bool value,const char* error) { if(!value) throw std::runtime_error(error); }
static void pump(DWORD milliseconds) {
    const auto until=GetTickCount64()+milliseconds;
    do {
        MSG message;
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&message);DispatchMessageW(&message);}
        Sleep(1);
    } while(GetTickCount64()<until);
}
int main() {
    try {
        unsigned calls=0;
        auto timer=tiger::tsf::ManualTimer::create(GetModuleHandleW(nullptr),[&]{++calls;});
        timer->schedule(0);require(calls==0,"Immediate timer ran synchronously");
        timer->schedule(100);pump(20);require(calls==0,"Replaced immediate timer fired");
        pump(180);require(calls==1,"Replacement timer did not fire once");
        pump(30);require(calls==1,"Timer repeated");
        timer->schedule(0);timer->schedule(-1);pump(20);require(calls==2,"Invalid delay cancelled existing timer");
        timer->schedule(0);timer->close();pump(20);require(calls==2,"Closed timer fired");
        timer.reset();require(leases==0,"Closed timer retained DLL");
        std::shared_ptr<tiger::tsf::ManualTimer> releasing; bool callbackLifetime=false;
        releasing=tiger::tsf::ManualTimer::create(GetModuleHandleW(nullptr),[&] {
            ++calls;releasing->close();releasing.reset();
            callbackLifetime=leases==1;
        });
        releasing->schedule(0);pump(20);
        require(calls==3 && leases==0 && callbackLifetime,"Callback teardown lost or retained DLL lifetime");
        auto reminder=tiger::tsf::ManualTimer::create(GetModuleHandleW(nullptr));
        std::thread closer([] {
            for(int i=0;i<250;++i) {
                if(popupWindow && !IsWindow(popupWindow)) {popupClosed=true;break;}
                EnumWindows(dismissPopup,0);Sleep(20);
            }
        });
        reminder->schedule(0);pump(60);closer.join();reminder.reset();
        require(popup && popupText && popupClosed && leases==0,"Default reminder text, close or lifetime differs");
        std::cout<<"{\"status\":\"passed\",\"callbacks\":3,\"replacement\":true,\"cancellation\":true,\"reminder_popup\":true,\"dll_leases\":0}\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
