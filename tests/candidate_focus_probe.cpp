// Production Engine/SentenceSession; synthetic lexicon, no installed IME.
#include "Engine.h"
#include "LexiconSerialize.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tiger;
namespace {
unsigned checks=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
KeyResult press(Engine& e,int vk){KeyEvent k;k.vk=vk;auto r=e.process(k);k.down=false;e.process(k);return r;}
void type(Engine& e,const char* code){for(;*code;++code)press(e,*code-'a'+'A');}
SentenceDecodeResult result() {
    SentenceDecodeResult r;r.learningMode=u"focus-test";
    for(auto text:{u"龙族",u"陲机"}) {
        SentenceCandidate c;c.text=text;c.segmentedCode=u"aa bb";c.source=SentenceSourceComposed;
        c.boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{nullptr,2,4});
        r.candidates.push_back(std::move(c));
    }
    return r;
}
void shortcut(Engine& e) {
    const auto before=e.snapshot();KeyEvent k;
    k.vk=91;k.win=true;e.process(k);
    k.vk=160;k.shift=true;e.process(k);
    k.vk='S';auto r=e.process(k);
    require(!r.handled && !r.cancelComposition && r.commit.empty(),"Win shortcut cancelled composition");
    k.down=false;e.process(k);k.vk=91;k.win=false;e.process(k);
    k.vk=160;k.shift=false;e.process(k);
    const auto after=e.snapshot();
    require(after.raw==before.raw && after.mode==before.mode && after.total==before.total &&
        after.page==before.page && after.selectedCandidate==before.selectedCandidate,"Win shortcut changed candidates or selection");
}
void tests(const std::shared_ptr<const Dictionary>& dictionary) {
    Config config;config.maxCodeLength=16;config.maxCodeAutoCommit=false;config.pageSize=5;
    Engine ordinary(dictionary,config);type(ordinary,"aa");ordinary.setPage(1);
    require(ordinary.snapshot().page==1,"Fixture needs a second page");shortcut(ordinary);
    ordinary.focusChanged();ordinary.focusChanged();
    require(ordinary.snapshot().raw==u"aa" && ordinary.snapshot().page==1,"Focus reset ordinary candidate page");
    // The shell may swallow S entirely. Releasing Win before Shift must not
    // turn the remaining Shift-up into a language toggle.
    KeyEvent k;k.vk=91;k.win=true;ordinary.process(k);k.vk=160;k.shift=true;ordinary.process(k);
    k.down=false;k.vk=91;k.win=false;ordinary.process(k);k.vk=160;k.shift=false;
    auto up=ordinary.process(k);
    require(up.commit.empty() && ordinary.chinese() && ordinary.snapshot().raw==u"aa","Shell-consumed chord became a Shift toggle");
    k={};k.vk=160;k.shift=true;ordinary.process(k);ordinary.focusChanged();k.down=false;k.shift=false;up=ordinary.process(k);
    require(up.commit.empty() && ordinary.chinese() && ordinary.snapshot().raw==u"aa","Lost Shift-up toggled after focus reset");
    k={};k.vk='C';k.ctrl=true;const auto copy=ordinary.process(k);
    require(!copy.handled && copy.cancelComposition && !ordinary.composing(),"Ctrl editing shortcut behavior changed");
    Engine bound(dictionary,config);config.selection[91]=2;bound.configure(config);type(bound,"aa");
    k={};k.vk=91;k.win=true;
    require(bound.process(k).handled && !bound.composing(),"Explicit modifier selection binding was bypassed");
    Engine sentence(dictionary,config);sentence.enableSentenceInput(true,10);type(sentence,"aabb");
    auto ticket=*sentence.sentenceRequest();require(sentence.applySentenceResult(ticket,result()),"Fixture decode failed");
    press(sentence,9);require(sentence.snapshot().selectedCandidate==1,"Fixture Tab selection failed");shortcut(sentence);
    sentence.focusChanged();sentence.focusChanged();
    require(sentence.snapshot().selectedCandidate==1 && !sentence.sentenceDecodePending(),"Focus reset sentence selection or decode");
    require(sentence.sentenceRequest()->generation==ticket.generation,"Focus changed input generation");
    require(!sentence.applySentenceResult(ticket,result()) && sentence.snapshot().selectedCandidate==1,"Duplicate decode reset manual selection");
    const auto commit=press(sentence,32);
    require(commit.commit==u"陲机" && commit.learning.size()==1,"Focus lost the selected text or manual-learning evidence");
    type(sentence,"aabb");ticket=*sentence.sentenceRequest();sentence.focusChanged();
    require(sentence.applySentenceResult(ticket,result()),"Same-input background decode was discarded");
    sentence.cancel();type(sentence,"aabb");ticket=*sentence.sentenceRequest();sentence.focusChanged();press(sentence,'A');
    require(!sentence.applySentenceResult(ticket,result()),"Input edit accepted obsolete decode");
    ticket=*sentence.sentenceRequest();sentence.enableSentenceInput(true,11);
    require(!sentence.applySentenceResult(ticket,result()),"Resource change accepted obsolete decode");
    ticket=*sentence.sentenceRequest();sentence.cancel();
    require(!sentence.applySentenceResult(ticket,result()),"Cancellation accepted obsolete decode");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"candidate_focus_probe <new-fixture>");const std::filesystem::path path=std::filesystem::u8path(argv[1]);
        require(!std::filesystem::exists(path),"Fixture exists");
        ImportedLexicon data;data.main={{u"aa",{u"甲",u"乙",u"丙",u"丁",u"戊",u"己",u"庚"}},{u"bb",{u"中"}}};
        data.indexedMain={{u"aa",8,0},{u"bb",8,1}};const auto bytes=serializeImportedLexicon(data);
        {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(file),"Fixture write failed");}
        tests(Dictionary::Open(path));
        std::cout<<"{\"status\":\"passed\",\"focus_checks\":"<<checks<<",\"physical_input_tested\":false}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
