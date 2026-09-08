#include "CandidatePresentation.h"
namespace tiger {
namespace {
std::u16string escape(std::u16string_view text) {
    std::u16string result;
    for(std::size_t i=0;i<text.size();++i) {
        auto c=text[i];
        if(c==u'\r') { if(i+1<text.size() && text[i+1]==u'\n') ++i; result+=u"\\n"; }
        else if(c==u'\n') result+=u"\\n";
        else if(c==u'\t') result+=u"\\t";
        else result+=c;
    }
    return result;
}
}
CandidatePresentation presentCandidates(const Snapshot& snapshot,const CandidateStyle& style) {
    CandidatePresentation result;
    if(style.showCode || (!style.hideCandidates && snapshot.candidates.empty())) result.code=escape(snapshot.compositionText());
    if(!style.hideCandidates) for(std::size_t i=0;i<snapshot.candidates.size();++i) {
        const auto& candidate=snapshot.candidates[i];
        std::u16string item;
        if(style.showIndex) { const auto n=std::to_string(i+1); item.assign(n.begin(),n.end()); item+=u' '; }
        item+=escape(candidate.display);
        if(!candidate.annotation.empty()) item+=u"〔"+escape(candidate.annotation)+u"〕";
        result.items.push_back(std::move(item));
    }
    return result;
}
}
