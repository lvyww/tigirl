#pragma once
#include <windows.h>
namespace tiger::tsf {
// Scope only the candidate's operations; never change the host process default.
class CandidateDpiScope final {
public:
    CandidateDpiScope():previous_(SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {}
    ~CandidateDpiScope(){if(previous_)SetThreadDpiAwarenessContext(previous_);}
    CandidateDpiScope(const CandidateDpiScope&)=delete;
    CandidateDpiScope& operator=(const CandidateDpiScope&)=delete;
private:
    DPI_AWARENESS_CONTEXT previous_;
};
inline RECT candidatePhysicalCaret(RECT caret,HWND owner) {
    if(owner && GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext())!=DPI_AWARENESS_PER_MONITOR_AWARE) {
        POINT first{caret.left,caret.top},last{caret.right,caret.bottom};
        if(LogicalToPhysicalPointForPerMonitorDPI(owner,&first) && LogicalToPhysicalPointForPerMonitorDPI(owner,&last))
            return {first.x,first.y,last.x,last.y};
    }
    return caret;
}
}
