#include "SentenceLearningStore.h"
#include "LearningFileExclusion.h"
#include "SentenceSession.h"
#include "SentenceDecoder.h"
#include "LexiconSerialize.h"
#include "Engine.h"
#include "sentence_learning_performance.h"
#ifdef _WIN32
#include "LexiconOrder.h"
#endif
#include <iostream>
#include <fstream>
#include <stdexcept>
using namespace tiger;
static int checks=0;
static void check(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
static SentenceLearningEvent event(std::u16string code=u"aa",std::u16string text=u"乙",std::u16string context=u"",std::string id={}) {
    SentenceLearningEvent e;e.id=id.empty()?learningId():id;e.time=learningNow();e.mode=u"test-v1";e.code=std::move(code);e.text=std::move(text);e.context=std::move(context);return e;
}
static SentenceDecodeResult fixture(std::u16string raw,std::initializer_list<std::u16string> values) {
    SentenceDecodeResult r;r.rawCode=raw;r.learningMode=u"test-v1";
    for(const auto& text:values){SentenceCandidate c;c.text=text;c.segmentedCode=raw;c.maxLexiconRank=r.candidates.empty()?1:2;
        c.source=SentenceSourceComposed;
        for(std::size_t i=0;i<text.size();++i)c.boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{c.boundary,static_cast<int>(i+1),static_cast<int>((i+1)*2)});
        r.candidates.push_back(std::move(c));}return r;
}
static void apply(SentenceSession& s,SentenceDecodeResult r){check(s.apply(*s.request(),std::move(r)),"apply fixture");}
static std::shared_ptr<const Dictionary> save(const ImportedLexicon& lex,const std::filesystem::path& p) {
    auto data=serializeImportedLexicon(lex);{std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size());if(!f)throw std::runtime_error("write dictionary");}return Dictionary::Open(p);
}
static void pureTests() {
    check(learningCharacters(u"虎娘")==2,"unicode count");check(learningCharacters(u"𰻞娘")==2,"supplementary count");
    check(learningContext(u"前𰻞娘")==u"𰻞娘","context not surrogate half");
    check(!learningStaticText(std::u16string(17,u'中')),"long text rejected");
    check(learningStaticText(std::u16string(16,u'中')),"sixteen accepted");
    check(!learningStaticText(u"{日期}"),"dynamic rejected");check(!learningStaticText(u"显示\x1e输出"),"alias rejected");
    check(!learningStaticText(std::u16string(1,0xd800)),"malformed surrogate rejected");
    auto diff=sentenceLearningDiff(u"AAbbcc",u"设置虎狼窗口",u"设置虎娘窗口",{{2,2},{4,4},{6,6}},{{2,2},{4,4},{6,6}},0);
    check(diff.size()==1 && diff[0].text==u"虎娘" && diff[0].code==u"bb" && diff[0].context==u"设置","local change only");
    auto merged=sentenceLearningDiff(u"aabb",u"甲乙",u"甲丙",{{4,2}},{{2,1},{4,2}},0);
    check(merged.size()==1 && merged[0].code==u"aabb" && merged[0].text==u"甲丙","shared boundaries not character substring guesses");
    check(sentenceLearningDiff(u"aabb",u"甲乙",u"甲丙",{{4,2}},{{2,1},{4,2}},2).empty(),"do not cross locked floor");
    check(sentenceLearningDiff(u"aa",u"甲",u"乙",{{1,1}},{{2,1}},0).empty(),"incomplete boundary rejected");
    auto e=event(u"aabb",u"虎娘",u"设置");auto s=SentenceLearningSnapshot::build({e},e.time);
    check(s->score(e.mode,e.code,e.text,e.context)==9,"first correction equals supplement weight 1000");
    check(s->score(e.mode,e.code,e.text,u"其他")==6,"first correction cross-context preference");
    check(s->score(u"other-mode",e.code,e.text,e.context)==0,"mode isolation");
    check(s->prefixScore(e.mode,u"aa",u"虎",e.context)==9,"partial path hint");
    check(s->prefixScore(e.mode,u"aa",u"狼",e.context)==0,"wrong prefix no hint");
    auto persistent=SentenceLearningSnapshot::build({e},e.time+3650LL*86400);check(persistent->score(e.mode,e.code,e.text,e.context)==9,"learning has no time decay");
    auto second=e;second.id=learningId();auto third=e;third.id=learningId();third.context=u"测试";
    auto general=SentenceLearningSnapshot::build({e,second,third},e.time);
    check(general->score(e.mode,e.code,e.text,u"其他")==10,"three explicit corrections reach cross-context level three");
    auto single1=event(u"aa",u"乙",u"前甲"),single2=single1,single3=single1;single2.id=learningId();single3.id=learningId();single3.context=u"后甲";
    auto singles=SentenceLearningSnapshot::build({single1,single2,single3},single1.time);
    check(singles->score(single1.mode,u"aa",u"乙",u"别处")==10,"single-character fragments use explicit correction levels");
    auto alternative=e;alternative.id=learningId();alternative.text=u"虎爪";
    auto competition=SentenceLearningSnapshot::build({e,second,alternative},e.time);
    check(competition->score(e.mode,e.code,alternative.text,e.context)>competition->score(e.mode,e.code,e.text,e.context),"new correction beats older habit");
    std::vector<SentenceLearningEvent> repeated(100,e);auto cap=SentenceLearningSnapshot::build(repeated,e.time);
    check(cap->score(e.mode,e.code,e.text,e.context)==27 && cap->score(e.mode,e.code,e.text,u"其他")==24,"ten-level exact/general caps");
    for(int n=1;n<=40;++n) {
        auto learned=SentenceLearningSnapshot::build(std::vector<SentenceLearningEvent>(n,e),e.time);
        const int level=std::min(10,n);const double expected=7+2*level;
        check(std::abs(learned->score(e.mode,e.code,e.text,e.context)-expected)<1e-8,"repeated explicit corrections advance two points per level");
    }
    auto twice=SentenceLearningSnapshot::build({e,second},e.time);
    check(s->score(e.mode,e.code,e.text,e.context)<10.083 && twice->score(e.mode,e.code,e.text,e.context)>10.083,"two corrections overcome a gap above the old ten-point cap");
    check(competition->score(e.mode,e.code,e.text,e.context)==0,"competing manual correction can demote old local preference");
    auto old=SentenceLearningSnapshot::build({e},e.time+3650LL*86400);
    check(old->score(e.mode,e.code,e.text,e.context)==9,"long-term idle time preserves reward");
    for(auto name:{u"用户调整.txt",u".TIGIRL-USER.tsv.bak.txt",u".tigirl-learning.tsv",u".TIGIRL-LEARNING-v1.log.bak.txt",u".tigerclaw-learning-v1.log.tmp.dict.yaml",u".tigirl-learning.tsv.lock"})
        check(isLearningFile(name),"reserved basename including fake lexicon extensions");
    check(!isLearningFile(u"正常码表.txt") && !isLearningFile(u"tiger.txt"),"normal dictionaries retained");
}
#ifdef _WIN32
static void fileEnumerationTests(const std::filesystem::path& root) {
    auto dir=root/"enumeration";std::filesystem::create_directories(dir);
    for(auto name:{u"fixture.txt",u"用户调整.txt.bak.txt",u".tigirl-learning.tsv.bak.txt",u".TIGERCLAW-LEARNING-v1.log.tmp.dict.yaml"}) {
        std::ofstream out(dir/std::filesystem::path(name));out<<"aa\tword\n";
    }
    auto files=orderedLexiconFiles(dir,"zh-CN");
    check(files.size()==1 && files[0].filename()=="fixture.txt","real Windows table enumeration excludes learning files and backups");
}
#endif
static void sessionTests() {
    SentenceSession s;s.start(u"aa",1);apply(s,fixture(u"aa",{u"甲",u"乙"}));
    s.moveSelection(1,5,true);check(s.takeLearning().empty(),"Tab browsing no learning");s.clear();check(s.takeLearning().empty(),"cancel no learning");
    s.start(u"aa",1);apply(s,fixture(u"aa",{u"甲",u"乙"}));s.moveSelection(1,5,true);s.moveSelection(1,5,true);s.commitCandidate(0);
    check(s.takeLearning().empty(),"wrap back to initial top no learning");
    s.start(u"aa",1);apply(s,fixture(u"aa",{u"甲",u"乙"}));s.commitCandidate(0);check(s.takeLearning().empty(),"ordinary top use no learning");
    auto direct=fixture(u"aa",{u"甲",u"乙"});direct.candidates[0].source=direct.candidates[1].source=SentenceSourceDirect;
    direct.candidates[0].directRank=1;direct.candidates[1].directRank=2;
    s.start(u"aa",1);apply(s,direct);s.moveSelection(1,5,true);s.commitCandidate(1);
    check(s.takeLearning().empty(),"Direct-to-Direct manual choice never creates sentence learning");
    s.start(u"aa",1);apply(s,fixture(u"aa",{u"甲",u"乙"}));s.moveSelection(1,5,true);
    auto preview=s;preview.commitCandidate(1);check(preview.takeLearning().size()==1 && s.takeLearning().empty() && s.active(),"pure preview cannot persist");
    check(s.commitWithSuffix(u"。")==u"乙。","Tab punctuation output");auto events=s.takeLearning();check(events.size()==1 && events[0].code==u"aa" && events[0].text==u"乙","Tab punctuation learn once");
    check(s.takeLearning().empty(),"drain once");
    s.start(u"aa",1);apply(s,fixture(u"aa",{u"甲",u"乙"}));s.moveSelection(1,5,true);s.appendAutomatic(u'b',false,0,{});
    check(s.takeLearning().empty(),"locked but uncommitted no ready events");s.append(u'b');apply(s,fixture(u"aabb",{u"乙中",u"乙国"}));
    s.moveSelection(1,5,true);s.appendAutomatic(u'c',false,0,{});s.append(u'c');apply(s,fixture(u"aabbcc",{u"乙国人",u"乙国民"}));
    check(s.commitCandidate(0)==std::optional<std::u16string>(u"乙国人"),"multiple locks output");events=s.takeLearning();
    check(events.size()==2 && events[0].text==u"乙" && events[1].text==u"国","A AB ABC learn only A B, not repeated A or automatic C");
    check(events[0].rawEnd==2 && events[1].rawStart==2 && events[1].rawEnd==4,"incremental exact raw ranges");
    s.start(u"aa",1);apply(s,fixture(u"aa",{u"甲",u"乙"}));s.moveSelection(1,5,true);s.appendAutomatic(u'b',false,0,{});s.backspace();
    apply(s,fixture(u"aa",{u"甲",u"乙"}));s.commitCandidate(0);check(s.takeLearning().empty(),"backspace unlock discards pending");
    s.start(u"aa",1);apply(s,fixture(u"aa",{u"甲",u"乙"}));s.moveSelection(1,5,true);s.changeResources(2);
    apply(s,fixture(u"aa",{u"甲",u"乙"}));s.commitCandidate(1);check(s.takeLearning().empty(),"resource switch clears pending");
    auto learned=fixture(u"aa",{u"乙",u"甲"});learned.learningAffected=true;
    s.start(u"aa",2);apply(s,learned);check(s.commitCandidate(0)==std::optional<std::u16string>(u"乙"),"learned top normal commit");
    check(s.takeLearning().empty(),"learned top normal commit never reinforces");
    s.start(u"aa",2);apply(s,learned);int queries=0;SentencePathQueries q;
    q.complete=[&](auto,auto,auto,auto,auto){queries++;return false;};q.properPrefix=[&](auto){queries++;return false;};
    check(!s.appendAutomatic(u'b',true,0,q) && queries==0,"learning cannot prove empty code or probability auto commit");
    s.start(u"aa",2);apply(s,learned);s.moveSelection(1,5,true);
    check(s.appendAutomatic(u'b',true,0,q)==std::optional<std::u16string>(u"甲"),"explicit Tab confirmation still commits");check(s.takeLearning().size()==1,"explicit early commit produces event");
    s.clear();check(s.takeLearning().empty(),"later cancel does not repeat already drained event");
}
static void storageTests(const std::filesystem::path& folder) {
    const auto path=folder/".tigirl-learning.tsv";SentenceLearningStore store(path);store.refresh();check(!std::filesystem::exists(path),"read-only load does not create journal");
    auto a=event(),b=event(u"bb",u"国",u"乙");store.confirm({a});check(store.entries().size()==1,"successful confirmation persisted");
    store.confirm({a,a});check(store.entries().size()==1,"idempotent replays");
    SentenceLearningStore other(path);other.refresh();check(other.snapshot()->score(a.mode,a.code,a.text,a.context)>0,"second reader sees published record");
    std::ifstream input(path,std::ios::binary);std::string original((std::istreambuf_iterator<char>(input)),{});input.close();
    check(original.find(utf8(a.text))!=std::string::npos,"learning text is human readable UTF-8");
    {std::ofstream out(path,std::ios::binary);out<<"\xef\xbb\xbf# editable records\r\n"<<original.substr(0,original.size()-1);}
    store.confirm({b});check(store.entries().size()==2,"BOM, comment and no final newline preserved on append");
    std::ifstream saved(path,std::ios::binary);std::string valid((std::istreambuf_iterator<char>(saved)),{});saved.close();
    {std::ofstream out(path,std::ios::app);out<<"invalid row\n";}
    bool rejected=false;try{store.confirm({event()});}catch(...){rejected=true;}
    check(rejected,"malformed hand edit refused instead of discarded");
    {std::ofstream out(path,std::ios::binary);out<<valid;}
    check(store.undoLast() && store.entries().size()==1,"undo exact last event");
    store.confirm({b});check(store.entries().size()==1,"undone event cannot replay back");
    store.clear();check(store.entries().empty() && store.snapshot()->empty(),"clear learning only");store.confirm({a});check(store.entries().empty(),"clear tombstones old ids");
    auto bad=event();bad.text=u"{动态}";store.confirm({bad});check(store.entries().empty(),"dynamic records never persisted");
    auto fresh=event();store.confirm({fresh});check(store.entries().size()==1,"new events after clear");
    const auto blocker=folder/"not-a-directory";{std::ofstream out(blocker);out<<"x";}
    bool failed=false;try{SentenceLearningStore fail(blocker/".tigirl-learning.tsv");fail.confirm({fresh});}catch(...){failed=true;}
    check(failed && store.entries().size()==1,"write failure isolated");
}
static void decoderTests(const std::filesystem::path& folder) {
    auto prepared=prepareSentenceLexicon({{u"aa",{u"甲",u"乙",u"重庆"}},{u"bb",{u"中"}},{u"cc",{u"国"}}});
    auto dict=save(prepared,folder/"sentence.tcd");auto lex=std::make_shared<SentenceLexicon>(dict);
    SentenceDecoderOptions options;options.beamWidth=1;options.allowDuplicateSingleCharacters=true;options.isolationRankThreshold=0;
    SentenceDecoder decoder(lex,{},options);auto plain=decoder.decode(u"aabb",20,true);
    check(!plain.candidates.empty() && plain.candidates.front().text==u"甲中","baseline beam top");
    check(std::none_of(plain.candidates.begin(),plain.candidates.end(),[](auto& c){return c.text==u"乙中";}),"beam one originally prunes learned path");
    auto e=event(u"aabb",u"乙中");auto memory=SentenceLearningSnapshot::build({e});decoder.setLearning(memory,e.mode);
    auto learned=decoder.decode(u"aabb",20,true);
    check(!learned.candidates.empty() && learned.candidates.front().text==u"乙中","learned multi-edge path survives pruning and ranks first");
    check(learned.learningAffected && learned.earlyCommitEvidence.prefixes.empty() && learned.earlyCommitEvidence.confidenceTruncated,"learned candidate pool not auto confidence");
    check(std::abs(learned.candidates.front().finalScore-learned.candidates.front().baseScore-9)<1e-5,"learning reward exactly once");
    check(std::abs(learned.candidates.front().confidenceScore-learned.candidates.front().baseScore)<1e-5,"confidence excludes reward");
    // A complete confidence pool now lets learning contribute gradually to
    // ordinary early evidence while the model-only confidence remains intact.
    auto confidenceOptions=options;confidenceOptions.beamWidth=100;
    SentenceDecoder confidenceDecoder(lex,{},confidenceOptions);
    auto baselineConfidence=confidenceDecoder.decode(u"aabb",20,true);
    auto target=[](const SentenceDecodeResult& r)->const SentenceCandidate& {
        auto it=std::find_if(r.candidates.begin(),r.candidates.end(),[](const auto& c){return c.text==u"乙中";});
        if(it==r.candidates.end())throw std::runtime_error("learning confidence target missing");return *it;
    };
    const auto baseTarget=target(baselineConfidence);
    confidenceDecoder.setLearning(SentenceLearningSnapshot::build({e},e.time),e.mode);
    auto firstConfidence=confidenceDecoder.decode(u"aabb",20,true);const auto firstTarget=target(firstConfidence);
    check(firstConfidence.learningAffected && !firstConfidence.earlyCommitEvidence.prefixes.empty(),"learning keeps ordinary early evidence");
    check(std::abs(firstTarget.earlyCommitConfidenceScore-firstTarget.confidenceScore)<1e-12,"first correction adds zero early confidence");
    auto e2=e;e2.id=learningId();auto e3=e;e3.id=learningId();
    confidenceDecoder.setLearning(SentenceLearningSnapshot::build({e,e2},e.time),e.mode);
    const auto secondTarget=target(confidenceDecoder.decode(u"aabb",20,true));
    confidenceDecoder.setLearning(SentenceLearningSnapshot::build({e,e2,e3},e.time),e.mode);
    auto matureConfidence=confidenceDecoder.decode(u"aabb",20,true);const auto matureTarget=target(matureConfidence);
    check(secondTarget.earlyCommitConfidenceScore>firstTarget.earlyCommitConfidenceScore &&
        matureTarget.earlyCommitConfidenceScore>secondTarget.earlyCommitConfidenceScore,"learning early confidence matures progressively");
    check(std::abs(baseTarget.confidenceScore-matureTarget.confidenceScore)<1e-12,"mature learning does not alter model confidence");
    bool sawPersonalizedPrefix=false;
    for(const auto& prefix:matureConfidence.earlyCommitEvidence.prefixes)
        if(prefix.share>prefix.baseShare+1e-12){sawPersonalizedPrefix=true;break;}
    check(sawPersonalizedPrefix,"mature learning raises ordinary Share while retaining BaseShare");
    auto incremental=decoder.decode(u"aabbcc",20,true);check(incremental.candidates.front().text==u"乙中国","local preference survives added suffix");
    auto exact=decoder.decode(u"aa2bb",20,true);check(exact.candidates.front().text==u"乙中","explicit rank semantics unchanged");
    auto illegal=event(u"aabb",u"重庆中");decoder.setLearning(SentenceLearningSnapshot::build({illegal}),illegal.mode);
    auto prohibited=decoder.decode(u"aabb",20,true);check(std::none_of(prohibited.candidates.begin(),prohibited.candidates.end(),[](auto& c){return c.text==u"重庆中";}),"learning cannot create illegal non-first word edge");
    decoder.setLearning({},u"");auto restored=decoder.decode(u"aabb",20,true);
    check(restored.candidates.front().text==plain.candidates.front().text && !restored.learningAffected,"disabled learning restores baseline");
    auto single=event();decoder.setLearning(SentenceLearningSnapshot::build({single}),single.mode);auto first=decoder.decode(u"aa",20,true);
    check(first.candidates.front().text==u"甲" && (first.candidates.front().source&SentenceSourceDirect) &&
        std::all_of(first.candidates.begin(),first.candidates.end(),[](const auto& c){return c.learningScore==0;}),
        "same-code Direct correction cannot change table order");
    auto lock=std::make_shared<SentenceLockedPrefix>(SentenceLockedPrefix{u"aa",first.candidates.front().text,first.candidates.front().boundary});
    decoder.setLearning({},u"");auto locked=decoder.decode(u"aabb",20,true,u"",lock);
    check(locked.candidates.front().text==u"甲中" && locked.candidates.front().learningScore==0,"locked prefix never carries stale learning scores");

    std::vector<SentenceCandidate> merge;
    SentenceCandidate a;a.text=u"A";a.source=SentenceSourceComposed;merge.push_back(a);
    SentenceCandidate b;b.text=u"B";b.source=SentenceSourceDirect;b.directRank=1;merge.push_back(b);
    SentenceCandidate c;c.text=u"C";c.source=SentenceSourceDirect;c.directRank=2;merge.push_back(c);
    decoder.setLearning({},u"sentence-v2|test");decoder.applyFusionOrdering(u"ii",merge);
    check(merge[0].text==u"A" && merge[1].text==u"B" && merge[2].text==u"C","baseline cross-source order preserved");
    auto fusion=SentenceFusionPreference::event(u"sentence-v2|test",u"ii",u"C",u"A",true,2);
    decoder.setLearning(SentenceLearningSnapshot::build({fusion},fusion.time),u"sentence-v2|test");
    merge={};a={};a.text=u"A";a.source=SentenceSourceComposed;merge.push_back(a);
    b={};b.text=u"B";b.source=SentenceSourceDirect;b.directRank=1;merge.push_back(b);
    c={};c.text=u"C";c.source=SentenceSourceDirect;c.directRank=2;merge.push_back(c);
    decoder.applyFusionOrdering(u"ii",merge);
    check(merge[0].text==u"B" && merge[1].text==u"C" && merge[2].text==u"A",
        "Direct C over Composed A promotes only Direct prefix B,C");
}
static void engineTests(const std::filesystem::path& folder) {
    ImportedLexicon lex;lex.main={{u"aa",{u"甲",u"乙"}}};lex.indexedMain={{u"aa",8,0}};
    auto dict=save(lex,folder/"ordinary.tcd");Engine engine(dict);engine.enableSentenceInput(true,1);
    KeyEvent key;key.vk='A';engine.process(key);engine.process(key);
    auto r=fixture(u"aa",{u"甲",u"乙"});check(engine.applySentenceResult(*engine.sentenceRequest(),r),"engine publishes candidates");
    key.vk=9;engine.process(key);auto preview=engine;key.vk=32;auto result=preview.process(key);
    check(result.commit==u"乙" && result.learning.size()==1,"engine carries learning alongside space commit");
    check(engine.composing(),"TSF preview copy leaves live engine untouched");
    key.vk=27;auto cancel=engine.process(key);check(cancel.learning.empty(),"engine escape carries no learning");
}
int main(int argc,char** argv) {
    try {
        if(argc<3)return 2;const std::string mode=argv[1];const auto path=std::filesystem::u8path(argv[2]);
        if(mode=="worker") {if(argc!=5)return 2;SentenceLearningStore store(path);std::vector<SentenceLearningEvent> items;for(int i=0;i<std::stoi(argv[4]);i++)items.push_back(event(u"aa",u"乙",u"",std::string(argv[3])+"-"+std::to_string(i)));store.confirm(items);return 0;}
        if(mode=="count") {SentenceLearningStore store(path);std::cout<<store.entries().size()<<'\n';return 0;}
        std::filesystem::create_directories(path);
#ifdef _WIN32
        fileEnumerationTests(path);
#endif
        pureTests();sessionTests();storageTests(path);decoderTests(path);engineTests(path);
        checks+=learning_test::runPerformanceTests(path/"performance");
        std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"physical_tsf_tested\":false}\n";return 0;
    }catch(const std::exception& e){std::cerr<<"check "<<checks<<": "<<e.what()<<'\n';return 1;}
}
