#define NOMINMAX
#include "ManualTimer.h"
#include <stdexcept>
#include <limits>
#include <cstdint>
extern void DllAddRef();
extern void DllRelease();
namespace tiger::tsf {
namespace {
DWORD WINAPI reminderThread(void* parameter) {
    const auto module=static_cast<HMODULE>(parameter);
    const auto previous=GetThreadDesktop(GetCurrentThreadId());
    const auto desktop=OpenDesktopW(L"Default",0,FALSE,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS);
    if(desktop && SetThreadDesktop(desktop)) {
        MessageBoxW(nullptr,L"时间差不多咯！",L"计时器",MB_OK|MB_ICONINFORMATION);
        SetThreadDesktop(previous);
    } else OutputDebugStringW(L"NativeTiger: Cannot open default desktop for timer reminder\n");
    if(desktop) CloseDesktop(desktop);
    FreeLibraryAndExitThread(module,0);
}
void showReminder() {
    HMODULE module=nullptr;
    // The notification may outlive TSF deactivation. Hold an actual loader
    // reference, released atomically with thread exit, not just a COM lease.
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&reminderThread),&module)) throw std::runtime_error("Cannot retain reminder module");
    HANDLE thread=CreateThread(nullptr,0,reminderThread,module,0,nullptr);
    if(!thread) { FreeLibrary(module); throw std::runtime_error("Cannot start reminder thread"); }
    CloseHandle(thread);
}
}
ManualTimer::ManualTimer(HINSTANCE module,std::function<void()> elapsed,std::wstring classPrefix)
    :module_(module),elapsed_(std::move(elapsed)) {
    if(!elapsed_) elapsed_=showReminder;
    className_=std::move(classPrefix)+std::to_wstring(reinterpret_cast<std::uintptr_t>(this));
    WNDCLASSW type{};type.hInstance=module_;type.lpfnWndProc=dispatch;type.lpszClassName=className_.c_str();
    if(!RegisterClassW(&type)) throw std::runtime_error("Cannot register timer window");
    window_=CreateWindowExW(0,className_.c_str(),L"",0,0,0,0,0,HWND_MESSAGE,nullptr,module_,this);
    if(!window_) { UnregisterClassW(className_.c_str(),module_); throw std::runtime_error("Cannot create timer window"); }
    DllAddRef();
}
std::shared_ptr<ManualTimer> ManualTimer::create(HINSTANCE module,std::function<void()> elapsed,std::wstring classPrefix) {
    return std::shared_ptr<ManualTimer>(new ManualTimer(module,std::move(elapsed),std::move(classPrefix)));
}
ManualTimer::~ManualTimer() { close(); UnregisterClassW(className_.c_str(),module_); DllRelease(); }
void ManualTimer::close() {
    if(window_) {
        if(active_) KillTimer(window_,active_);
        active_=0; DestroyWindow(window_); window_=nullptr;
    }
    elapsed_={};
}
void ManualTimer::schedule(int milliseconds) {
    if(milliseconds<0) return; // Zero/invalid minutes do not replace an old timer.
    if(!window_) throw std::runtime_error("Manual timer is closed");
    if(serial_==std::numeric_limits<UINT_PTR>::max()) throw std::runtime_error("Manual timer generation exhausted");
    const auto id=++serial_;
    if(milliseconds==0) {
        if(!PostMessageW(window_,WM_TIMER,id,0)) throw std::runtime_error("Cannot queue immediate timer");
    } else if(!SetTimer(window_,id,static_cast<UINT>(milliseconds),nullptr)) {
        throw std::runtime_error("Cannot schedule manual timer");
    }
    if(active_) KillTimer(window_,active_);
    active_=id;
}
LRESULT CALLBACK ManualTimer::dispatch(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_NCCREATE) SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
    auto self=reinterpret_cast<ManualTimer*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(self && message==WM_TIMER) {
        if(w!=self->active_) return 0; // A replaced timer's message may still be queued.
        try {
            auto lifetime=self->shared_from_this();
            KillTimer(window,self->active_); self->active_=0;
            auto callback=self->elapsed_;
            if(callback) callback(); // May close the window/release its external owner.
        } catch(...) { MessageBeep(MB_ICONERROR); }
        return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
}
