#pragma once
#include "Engine.h"
#include <string>
#include <vector>
namespace tiger {
struct CandidateStyle {
    int layoutMode=6;
    void setLayoutMode(int mode) {
        layoutMode=mode==2 || mode==4 || mode==5 || mode==6 || mode==7?mode:6;
        vertical=layoutMode==3 || layoutMode==4 || layoutMode==6;
        showCode=layoutMode<=4 || layoutMode==7;
        hideCandidates=layoutMode==7;
    }
    bool split() const { return false; }
    bool vertical=true,showIndex=true,showCode=false,hideCandidates=false;
    std::u16string font=u"#霞鹜文楷 GB 屏幕阅读版";
    std::u16string codeMask;
    std::u16string theme=u"默认.ssf";
    std::u16string skinRevision;
    bool skinFont=true,skinAnimation=true,skinEnabled=true;
    double fontSize=17;
    int candidateDelayMs=0,annotationDelayMs=0;
    bool animationEnabled=true;
    int animationDurationMs=100;
    bool operator==(const CandidateStyle& other) const {
        return layoutMode==other.layoutMode && skinRevision==other.skinRevision && skinFont==other.skinFont && skinAnimation==other.skinAnimation && skinEnabled==other.skinEnabled && animationEnabled==other.animationEnabled && animationDurationMs==other.animationDurationMs && codeMask==other.codeMask && vertical==other.vertical && showIndex==other.showIndex && showCode==other.showCode &&
            candidateDelayMs==other.candidateDelayMs && annotationDelayMs==other.annotationDelayMs &&
            hideCandidates==other.hideCandidates && font==other.font && theme==other.theme && fontSize==other.fontSize;
    }
};
struct CandidatePresentation {
    std::u16string code;
    std::u16string placeholder; // Empty-code display; never a selectable candidate.
    std::vector<std::u16string> items;
    bool codeOnly=false;
    std::vector<std::uint32_t> annotationOffsets;
};
std::u16string displayComposition(const Snapshot& snapshot,const CandidateStyle& style);
CandidatePresentation presentCandidates(const Snapshot& snapshot,const CandidateStyle& style,bool showCandidates=true,bool includeAnnotations=true);
}
