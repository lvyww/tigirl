// Controlled synthetic work/CPU probe. Linux std::clock measurements only;
// not a production-model, TSF, storage-cold or physical key latency claim.
#include "SentenceDecoder.h"
#include "LexiconSerialize.h"
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace tiger;
static double milliseconds(std::clock_t begin){return 1000.0*(std::clock()-begin)/CLOCKS_PER_SEC;}
int main(int argc,char** argv){try{
    if(argc!=2)return 2;std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
    auto data=serializeImportedLexicon(prepareSentenceLexicon({{u"aa",{u"甲"}},{u"bb",{u"乙",u"丙"}}}));auto path=root/"lex.tcd";
    {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size());}
    auto lex=std::make_shared<SentenceLexicon>(Dictionary::Open(path));SentenceDecoderOptions o;o.allowDuplicateSingleCharacters=true;o.isolationLambda=0;
    auto lock=std::make_shared<SentenceLockedPrefix>();lock->rawCode=u"aa";lock->text=u"甲";lock->boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{nullptr,1,2,0});
    std::cout<<std::setprecision(8);
    for(int length:{80,128}) {
        SentenceDecoder decoder(lex,{},o);std::u16string raw=u"aa";while(static_cast<int>(raw.size())<length)raw+=u"bb";
        decoder.decode(raw,20,true,u"",lock);constexpr int rounds=20;double repeated=0,append=0,back=0;long long expandedRepeat=0,expandedAppend=0,expandedBack=0;
        for(int i=0;i<rounds;++i){auto start=std::clock();auto r=decoder.decode(raw,20,true,u"",lock);repeated+=milliseconds(start);expandedRepeat+=r.expandedStates;
            start=std::clock();r=decoder.decode(raw+u"b",20,true,u"",lock);append+=milliseconds(start);expandedAppend+=r.expandedStates;
            start=std::clock();r=decoder.decode(raw,20,true,u"",lock);back+=milliseconds(start);expandedBack+=r.expandedStates;}
        std::cout<<"{\"test\":\"locked_tail\",\"length\":"<<length<<",\"repeat_ms\":"<<repeated/rounds<<",\"append_ms\":"<<append/rounds<<",\"backspace_ms\":"<<back/rounds<<",\"repeat_expanded\":"<<expandedRepeat/rounds<<",\"append_expanded\":"<<expandedAppend/rounds<<",\"backspace_expanded\":"<<expandedBack/rounds<<"}\n";
    }
    std::vector<SentenceLearningEvent> events;events.reserve(9050);
    for(int i=0;i<9000;++i){SentenceLearningEvent e;e.id=std::to_string(i);e.time=1700000000;e.mode=u"m";e.text=u"虎娘";e.context=u"上下";e.code=u"aaaa";int n=i;for(auto& c:e.code){c+=n%26;n/=26;}events.push_back(e);}
    auto snapshot=SentenceLearningSnapshot::build(events,1700000000);
#ifdef TIGIRL_REVIEW_NEW
    SentenceLearningAccumulator replay;snapshot=replay.update(events,1700000000);
#endif
    auto start=std::clock();volatile double sink=0;constexpr int rounds=20;
    for(int i=0;i<rounds;++i){auto e=events.front();e.id="new-"+std::to_string(i);events.push_back(e);
#ifdef TIGIRL_REVIEW_NEW
        snapshot=replay.update(events,1700000000);
#else
        snapshot=SentenceLearningSnapshot::build(events,1700000000);
#endif
        sink+=snapshot->score(u"m",u"aaaa",u"虎娘",u"上下");}
    std::cout<<"{\"test\":\"learning_update\",\"initial_events\":9000,\"updates\":20,\"ms_per_update\":"<<milliseconds(start)/rounds<<",\"score_sum\":"<<sink<<",\"includes_journal_io\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
