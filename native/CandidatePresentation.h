#pragma once
#include "Engine.h"
#include <string>
#include <vector>
namespace tiger {
struct CandidateStyle {
    bool vertical=true,showIndex=true,showCode=false,hideCandidates=false;
    std::u16string font=u"#霞鹜文楷 GB 屏幕阅读版";
    std::u16string theme=u"默认";
    double fontSize=17;
    bool operator==(const CandidateStyle& other) const {
        return vertical==other.vertical && showIndex==other.showIndex && showCode==other.showCode &&
            hideCandidates==other.hideCandidates && font==other.font && theme==other.theme && fontSize==other.fontSize;
    }
};
struct CandidatePresentation {
    std::u16string code;
    std::vector<std::u16string> items;
};
CandidatePresentation presentCandidates(const Snapshot& snapshot,const CandidateStyle& style);
}
