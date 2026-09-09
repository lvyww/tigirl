#include "CandidateReveal.h"
#include "Settings.h"
#include <stdexcept>
#include <iostream>
using namespace tiger;
void check(bool yes) {if(!yes)throw std::runtime_error("Candidate reveal mismatch");}
int main() {
    CandidateStyle style;style.candidateDelayMs=250;style.annotationDelayMs=500;
    Snapshot s;s.mode=Mode::Composing;s.raw=u"ab";s.total=1;s.candidates={{u"交",u"交",u"拆分"}};
    CandidateReveal r;
    r.update(s,style,1000);check(r.presentation(s,style).items.empty());check(r.presentation(s,style).codeOnly);check(r.remaining(style,1000)==250);
    s.raw=u"abc";r.update(s,style,1249);check(r.presentation(s,style).items.empty());check(r.remaining(style,1249)==1);
    r.update(s,style,1250);check(r.presentation(s,style).items.at(0)==u"1 交");check(!r.presentation(s,style).codeOnly);check(r.remaining(style,1250)==250);
    r.update(s,style,1500);check(r.presentation(s,style).items.at(0)==u"1 交〔拆分〕");check(!r.remaining(style,1500));
    s.raw=u"abcd";r.update(s,style,1600);check(r.presentation(s,style).items.at(0)==u"1 交〔拆分〕");
    r.reset();r.update(s,style,2000);check(r.presentation(s,style).items.empty());
    style.showCode=true;check(r.presentation(s,style).code==u"abcd");
    style.hideCandidates=true;r.update(s,style,2250);check(!r.presentation(s,style).items.empty());
    style.candidateDelayMs=0;r.update(s,style,2300);check(r.presentation(s,style).items.empty());
    style.hideCandidates=false;style.showCode=false;style.annotationDelayMs=0;r.reset();r.update(s,style,3000);check(r.presentation(s,style).items.at(0)==u"1 交〔拆分〕");
    style.candidateDelayMs=500;style.annotationDelayMs=100;r.reset();r.update(s,style,4000);r.update(s,style,4100);check(r.presentation(s,style).items.empty());r.update(s,style,4500);check(r.presentation(s,style).items.at(0)==u"1 交〔拆分〕");
    style.candidateDelayMs=0;style.annotationDelayMs=500;s.mode=Mode::Pinyin;r.reset();r.update(s,style,5000);check(r.presentation(s,style).items.at(0)==u"1 交〔拆分〕");
    // TigerClaw latches an empty list/annotation as already expanded.
    s.mode=Mode::Composing;s.candidates.clear();style.candidateDelayMs=500;r.reset();r.update(s,style,6000);check(r.presentation(s,style).code==s.raw);
    s.candidates={{u"交",u"交",u"拆分"}};r.update(s,style,6001);check(r.presentation(s,style).items.at(0)==u"1 交〔拆分〕");
    s={};r.update(s,style,6100);check(!r.remaining(style,6100));
    for(auto key:{u"延时显示候选(毫秒)",u"延时展开注释和拆分(毫秒)"})for(auto pair:{std::pair{u"",0}, {u"0",0},{u"-1",0},{u"+250",250},{u"60000",60000},{u"60001",60000},{u"2147483647",60000},{u"2147483648",0},{u"1.5",0},{u"bad",0}}) {
        auto parsed=parseCandidateStyle(std::u16string(key)+u"\t"+pair.first);
        check((std::u16string(key)==u"延时显示候选(毫秒)"?parsed.candidateDelayMs:parsed.annotationDelayMs)==pair.second);
    }
    std::cout<<"candidate reveal timing, latching, reset, hidden/code-only, pinyin and parsing passed\n";
}
