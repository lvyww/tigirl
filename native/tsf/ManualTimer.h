#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include <string>
namespace tiger::tsf {
// Thread-affine scheduler. Only actual TSF dispatch may schedule; previews
// carry a delay value without allocating or arming a Windows timer.
class ManualTimer final : public std::enable_shared_from_this<ManualTimer> {
public:
    static std::shared_ptr<ManualTimer> create(HINSTANCE module,std::function<void()> elapsed={},
        std::wstring classPrefix=L"NativeTiger.ManualTimer.");
    ~ManualTimer();
    void schedule(int milliseconds);
    void close();
private:
    ManualTimer(HINSTANCE,std::function<void()>,std::wstring classPrefix);
    static LRESULT CALLBACK dispatch(HWND,UINT,WPARAM,LPARAM);
    HINSTANCE module_;
    HWND window_=nullptr;
    UINT_PTR active_=0,serial_=0;
    std::wstring className_;
    std::function<void()> elapsed_;
};
}
