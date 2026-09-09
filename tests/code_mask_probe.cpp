#include "CodeMask.h"
#include "CandidatePresentation.h"
#include "Settings.h"
#include <iostream>
#include <stdexcept>
using namespace tiger;
#define check(v) do {if(!(v))throw std::runtime_error("Mask mismatch line " + std::to_string(__LINE__));} while(false)
int main(){
 check(maskInputCode(u"ab",u"")==u"ab");
 check(maskInputCode(u"ab",u"●")==u"●●");
 check(maskInputCode(u"abcd",u"甲乙丙")==u"甲乙丙甲");
 check(maskInputCode(u"aB;9!",u"甲乙丙")==u"甲乙丙甲甲");
 check(maskInputCode(u"ab",u"😀e\u0301")==u"😀e\u0301");
 check(maskInputCode(u"abc",u"👨‍👩‍👧‍👦🇨🇳")==u"👨‍👩‍👧‍👦🇨🇳👨‍👩‍👧‍👦");
 check(maskInputCode(u"a\t b\n\u3000c",u"●")==u"●\t ●\n\u3000●");
 check(maskInputCode(u"😀",u"●")==u"●●"); // Original iterates UTF-16 units of input.
 Snapshot s;s.raw=u"abcde";s.surface=u"已解析e";s.displayPrefixLength=3;s.candidates={{u"词",u"词",u"注"}};
 CandidateStyle style;style.codeMask=u"●";style.showCode=true;
 check(displayComposition(s,style)==u"已解析●");auto p=presentCandidates(s,style);
 check(p.code==u"已解析●" && p.items[0]==u"1 词〔注〕");check(s.raw==u"abcde" && s.surface==u"已解析e");
 s.surface=u"ab cd";s.displayPrefixLength=0;s.mode=Mode::Sentence;
 check(displayComposition(s,style)==u"●● ●●");
 check(parseCandidateStyle(u"编码伪装\t甲😀\n").codeMask==u"甲😀");
 check(parseCandidateStyle(u"编码伪装\t甲\n编码伪装\t\n").codeMask.empty());
 check(!(style==CandidateStyle{}));
 std::cout<<"Unicode masking, whitespace, prefix, sentence, parsing and unchanged candidate data passed\n";
}
