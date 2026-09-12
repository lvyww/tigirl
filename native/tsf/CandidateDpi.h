#pragma once
#include <windows.h>
#include <shellscalingapi.h>
#pragma comment(lib,"shcore.lib")
namespace tiger::tsf {
// Resolve the destination monitor before moving a visible candidate HWND.
inline UINT candidateMonitorDpi(HMONITOR monitor) {
    UINT x=96,y=96;
    return SUCCEEDED(GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&x,&y)) && x?x:96;
}
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
// TSF callbacks can arrive under a candidate/dispatcher window's DPI context.
// Query host text geometry and convert it before leaving the host's context;
// otherwise cached logical rectangles may be mistaken for physical pixels.
class CandidateHostDpiScope final {
public:
    explicit CandidateHostDpiScope(HWND owner):previous_(nullptr) {
        if(owner) {
            const auto context=GetWindowDpiAwarenessContext(owner);
            if(context)previous_=SetThreadDpiAwarenessContext(context);
        }
    }
    ~CandidateHostDpiScope(){if(previous_)SetThreadDpiAwarenessContext(previous_);}
    CandidateHostDpiScope(const CandidateHostDpiScope&)=delete;
    CandidateHostDpiScope& operator=(const CandidateHostDpiScope&)=delete;
private:
    DPI_AWARENESS_CONTEXT previous_;
};
inline RECT candidatePhysicalCaret(RECT caret,HWND owner) {
    if(owner && GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext())!=DPI_AWARENESS_PER_MONITOR_AWARE) {
        const auto awareness=GetWindowDpiAwarenessContext(owner);
        const auto dpi=GetDpiForWindow(owner);
        const auto root=GetAncestor(owner,GA_ROOT);
        // A TSF text extent may extend slightly outside its edit HWND. The
        // conversion API rejects out-of-window points (Everything: bottom +2).
        // Retry both ORIGINAL corners against a containing ancestor only when
        // it has the same coordinate space. Never reuse a half-converted pair.
        for(HWND window=owner;window;) {
            if(AreDpiAwarenessContextsEqual(awareness,GetWindowDpiAwarenessContext(window)) &&
               dpi && GetDpiForWindow(window)==dpi) {
                POINT first{caret.left,caret.top},last{caret.right,caret.bottom};
                if(LogicalToPhysicalPointForPerMonitorDPI(window,&first) && LogicalToPhysicalPointForPerMonitorDPI(window,&last))
                    return {first.x,first.y,last.x,last.y};
            }
            if(window==root)break;
            window=GetAncestor(window,GA_PARENT);
        }
        // Unknown geometry must not become a visible logical-pixel position.
        return {};
    }
    return caret;
}
}
