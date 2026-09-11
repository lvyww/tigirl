#pragma once
#include "Engine.h"
#include <string>
#include <vector>
namespace tiger {
struct CandidateStyle {
    bool vertical=true,showIndex=true,showCode=false,hideCandidates=false;
    std::u16string font=u"#霞鹜文楷 GB 屏幕阅读版";
    std::u16string codeMask;
    std::u16string theme=u"默认";
    double fontSize=17;
    int candidateDelayMs=0,annotationDelayMs=0;
    bool animationEnabled=true;
    int animationDurationMs=100;
    bool operator==(const CandidateStyle& other) const {
        return animationEnabled==other.animationEnabled && animationDurationMs==other.animationDurationMs && codeMask==other.codeMask && vertical==other.vertical && showIndex==other.showIndex && showCode==other.showCode &&
            candidateDelayMs==other.candidateDelayMs && annotationDelayMs==other.annotationDelayMs &&
            hideCandidates==other.hideCandidates && font==other.font && theme==other.theme && fontSize==other.fontSize;
    }
};
struct CandidatePresentation {
    std::u16string code;
    std::vector<std::u16string> items;
    bool codeOnly=false;
};
std::u16string displayComposition(const Snapshot& snapshot,const CandidateStyle& style);
CandidatePresentation presentCandidates(const Snapshot& snapshot,const CandidateStyle& style,bool showCandidates=true,bool includeAnnotations=true);
}
