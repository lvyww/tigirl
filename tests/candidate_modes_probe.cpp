#include "../native/CandidatePresentation.h"
#include "../native/CandidateReveal.h"
#include "../native/Settings.h"
#include <stdexcept>
#include <iostream>
using namespace tiger;
void require(bool ok){if(!ok)throw std::runtime_error("Candidate mode regression");}
int main(){
    require(parseCandidateStyle(u"竖排候选\t否\n隐藏候选\t是\n候选窗显示编码\t是\n").layoutMode==6);
    require(parseCandidateStyle(u"皮肤动画\t否\n跟随皮肤字体\t否").skinAnimation);
    require(parseCandidateStyle(u"候选窗动效\t否\n候选窗动效时间(毫秒)\t321").animationEnabled);
    require(parseCandidateStyle(u"候选窗动效时间(毫秒)\t321").animationDurationMs==100);
    for(int mode:{2,4,5,6,7}){
        auto style=parseCandidateStyle(u"候选布局\t"+std::u16string(1,char16_t(u'0'+mode)));
        require(style.layoutMode==mode);require(style.vertical==(mode==3 || mode==4 || mode==6));
        Snapshot snapshot;snapshot.raw=u"ab";Candidate candidate;candidate.display=u"字";snapshot.candidates.push_back(candidate);
        auto p=presentCandidates(snapshot,style);require(p.items.size()==(mode==7?0u:1u));require(p.code.empty()==(mode>=5));require(p.placeholder.empty()==(mode!=7));
        auto delayed=presentCandidates(snapshot,style,false);require(delayed.items.empty() && (delayed.placeholder.empty()==(mode!=7)));
        style.candidateDelayMs=100;CandidateReveal reveal;reveal.update(snapshot,style,0);reveal.update(snapshot,style,200);
        require(reveal.presentation(snapshot,style).items.size()==(mode==7?0u:1u));
        snapshot.candidates.clear();p=presentCandidates(snapshot,style);require(p.items.empty());require((mode>=5)?p.placeholder==u"ab" && p.code.empty():p.code==u"ab" && p.placeholder.empty());
        reveal.update(snapshot,style,201);require(reveal.presentation(snapshot,style).placeholder==p.placeholder);
    }
    std::cout<<"Five modes: settings, old-key exclusion, visibility, empty-code placeholder and delay passed\n";
}
