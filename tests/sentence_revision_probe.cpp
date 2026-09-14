// One probe compiled independently against old and new production sources.
// Old source runs fresh each time; new source reuses its real cache. Confidence
// fields fixed by this review are tested separately, not forced to match a bug.
#include "SentenceDecoder.h"
#include "SentenceSupplement.h"
#include "LexiconSerialize.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
using namespace tiger;
static void text(std::u16string_view value){std::cout<<value.size()<<':';for(auto c:value)std::cout<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c);std::cout<<std::dec<<' ';}
static void output(const SentenceDecodeResult& r){
    text(r.rawCode);std::cout<<r.learningAffected<<' ';text(r.learningMode);std::cout<<r.candidates.size()<<' ';
    for(const auto& c:r.candidates){text(c.text);text(c.segmentedCode);std::cout<<std::hexfloat<<c.baseScore<<' '<<c.finalScore<<' '<<c.confidenceScore<<' '<<c.supplementScore<<' '<<c.learningScore<<' '<<std::defaultfloat<<c.maxLexiconRank<<' '<<c.eligibleDuplicateSinglePath<<' ';
        for(auto b=c.boundary;b;b=b->previous)std::cout<<b->rawLength<<','<<b->textLength<<','<<std::hexfloat<<b->learningScore<<std::defaultfloat<<';';std::cout<<"| ";}
    std::cout<<'\n';
}
int main(int argc,char** argv){try{
    if(argc<4)return 2;std::filesystem::path root=argv[1];std::filesystem::create_directories(root);const bool fresh=std::string(argv[2])=="fresh";
    std::shared_ptr<const SentenceLanguageModel> model;if(std::string(argv[3])!="-")model=SentenceNgram::Open(argv[3]);
    std::vector<ImportedLexiconEntry> entries={{u"aa",{u"甲",u"乙",u"e\u0301"}},{u"bb",{u"国",u"\U00020000"}},{u"cc",{u"中",u"丙"}},{u"abcde",{u"天",u"天地"}},{u"dd",{u"丁"}}};
    auto data=serializeImportedLexicon(prepareSentenceLexicon(entries));auto path=root/"lexicon.tcd";{std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size());}
    auto lexicon=std::make_shared<SentenceLexicon>(Dictionary::Open(path));
    SentenceSupplementMatcher matcher({SentenceSupplementEntry::create(u"甲国",1000),SentenceSupplementEntry::create(u"e\u0301国",500)});
    data=serializeImportedLexicon(matcher.serializeGraph());path=root/"supplement.tcd";{std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size());}
    auto supplement=std::make_shared<MappedSentenceSupplement>(Dictionary::Open(path));
    std::mt19937 rng(440881);for(int beam:{1,8,64})for(bool duplicates:{false,true})for(bool learned:{false,true}){
        SentenceDecoderOptions o;o.beamWidth=beam;o.allowDuplicateSingleCharacters=duplicates;o.emittedCharacterReward=2;o.wholeInputSingleCharacterReward=5;o.isolationRankThreshold=1;
        SentenceDecoder d(lexicon,model,o,supplement);
        if(learned){SentenceLearningEvent e;e.id="fixture";e.mode=u"m";e.code=u"aabb";e.text=u"乙国";e.time=1700000000;d.setLearning(SentenceLearningSnapshot::build({e},e.time),u"m");}
        std::u16string raw;std::shared_ptr<SentenceLockedPrefix> lock;
        for(int i=0;i<240;++i){auto action=rng()%8;
            if(action<3 && raw.size()<60)raw+=std::u16string_view(u"aabbccdd")[rng()%8];
            else if(action==3 && !raw.empty())raw.pop_back();
            else if(action==4){raw=u"aabbccaabb";lock.reset();}
            else if(action==5){raw=u"aabbcc";lock=std::make_shared<SentenceLockedPrefix>();lock->rawCode=u"aa";lock->text=u"甲";lock->boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{nullptr,1,2,0});}
            else if(action==6)raw+=std::u16string_view(u";012'")[rng()%5];
            else if(action==7 && raw.size()>4)raw.erase(3,1);
            if(fresh)d.resetDecodeCache();output(d.decode(raw,1+rng()%20,i%2,u"",lock));
        }
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
