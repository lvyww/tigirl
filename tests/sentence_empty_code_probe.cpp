#include "../native/Engine.h"
#include "../native/LexiconSerialize.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tiger;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
class PrefersTopic final:public SentenceLanguageModel {
public:
 double logProbability(std::u16string_view,std::u16string_view,std::u16string_view target,bool) const override {
  return target==u"题" || target==u"目"?20.0:0.0;
 }
 bool hasObservedBigram(std::u16string_view,std::u16string_view) const override{return false;}
};
int wmain(int argc,wchar_t** argv) {
 if(argc!=2)return 2;
 try {
  auto bytes=serializeImportedLexicon(prepareSentenceLexicon({
   {u"ot",{u"是",u"题",u"多字词"}},{u"qm",{u"目"}},
   {u"uv",{u"甲",u"\U00020000"}},{u"wx",{u"首",u"多字词"}},
   {u"yz",{u"乙",u"a\u0301"}}}));
  {std::ofstream file(argv[1],std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(file),"fixture write");}
  auto dictionary=Dictionary::Open(argv[1]);auto lexicon=std::make_shared<SentenceLexicon>(dictionary);
  int cases=0;
  for(bool duplicates:{false,true})for(int beam:{1,100})for(int limit:{1,20}) {
   SentenceDecoderOptions options;options.beamWidth=beam;options.allowDuplicateSingleCharacters=duplicates;
   SentenceDecoder decoder(lexicon,std::make_shared<PrefersTopic>(),options);
   require(decoder.hasCompleteCandidate(u"ot",{},u"是",true)==duplicates,"duplicate exact ambiguity");
   require(decoder.hasCompleteCandidate(u"uv",{},u"甲",true)==duplicates,"supplementary character eligibility");
   require(decoder.hasCompleteCandidate(u"yz",{},u"乙",true)==duplicates,"grapheme eligibility");
   require(!decoder.hasCompleteCandidate(u"wx",{},u"首",true),"implicit non-first word leaked");
   require(decoder.hasCompleteCandidate(u"ot3",{},{},true),"explicit word selector lost");
   const auto whole=decoder.decode(u"ot",20,true);
   require(!whole.candidates.empty(),"missing whole-input candidate");
   // A one-state beam can discard the rank-first character before final ordering.
   if(beam==100)require(whole.candidates.front().text==u"是","whole-input rank order changed");
   for(const auto& c:whole.candidates)if(c.text==u"多字词")require(!c.eligibleDuplicateSinglePath,"word marked as single");
   Engine engine(dictionary);engine.enableSentenceInput(true,1,true,0);
   SentencePathQueries queries;
   queries.complete=[&](auto raw,auto prefix,auto excluded,bool grouped,const SentenceLockedPrefix* locked){return decoder.hasCompleteCandidate(raw,prefix,excluded,grouped,locked);};
   queries.properPrefix=[&](auto raw){return decoder.isProperCodePrefix(raw);};
   auto tap=[&](int vk) {
    KeyEvent key;key.vk=vk;auto result=engine.process(key,queries);
    key.down=false;engine.process(key,queries);
    if(auto ticket=engine.sentenceRequest()){
     engine.applySentenceResult(*ticket,decoder.decode(ticket->raw,limit,true,ticket->requiredPrefix,ticket->lockedPrefix));
     result.commit+=engine.autoCommitSentence().commit;
    }
    return result.commit;
   };
   require(tap('O').empty() && tap('T').empty(),"premature initial commit");
   auto output=tap('Q');require(output==(duplicates?u"":u"是"),"otq wrong empty-code commit");
   output+=tap('M');
   auto view=engine.snapshot();require(!view.candidates.empty(),"missing sentence candidate");
   if(duplicates && beam==100)require(output+view.candidates.front().commit==u"题目","otqm lost topic candidate");
   if(!duplicates)require(output+view.candidates.front().commit==u"是目","disabled duplicate policy changed");
   auto expected=output+view.candidates.front().commit;
   output+=tap(32);require(output==expected,"space commit duplicated or lost text");
   ++cases;
  }
  std::cout<<"{\"status\":\"passed\",\"engine_cases\":"<<cases<<",\"upstream\":\"1784df1\"}\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
