#include "../native/Engine.h"
#include <iostream>
#include <stdexcept>
using namespace tiger;
int checks=0;
void check(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
KeyResult tap(Engine& e,int vk,bool shift=false) {
 KeyEvent k;k.vk=vk;k.shift=shift;auto r=e.process(k);k.down=false;e.process(k);return r;
}
SentenceDecodeResult decoded() {
 SentenceDecodeResult result;
 for(auto text:{u"中国",u"中华"}) {SentenceCandidate c;c.text=text;c.segmentedCode=u"aa bb";result.candidates.push_back(c);}
 return result;
}
int main(int argc,char** argv) {
 try {
  if(argc!=2)return 2;Engine e(Dictionary::Open(std::filesystem::u8path(argv[1])));
  e.enableSentenceInput(true,10);
  for(int key:{'A','A','B','B'})check(tap(e,key).handled,"raw letter");
  check(e.snapshot().mode==Mode::Sentence && e.snapshot().raw==u"aabb","sentence activation");
  auto ticket=*e.sentenceRequest();
  check(tap(e,32).awaitSentenceDecode && e.snapshot().raw==u"aabb","space waits current decode");
  check(e.applySentenceResult(ticket,decoded()),"apply decode");
  check(e.snapshot().surface==u"aa bb" && e.snapshot().candidates.size()==2,"published snapshot");
  tap(e,9);check(e.snapshot().selectedCandidate==1,"tab selection");
  check(!e.applySentenceResult(ticket,decoded()) && e.snapshot().selectedCandidate==1,"duplicate completion selection");
  check(tap(e,32).commit==u"中华" && e.snapshot().mode==Mode::Idle,"space selected commit");
  tap(e,'A');tap(e,'A');tap(e,'B');tap(e,'B');tap(e,'2');
  check(e.snapshot().raw==u"aabb2" && e.sentenceRequest()->raw==u"aabb2","digit suffix encoding");
  tap(e,186);tap(e,222);check(e.snapshot().raw==u"aabb2;'","punctuation selectors");
  tap(e,8);check(e.snapshot().raw==u"aabb2;","sentence backspace");
  check(tap(e,13).commit==u"aabb2;","literal enter");
  tap(e,'A');ticket=*e.sentenceRequest();tap(e,27);check(!e.applySentenceResult(ticket,decoded()),"cancel rejects completion");
  tap(e,'A');ticket=*e.sentenceRequest();auto preview=e;tap(preview,'B');
  check(e.snapshot().raw==u"a" && preview.snapshot().raw==u"ab","engine preview independent");
  check(!e.applySentenceResult(*preview.sentenceRequest(),decoded()),"preview generation not published");
  e.focusChanged();check(!e.applySentenceResult(ticket,decoded()),"focus invalidates worker generation");
  ticket=*e.sentenceRequest();
  e.switchSchema(e.lexicon(),Config{});check(e.snapshot().raw==u"a" && e.sentenceRequest().has_value(),"schema preserves sentence raw");
  check(!e.applySentenceResult(ticket,decoded()),"schema rejects old generation");
  ticket=*e.sentenceRequest();e.setLexicon(e.lexicon());check(!e.applySentenceResult(ticket,decoded()) && e.snapshot().candidates.empty(),"word change invalidation");
  ticket=*e.sentenceRequest();
  e.enableSentenceInput(true,11);check(!e.applySentenceResult(ticket,decoded()),"resource revision invalidation");
  e.applySentenceResult(*e.sentenceRequest(),decoded());check(tap(e,190).commit==u"中国。","punctuation commit");
  tap(e,'A');check(e.setChinese(false).commit==u"a" && !e.chinese(),"mode switch literal");
  e.setChinese(true);tap(e,'A');e.enableSentenceInput(false,12);check(!e.composing(),"disable cancels sentence");
  std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
