#include "../native/Engine.h"
#include "../native/LexiconSerialize.h"
#include <fstream>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace tiger;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
class ContextModel final:public SentenceLanguageModel {
public:
 double logProbability(std::u16string_view p2,std::u16string_view p1,std::u16string_view target,bool) const override {
  return p2==u"乙" && p1==u"乙" && target==u"丙"?17.0:-1.0;
 }
 bool hasObservedBigram(std::u16string_view,std::u16string_view) const override{return false;}
};
int wmain(int argc,wchar_t** argv) {
 if(argc!=2)return 2;
 try {
  auto bytes=serializeImportedLexicon(prepareSentenceLexicon({{u"ab",{u"甲",u"乙乙"}},{u"cd",{u"丙"}},{u"ef",{u"丁"}},{u"cdef",{u"戊"}},{u"abcd",{u"己"}}}));
  {std::ofstream file(argv[1],std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(file),"write fixture");}
  auto dictionary=Dictionary::Open(argv[1]);auto lexicon=std::make_shared<SentenceLexicon>(dictionary);
  int cases=0;
  for(bool automatic:{false,true})for(bool synchronous:{false,true}) {
   SentenceDecoder decoder(lexicon);Engine engine(dictionary);engine.enableSentenceInput(true,1,automatic,synchronous?0:64);
   SentencePathQueries queries;
   queries.complete=[&](auto raw,auto prefix,auto excluded,bool grouped,const SentenceLockedPrefix* locked){return decoder.hasCompleteCandidate(raw,prefix,excluded,grouped,locked);};
   queries.properPrefix=[&](auto raw){return decoder.isProperCodePrefix(raw);};
   auto decode=[&] {if(auto t=engine.sentenceRequest())engine.applySentenceResult(*t,decoder.decode(t->raw,20,automatic,t->requiredPrefix,t->lockedPrefix));};
   auto tap=[&](int vk,bool shift=false) {
    KeyEvent key;key.vk=vk;key.shift=shift;
    auto result=engine.process(key,queries);
    if(result.awaitSentenceDecode){decode();result=engine.process(key,queries);require(!result.awaitSentenceDecode,"pending decode replay");}
    key.down=false;engine.process(key,queries);if(synchronous)decode();return result.commit;
   };
   auto type=[&](std::u16string_view raw){std::u16string output;for(auto c:raw)output+=tap(c-u'a'+'A');return output;};
   auto top=[&] {decode();auto s=engine.snapshot();require(!s.candidates.empty(),"missing candidates");return s.candidates[0].commit;};
   type(u"ab");require(tap(9).empty(),"Tab must not commit immediately");
   auto old=*engine.sentenceRequest();auto stale=decoder.decode(old.raw,20,automatic,old.requiredPrefix,old.lockedPrefix);
   require(tap('C')==(automatic?u"乙乙":u""),"letter confirms highlighted candidate");
   require(!engine.applySentenceResult(old,stale),"stale completion accepted after lock");
   tap('D');require(top()==(automatic?u"丙":u"乙乙丙"),"cross-boundary candidate leaked");
   require(tap(32)==(automatic?u"丙":u"乙乙丙"),"duplicate or wrong space commit");
   type(u"ab");tap(9);type(u"cd");tap(8);tap(8);
   if(automatic)require(!engine.sentenceRequest(),"backspace erased committed prefix context incorrectly");
   else require(top()==u"甲","backspace did not release lock");
   tap(27);type(u"ab");tap(9);type(u"cd");require(tap(13)==(automatic?u"cd":u"abcd"),"literal Enter includes committed text");
   type(u"ab");tap(9,true);require(tap('C')==(automatic?u"乙乙":u""),"Shift Tab lock");tap(27);
   type(u"ab");tap(9);tap('2');require(top()==u"乙乙","rank selector must edit current segment");tap(27);
   if(!automatic) {
    type(u"ab");tap(9);type(u"cd");tap(9);type(u"ef");require(top()==u"乙乙丙丁","stacked locks");
    tap(8);tap(8);require(top()==u"乙乙丙","previous lock lost");type(u"ef");decode();bool crossing=false;
    for(const auto& candidate:engine.snapshot().candidates)if(candidate.commit==u"乙乙戊")crossing=true;
    require(crossing,"latest lock not released");tap(27);
    type(u"abcd");decode();bool reset=false;for(const auto& candidate:engine.snapshot().candidates)if(candidate.commit==u"己")reset=true;
    require(reset,"escape did not reset locks");tap(27);
    type(u"ab");tap(40);type(u"cd");decode();bool arrows=false;for(const auto& candidate:engine.snapshot().candidates)if(candidate.commit==u"己")arrows=true;
    require(arrows,"arrow navigation unexpectedly locks");
   }
   ++cases;
  }
  SentenceDecoder decoder(lexicon);auto result=decoder.decode(u"ab");auto chosen=result.candidates.at(1);
  auto prefix=std::make_shared<SentenceLockedPrefix>(SentenceLockedPrefix{u"ab",chosen.text,chosen.boundary});
  require(decoder.hasCompleteCandidate(u"abcd",{},{},true,prefix.get()),"explicit lock group eligibility");
  require(!decoder.hasCompleteCandidate(u"abcd",{},u"乙乙丙",true,prefix.get()),"query crossed locked boundary");
  require(!decoder.hasCompleteCandidate(u"ab",{},u"乙乙",false,prefix.get()),"excluded fixed prefix");
  require(!decoder.hasCompleteCandidate(u"abcd",u"甲",{},false,prefix.get()),"required prefix mismatch");
  require(decoder.decode(u"ef",20,false,{},prefix).candidates.empty(),"mismatched raw lock");
  for(const auto& c:decoder.decode(u"abcd",20,true,{},prefix).candidates)require(c.text==u"乙乙丙" && c.segmentedCode==u"ab cd","decoder fixed boundary");
  SentenceDecoderOptions scoredOptions;scoredOptions.emittedCharacterReward=2;
  SentenceDecoder scored(lexicon,std::make_shared<ContextModel>(),scoredOptions);
  auto locked=scored.decode(u"abcd",20,false,{},prefix);
  auto explicitRank=scored.decode(u"ab2cd");
  require(locked.candidates.size()==1 && explicitRank.candidates.size()==1,"context fixture candidates");
  require(std::abs(locked.candidates[0].baseScore-explicitRank.candidates[0].baseScore)<1e-10,"locked prefix lost language model history");
  std::cout<<"{\"status\":\"passed\",\"engine_cases\":"<<cases<<",\"upstream\":\"14b611f\"}\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
