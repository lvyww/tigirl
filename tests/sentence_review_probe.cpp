// Regression and work-count tests for the 4f88a4e sentence review.
// Independent synthetic resources only; not a Windows TSF / physical-key test.
#include "SentenceDecoder.h"
#include "SentenceSession.h"
#include "SentenceLearningStore.h"
#include "LexiconSerialize.h"
#include "SentenceSupplement.h"
#include <cstring>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
using namespace tiger;
namespace fs=std::filesystem;
static fs::path root;
static fs::path lexicalPath;
static int serial=0,checks=0;
static void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static auto lex(std::vector<ImportedLexiconEntry> source) {
    auto bytes=serializeImportedLexicon(prepareSentenceLexicon(source));auto path=root/("lex-"+std::to_string(++serial)+".tcs");
    {std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
    return std::make_shared<SentenceLexicon>(Dictionary::Open(path));
}
static SentenceDecoderOptions options(int beam=2000) {
    SentenceDecoderOptions o;o.beamWidth=beam;o.isolationLambda=0;o.rankPenalty=0;o.allowDuplicateSingleCharacters=true;return o;
}
struct Model:SentenceLanguageModel {
    mutable std::size_t calls=0;
    std::shared_ptr<std::atomic<bool>> cancel;
    std::size_t cancelAfter=0;
    double logProbability(std::u16string_view a,std::u16string_view b,std::u16string_view c,bool=true) const override {
        ++calls;if(cancel && cancelAfter && calls>=cancelAfter)cancel->store(true);
        return c==u"乙"?-.1:0;
    }
    bool hasObservedBigram(std::u16string_view a,std::u16string_view b) const override {return a==u"甲" && b==u"乙";}
};
struct FlatModel:SentenceLanguageModel {
    double logProbability(std::u16string_view,std::u16string_view,std::u16string_view,bool=true) const override{return 0;}
    bool hasObservedBigram(std::u16string_view,std::u16string_view) const override{return false;}
};
struct RankingConflictModel:SentenceLanguageModel {
    double logProbability(std::u16string_view,std::u16string_view,std::u16string_view target,bool=true) const override {
        return target==u"鼎"?-7.0:0.0;
    }
    bool hasObservedBigram(std::u16string_view,std::u16string_view) const override{return false;}
};
static bool boundaries(std::shared_ptr<const SentencePathBoundary> a,std::shared_ptr<const SentencePathBoundary> b) {
    while(a && b){if(a->textLength!=b->textLength || a->rawLength!=b->rawLength || a->learningScore!=b->learningScore ||
        a->codeScore!=b->codeScore || a->protectsRareCharacter!=b->protectsRareCharacter || a->codeLength!=b->codeLength ||
        a->learningReward!=b->learningReward || a->learningRawStart!=b->learningRawStart || a->learningTextStart!=b->learningTextStart)return false;
        a=a->previous;b=b->previous;}return !a && !b;
}
static bool candidate(const SentenceCandidate& a,const SentenceCandidate& b) {
    const bool earlyEqual=a.earlyCommitConfidenceScore==b.earlyCommitConfidenceScore ||
        (std::isnan(a.earlyCommitConfidenceScore) && std::isnan(b.earlyCommitConfidenceScore));
    return a.text==b.text && a.segmentedCode==b.segmentedCode && a.baseScore==b.baseScore && a.finalScore==b.finalScore &&
        a.confidenceScore==b.confidenceScore && earlyEqual && a.supplementScore==b.supplementScore && a.learningScore==b.learningScore &&
        a.codeScore==b.codeScore && a.lexicalScore==b.lexicalScore &&
        a.maxLexiconRank==b.maxLexiconRank && a.eligibleDuplicateSinglePath==b.eligibleDuplicateSinglePath && boundaries(a.boundary,b.boundary);
}
static bool equal(const SentenceDecodeResult& a,const SentenceDecodeResult& b) {
    if(a.rawCode!=b.rawCode || a.learningAffected!=b.learningAffected || a.learningMode!=b.learningMode || a.candidates.size()!=b.candidates.size())return false;
    for(std::size_t i=0;i<a.candidates.size();++i)if(!candidate(a.candidates[i],b.candidates[i]))return false;
    if(a.confidencePool().size()!=b.confidencePool().size())return false;
    for(std::size_t i=0;i<a.confidencePool().size();++i)if(!candidate(a.confidencePool()[i],b.confidencePool()[i]))return false;
    const auto& x=a.earlyCommitEvidence;const auto& y=b.earlyCommitEvidence;
    if(x.confidenceTruncated!=y.confidenceTruncated || x.mergedIncompleteTail!=y.mergedIncompleteTail || x.neutralIncompleteTail!=y.neutralIncompleteTail ||
       x.neutralLowConfidence!=y.neutralLowConfidence || x.proposal!=y.proposal || x.proposalShare!=y.proposalShare || x.rawLengths!=y.rawLengths || x.prefixes.size()!=y.prefixes.size())return false;
    for(std::size_t i=0;i<x.prefixes.size();++i){const auto& p=x.prefixes[i];const auto& q=y.prefixes[i];
        const bool baseEqual=p.baseShare==q.baseShare || (std::isnan(p.baseShare) && std::isnan(q.baseShare));
        if(p.text!=q.text || p.rawLength!=q.rawLength || p.share!=q.share || !baseEqual || p.boundaryShare!=q.boundaryShare || p.boundaryClosed!=q.boundaryClosed)return false;}
    return true;
}
static double share(const SentenceDecodeResult& r,std::u16string_view text){for(const auto& p:r.earlyCommitEvidence.prefixes)if(p.text==text)return p.share;return -1;}
static void correctness() {
    {auto l=lex({{u"abcde",{u"甲",u"乙丙"}},{u"fg",{u"丁"}}});auto o=options();o.allowDuplicateSingleCharacters=false;o.emittedCharacterReward=2;o.wholeInputSingleCharacterReward=5;
     SentenceDecoder d(l,{},o);d.decode(u"abcde");auto a=d.decode(u"abcdefg",20,true);check(equal(a,d.decodeFull(u"abcdefg",20,true)),"C3 long code cache parity");check(a.candidates.size()==1 && a.candidates.front().finalScore==4,"C3 no whole-input reward leak");}
    {std::vector<std::u16string> tails;for(char16_t c=0x4e00;c<0x4e14;++c)tails.emplace_back(1,c);
     auto l=lex({{u"aa",{u"甲",u"乙"}},{u"bb",tails},{u"cc",{u"国"}}});auto m=std::make_shared<Model>();SentenceDecoder d(l,m,options());
     auto a=d.decode(u"aabbcc",20,true);auto b=d.decodeFull(u"aabbcc",40,true);
     check(a.candidates.size()==20 && a.confidencePool().size()==40,"C1 menu and confidence separated");
     check(share(a,u"甲")==share(b,u"甲") && std::abs(share(a,u"甲")-.52497918747894)<1e-12,"C1 hidden competitors retain mass");
     auto pool=a.confidenceCandidates;d.decode(u"aabbccbb");check(pool->size()==40,"immutable published pool survives next generation");
     SentenceSession session;session.start(u"aabbcc",1);auto one=d.decodeFull(u"aabbcc",1,true);session.apply(*session.request(),one);
     SentencePathQueries queries;queries.complete=[](auto,auto,auto,bool,const auto*){return false;};queries.properPrefix=[](auto){return false;};
     check(!session.appendAutomatic(u'x',true,0,queries),"C1 one visible candidate is not uniqueness");
     auto noMenu=one;noMenu.candidates.clear();session.start(u"aabbcc",1);session.apply(*session.request(),noMenu);
     check(!session.appendAutomatic(u'x',true,0,queries),"C1 hidden full pool cannot dereference empty menu");
     noMenu.confidenceCandidates=std::make_shared<const std::vector<SentenceCandidate>>(std::vector<SentenceCandidate>{one.confidencePool().front()});
     session.start(u"aabbcc",1);session.apply(*session.request(),noMenu);
     check(!session.appendAutomatic(u'x',true,0,queries),"C1 hidden singleton is not an exposed selection");}
    {auto l=lex({{u"aa",{u"甲",u"乙"}},{u"bb",{u"中"}},{u"cc",{u"国"}}});SentenceDecoder d(l,{},options(1));
     auto a=d.decode(u"aabbcc",20,true);check(a.earlyCommitEvidence.confidenceTruncated,"C2 ancestor truncation propagated");
     SentenceSession session;session.start(u"aabbcc",1);session.apply(*session.request(),a);SentencePathQueries queries;
     queries.complete=[](auto,auto,auto,bool,const auto*){return false;};queries.properPrefix=[](auto){return false;};
     check(!session.appendAutomatic(u'x',true,0,queries),"C2 truncated singleton cannot authorize empty-code commit");}
    {std::vector<std::u16string> v;for(char16_t c=0x4e00;c<0x4e00+124;++c)v.emplace_back(1,c);
     auto l=lex({{u"aa",{u"甲"}},{u"bb",{u"乙"}},{u"cc",v}});SentenceDecoder d(l,{},options());
     for(const auto* raw:{u"aabbcc123",u"aabbcc12",u"aabbcc1",u"aabbcc0",u"aabbcc10",u"aabbcc;",u"aabbcc'",u"aabbcc"})
         check(equal(d.decode(raw,20,true),d.decodeFull(raw,20,true)),"C4 skipped selector generations parity");}
    {auto l=lex({{u"aa",{u"甲"}},{u"bb",{u"乙"}},{u"cc",{u"丙"}},{u"dd",{u"丁"}}});SentenceDecoder d(l,{},options());
     SentenceLearningEvent e;e.id="c5";e.mode=u"m";e.code=u"dd";e.text=u"丁";e.context=u"乙丙";e.time=1700000000;d.setLearning(SentenceLearningSnapshot::build({e},e.time),u"m");
     d.decode(u"aabbcc");check(d.decode(u"aabbccdd",20,true).learningAffected,"C5 learned suffix reached");auto a=d.decode(u"aabbcc",20,true);
     check(!a.learningAffected && equal(a,d.decodeFull(u"aabbcc",20,true)),"C5 learning flags rolled back");}
    {auto l=lex({{u"nv",{u"有"}},{u"nvt",{u"郁"}},{u"tah",{u"衅"}},{u"ahx",{u"闷"}}});SentenceDecoder d(l,{},options());
     check(d.competingBoundaryEnd(u"jreynvtah",4,6,1)==7,"C6 nv boundary must protect aligned nvt split");
     check(d.competingBoundaryEnd(u"jreynvtahx",4,7,1)==7,"C6 second element must not delay one-element boundary");
     check(d.competingBoundaryEnd(u"jreynvtahx",4,9,2)==10,"C6 two-element paths must align at nvt|ahx");}
}
static void rankingPriors() {
    auto model=std::make_shared<FlatModel>();
    auto codeLexicon=lex({{u"xy",{u"甲"}},{u"ab",{u"甲"}},{u"cd",{u"乙"}},{u"abcd",{u"鼎"}}});
    auto baseOptions=options(100);SentenceDecoder baseline(codeLexicon,model,baseOptions);
    auto shapedOptions=baseOptions;shapedOptions.canonicalCodeReward=2;SentenceDecoder shaped(codeLexicon,model,shapedOptions);
    auto plain=baseline.decodeFull(u"abcd"),ranked=shaped.decodeFull(u"abcd");
    check(plain.candidates.front().text==u"甲乙" && ranked.candidates.front().text==u"鼎","R1 primary-code evidence reranks final candidates");
    check(plain.expandedStates==ranked.expandedStates && plain.candidates.size()==ranked.candidates.size(),"R1 Beam work/candidate set unchanged");
    for(const auto& original:plain.candidates) {
        auto changed=std::find_if(ranked.candidates.begin(),ranked.candidates.end(),[&](const auto& value){return value.text==original.text;});
        check(changed!=ranked.candidates.end() && changed->confidenceScore==original.confidenceScore,"R1 confidence excludes code evidence");
    }

    // The old policy accumulated confidence for 甲乙 and committed it on the
    // third generation even though canonical-code evidence displayed 鼎丁.
    // Reproduce the conflict through the real decoder and session boundary.
    auto conflictLexicon=lex({{u"xy",{u"甲"}},{u"ab",{u"甲"}},{u"uv",{u"乙"}},{u"cd",{u"乙"}},
        {u"abcd",{u"鼎"}},{u"ef",{u"丁",u"丙"}},{u"efg",{u"丁",u"丙"}},{u"efgh",{u"丁",u"丙"}}});
    auto conflictOptions=options(100);conflictOptions.canonicalCodeReward=2;
    SentenceDecoder conflictDecoder(conflictLexicon,std::make_shared<RankingConflictModel>(),conflictOptions);
    SentenceSession conflictSession;conflictSession.start(u"abcdef",1);
    for(const auto* raw:{u"abcdef",u"abcdefg",u"abcdefgh"}) {
        if(conflictSession.raw()!=raw)check(conflictSession.append(raw[std::char_traits<char16_t>::length(raw)-1]),"R1 conflict append");
        auto result=conflictDecoder.decodeFull(raw,20,true);
        check(!result.candidates.empty() && result.candidates.front().text==u"鼎丁","R1 final prior top is displayed");
        const auto supported=std::find_if(result.earlyCommitEvidence.prefixes.begin(),result.earlyCommitEvidence.prefixes.end(),
            [](const auto& prefix){return prefix.text==u"甲乙" && prefix.rawLength==4;});
        check(supported!=result.earlyCommitEvidence.prefixes.end() && supported->share>=.995 && supported->share<.99999,
            "R1 conflicting confidence fixture");
        check(conflictSession.apply(*conflictSession.request(),std::move(result)),"R1 conflict result apply");
        check(!conflictSession.tryAutoCommit(true),"R1 auto commit must follow displayed top");
    }

    auto rareFour=lex({{u"abcd",{u"揸"}}});auto rareBaseOptions=options();rareBaseOptions.isolationLambda=2;rareBaseOptions.isolationRankThreshold=3000;
    auto rareProtectedOptions=rareBaseOptions;rareProtectedOptions.canonicalIsolationFactor=0;rareProtectedOptions.canonicalIsolationMinCodeLength=4;
    SentenceDecoder rareBase(rareFour,model,rareBaseOptions),rareProtected(rareFour,model,rareProtectedOptions);
    auto rarePlain=rareBase.decodeFull(u"abcd").candidates.front(),rareRanked=rareProtected.decodeFull(u"abcd").candidates.front();
    check(std::abs((rareRanked.finalScore-rarePlain.finalScore)-2)<1e-12 && rareRanked.confidenceScore==rarePlain.confidenceScore,
        "R2 four-code primary rare character protected outside confidence");
    auto rareThree=lex({{u"abc",{u"揸"}}});SentenceDecoder rareThreeBase(rareThree,model,rareBaseOptions),rareThreeProtected(rareThree,model,rareProtectedOptions);
    check(rareThreeBase.decodeFull(u"abc").candidates.front().finalScore==rareThreeProtected.decodeFull(u"abc").candidates.front().finalScore,
        "R2 three-code rare character remains penalized");

    auto prior=SentenceLexicalPrior::Open(lexicalPath);
    check(prior && prior->byteCount()==150032 && prior->entryCount()==50000 && prior->contains(u"中国") && !prior->contains(u"一乙"),
        "R3 production lexical prior parses and hashes exactly");
    auto lexicalLexicon=lex({{u"ab",{u"一"}},{u"cd",{u"乙"}},{u"abcd",{u"中国"}}});
    SentenceDecoder lexicalBase(lexicalLexicon,model,baseOptions);
    auto lexicalOptions=baseOptions;lexicalOptions.lexicalPriorWeight=.1;lexicalOptions.lexicalCandidateLimit=5;
    SentenceDecoder lexicalRanked(lexicalLexicon,model,lexicalOptions,{},prior);
    auto lexicalPlain=lexicalBase.decodeFull(u"abcd"),lexicalResult=lexicalRanked.decodeFull(u"abcd");
    check(lexicalPlain.candidates.front().text==u"一乙" && lexicalResult.candidates.front().text==u"中国","R3 lexical prior reranks original Top-5");
    for(const auto& original:lexicalPlain.candidates) {
        auto changed=std::find_if(lexicalResult.candidates.begin(),lexicalResult.candidates.end(),[&](const auto& value){return value.text==original.text;});
        check(changed!=lexicalResult.candidates.end() && changed->confidenceScore==original.confidenceScore,"R3 confidence excludes lexical evidence");
    }

    SentenceDecoder noModel(codeLexicon,{},shapedOptions,{},prior);
    check(noModel.decodeFull(u"abcd").candidates.front().text==u"甲乙","R4 ranking priors disabled without n-gram model");

    auto lockedLexicon=lex({{u"xy",{u"甲"}},{u"ab",{u"甲"}},{u"cd",{u"乙"}},{u"abcd",{u"鼎"}},{u"ef",{u"丁"}}});
    auto source=ranked.candidates.front();auto lockedPrefix=std::make_shared<SentenceLockedPrefix>();
    lockedPrefix->rawCode=u"abcd";lockedPrefix->text=u"鼎";lockedPrefix->boundary=source.boundary;
    SentenceDecoder lockedDecoder(lockedLexicon,model,shapedOptions);
    auto lockedResult=lockedDecoder.decode(u"abcdef",20,false,u"",lockedPrefix);
    auto lockedCandidate=std::find_if(lockedResult.candidates.begin(),lockedResult.candidates.end(),[](const auto& value){return value.text==u"鼎丁";});
    check(lockedCandidate!=lockedResult.candidates.end() && std::abs(lockedCandidate->codeScore-12)<1e-12,
        "R5 locked prefix preserves ranking-only code evidence");
}
static auto locked() {
    auto lock=std::make_shared<SentenceLockedPrefix>();lock->rawCode=u"aa";lock->text=u"甲";
    lock->boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{nullptr,1,2,0});return lock;
}
static void caching() {
    auto l=lex({{u"aa",{u"甲"}},{u"bb",{u"乙",u"丙"}}});auto m=std::make_shared<Model>();SentenceDecoder d(l,m,options(64));auto lock=locked();
    std::u16string raw=u"aa";while(raw.size()<80)raw+=u"bb";
    d.decode(raw,20,false,u"",lock);m->calls=0;auto a=d.decode(raw,20,false,u"",lock);
    check(!m->calls && !a.expandedStates,"P1 locked identical request does no scoring");
    auto full=a.confidenceCandidates;m->calls=0;auto upgraded=d.decode(raw,20,true,u"",lock);
    check(!m->calls && upgraded.confidenceCandidates==full,"P4 evidence upgrade reuses scored pool");
    auto append=d.decode(raw+u"bb",20,true,u"",lock);check(append.expandedStates<300,"P1 locked append extends only tail");
    SentenceDecoder oracle(l,m,options(64));check(equal(append,oracle.decode(raw+u"bb",20,true,u"",lock)),"P1 locked append fresh parity");
    auto back=d.decode(raw,20,true,u"",lock);check(!back.expandedStates,"P1 safe locked backspace no expansion");oracle.resetDecodeCache();check(equal(back,oracle.decode(raw,20,true,u"",lock)),"P1 locked backspace fresh parity");
    std::weak_ptr<SentenceLockedPrefix> lifetime=lock;lock.reset();check(!lifetime.expired(),"P1 cache owns lock string views");d.resetDecodeCache();oracle.resetDecodeCache();check(lifetime.expired(),"P1 old lock released with caches");
    auto held=l->candidateView(u"bb");check(l->candidateView(u"BB")==held,"P3 folded metadata cache hit");
    for(int i=0;i<600;++i){auto code=u"nonexistent"+std::u16string(1,static_cast<char16_t>(i+100));l->candidateView(code);l->isProperCodePrefix(code);}
    auto again=l->candidateView(u"bb");check(again->size()==held->size() && again->at(1).text==held->at(1).text && again->at(1).logRank==held->at(1).logRank,"P3 eviction keeps metadata exact");
    check(l->isProperCodePrefix(u"b") && !l->isProperCodePrefix(u"bb"),"P3 proper prefix exact boundary");
    std::vector<std::u16string> vals;for(char16_t c=0x4e00;c<0x4e40;++c)vals.emplace_back(1,c);
    SentenceDecoder dense(lex({{u"aa",vals}}),{},options());dense.decode(u"aaaaaa");auto mem=dense.memoryStatus();
    check(mem.stateCapacity<mem.states*4+4096,"P2 oversized frozen capacity released");
    std::cout<<"{\"test\":\"frozen_capacity\",\"live_states\":"<<mem.states<<",\"capacity_states\":"<<mem.stateCapacity<<",\"state_buffer_bytes\":"<<mem.stateBytes<<",\"boundary_nodes\":"<<mem.boundaryNodes<<",\"boundary_bytes\":"<<mem.boundaryBytes<<",\"published_boundaries\":"<<mem.publishedBoundaries<<"}\n";
}
static void fuzz() {
    std::vector<ImportedLexiconEntry> entries={{u"aa",{u"甲",u"乙",u"e\u0301"}},{u"ab",{u"丙",u"丁戊"}},{u"bb",{u"国",u"\U00020000"}},
        {u"aab",{u"人",u"\U0001f469\u200d\U0001f4bb"}},{u"abcde",{u"天",u"天地"}},{u"cc",{u"中",u"e",u"\u0301"}},{u"dd",{u"文"}}};
    auto l=lex(entries);auto m=std::make_shared<Model>();std::mt19937 rng(7152026);std::size_t snapshots=0;
    for(int beam:{1,3,16,64})for(bool duplicates:{false,true})for(bool learned:{false,true}) {
        auto o=options(beam);o.allowDuplicateSingleCharacters=duplicates;o.emittedCharacterReward=2;o.wholeInputSingleCharacterReward=5;o.isolationLambda=2;
        SentenceDecoder d(l,m,o),oracle(l,m,o);
        if(learned){SentenceLearningEvent e;e.id="fuzz";e.time=1700000000;e.mode=u"m";e.code=u"aabb";e.text=u"乙国";
            auto learning=SentenceLearningSnapshot::build({e},e.time);d.setLearning(learning,u"m");oracle.setLearning(learning,u"m");}
        std::u16string raw;std::shared_ptr<SentenceLockedPrefix> lock;
        for(int step=0;step<320;++step) {
            const auto operation=rng()%10;
            if(operation<5 && raw.size()<38)raw+=std::u16string_view(u"abcd")[rng()%4];
            else if(operation==5 && !raw.empty())raw.resize(raw.size()-1);
            else if(operation==6)raw+=std::u16string_view(u";0123'")[rng()%6];
            else if(operation==7 && raw.size()>4)raw.erase(2,1);
            else if(operation==8){raw=u"aa";lock=locked();}
            else if(operation==9){raw=u"aabbccaabb";lock.reset();}
            int limit=1+rng()%20;bool evidence=rng()%2;std::u16string required=(rng()%4==0)?u"甲":u"";
            auto a=d.decode(raw,limit,evidence,required,lock);oracle.resetDecodeCache();auto b=oracle.decode(raw,limit,evidence,required,lock);
            if(!equal(a,b)) {
                std::cerr<<"fuzz mismatch beam="<<beam<<" dup="<<duplicates<<" learn="<<learned<<" step="<<step<<" raw=";
                for(auto c:raw)std::cerr<<static_cast<char>(c);std::cerr<<" candidates="<<a.candidates.size()<<"/"<<b.candidates.size()<<" flags="<<a.learningAffected<<"/"<<b.learningAffected<<"\n";
            }
            check(equal(a,b),"full vs incremental/locked exact differential");++snapshots;
        }
    }
    std::cout<<"{\"test\":\"decoder_differential\",\"snapshots\":"<<snapshots<<",\"precision\":\"exact doubles and all exposed fields\"}\n";
}
static void learning() {
    SentenceLearningAccumulator accumulator;std::vector<SentenceLearningEvent> events;std::mt19937 rng(90210);std::int64_t now=1700000000;std::size_t comparisons=0;
    for(int step=0;step<500;++step) {
        if(step%23==0 && events.size()>4)events.erase(events.begin(),events.begin()+3); // oldest-window eviction
        if(step%41==0 && !events.empty())events.pop_back();
        if(step%131==0)events.clear();
        if(step%17==0)now+=60;if(step%53==0)now-=120;
        SentenceLearningEvent e;e.id=std::to_string(step);e.time=now+static_cast<int>(rng()%121)-30;e.code=u"aa"+std::u16string(1,u'a'+rng()%9);
        e.mode=(rng()%2)?u"m":u"n";e.text=(rng()%2)?u"虎娘":u"虎爪";e.context=(rng()%3)?u"上下":u"左右";events.push_back(e);
        auto actual=accumulator.update(events,now);auto reference=SentenceLearningSnapshot::build(events,now);
        for(const auto& row:events)for(const auto* context:{u"上下",u"左右",u"其他"}) {
            check(actual->score(row.mode,row.code,row.text,context)==reference->score(row.mode,row.code,row.text,context),"P5 exact incremental replay score");
            check(actual->prefixScore(row.mode,u"aa",u"虎",context)==reference->prefixScore(row.mode,u"aa",u"虎",context),"P5 exact incremental prefix hint");comparisons+=2;
        }
        check(accumulator.update(events,now)==actual,"P5 unchanged snapshot identity");
    }
    auto before=accumulator.update(events,now);events.clear();check(accumulator.update(events,now)->empty(),"P5 clear replay state");check(!before->empty(),"P5 published snapshot immutable");
    std::cout<<"{\"test\":\"replay_differential\",\"comparisons\":"<<comparisons<<",\"future_clock_undo_clear\":true}\n";
}
static std::shared_ptr<const SentenceNgram> mappedModel() {
    auto path=root/"synthetic-ngram.bin";std::ofstream out(path,std::ios::binary);out.write("TCSKNM01",8);
    auto integer=[&](std::uint64_t value,int bytes){for(int i=0;i<bytes;++i)out.put(static_cast<char>((value>>(8*i))&255));};integer(1,4);
    auto probability=[&](float value){std::uint32_t bits;std::memcpy(&bits,&value,4);integer(bits,4);};
    std::vector<std::uint32_t> tokens={0,2,3,0x7532,0x4e59,0x56fd,0x4e19,0x4e01,0x4e2d,0x6587,0x20000};
    std::sort(tokens.begin(),tokens.end());
    integer(tokens.size(),4);for(auto c:tokens){integer(c,4);probability(c?.2f:.1f);}
    auto pair=[](std::uint64_t a,std::uint64_t b){return (a<<21)|b;};
    integer(tokens.size()*tokens.size(),8);for(auto a:tokens)for(auto b:tokens){integer(pair(a,b),8);probability((a+b)%5==0?0:.03f);}
    integer(tokens.size(),4);for(auto a:tokens){integer(a,4);probability(.7f);}
    integer(tokens.size()*tokens.size()*tokens.size(),8);for(auto a:tokens)for(auto b:tokens)for(auto c:tokens){integer((static_cast<std::uint64_t>(a)<<42)|pair(b,c),8);probability((a+b+c)%3==0?0:.013f);}
    integer(tokens.size()*tokens.size(),8);for(auto a:tokens)for(auto b:tokens){integer(pair(a,b),8);probability(.8f);}
    out.close();return SentenceNgram::Open(path);
}
static void mapped() {
    auto model=mappedModel();check(model->hasObservedBigram(u"甲",u"乙"),"mapped observed records");
    SentenceSupplementMatcher matcher({SentenceSupplementEntry::create(u"甲乙国",1000),SentenceSupplementEntry::create(u"乙国",100),
        SentenceSupplementEntry::create(u"e\u0301国",1500)});
    auto graph=serializeImportedLexicon(matcher.serializeGraph());auto path=root/"supplement.tcd";
    {std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(graph.data()),graph.size());}
    auto supplement=std::make_shared<MappedSentenceSupplement>(Dictionary::Open(path));
    auto l=lex({{u"aa",{u"甲",u"乙"}},{u"bb",{u"国",u"\U00020000",u"e\u0301"}},{u"cc",{u"丙"}},{u"dd",{u"中",u"丁"}}});
    int snapshots=0;
    for(bool boundary:{false,true})for(bool duplicate:{false,true}) {
        auto o=options(32);o.scoreSentenceBoundaries=boundary;o.allowDuplicateSingleCharacters=duplicate;o.isolationLambda=2;o.isolationRankThreshold=1;o.isolationUseLogRank=true;
        SentenceDecoder decoder(l,model,o,supplement),reference(l,model,o,supplement);
        std::shared_ptr<SentenceLockedPrefix> lock;
        for(int round=0;round<4;++round) {
            if(round%2)lock=locked();else lock.reset();std::u16string raw=u"aa";
            for(int i=0;i<32;++i){raw+=std::u16string_view(u"aabbccdd")[i%8];
                auto a=decoder.decode(raw,20,i%2,u"",lock);reference.resetDecodeCache();auto b=reference.decode(raw,20,i%2,u"",lock);
                check(equal(a,b),"mapped model / supplement append parity");++snapshots;}
            for(int i=0;i<20;++i){raw.pop_back();auto a=decoder.decode(raw,20,true,u"",lock);reference.resetDecodeCache();
                check(equal(a,reference.decode(raw,20,true,u"",lock)),"mapped model / supplement backspace parity");++snapshots;}
        }
    }
    auto meta=l->candidateView(u"bb");l.reset();check(meta->at(0).text==u"国","metadata holds mapping after lexicon destruction");
    std::cout<<"{\"test\":\"mapped_ngram_supplement\",\"snapshots\":"<<snapshots<<",\"synthetic_model\":true}\n";
}
static void journalTests() {
    auto path=root/"journal"/".tigirl-learning.tsv";SentenceLearningStore store(path);
    SentenceLearningEvent good;good.id="same-id";good.time=learningNow();good.mode=u"m";good.code=u"aa";good.text=u"虎娘";
    auto invalid=good;invalid.mode=std::u16string(1,0xd800);
    store.confirm({invalid});check(store.snapshot()->empty(),"P5 malformed persisted mode is not learnt");
    store.confirm({good});check(store.entries().size()==1 && !store.snapshot()->empty(),"P5 invalid record must not reserve an id in the parse cache");
    auto retained=store.snapshot();const double retainedScore=retained->score(u"m",u"aa",u"虎娘",u"");
    auto validSize=std::filesystem::file_size(path);
    {std::ofstream out(path,std::ios::app|std::ios::binary);out<<"torn-record";}
    good.id="after-torn";bool rejected=false;try{store.confirm({good});}catch(...){rejected=true;}
    check(rejected && std::filesystem::file_size(path)==validSize+11,"P5 invalid text preserved for user repair");
    std::filesystem::resize_file(path,validSize);store.confirm({good});check(store.entries().size()==2,"P5 user-repaired tail reloads");
    check(retained->score(u"m",u"aa",u"虎娘",u"")==retainedScore,"P5 old published scores stay immutable");
    SentenceLearningStore external(path);auto other=good;other.id="external";other.text=u"虎爪";external.confirm({other});store.refresh();
    check(store.entries().size()==3 && store.snapshot()->score(u"m",u"aa",u"虎爪",u"")>store.snapshot()->score(u"m",u"aa",u"虎娘",u""),"P5 external writer replay");
    store.undoLast();check(store.entries().size()==2,"P5 undo external event");store.clear();check(store.snapshot()->empty(),"P5 clear published snapshot");
    std::vector<SentenceLearningEvent> large;large.reserve(10001);
    for(int i=0;i<10001;++i){auto e=good;e.id="window-"+std::to_string(i);large.push_back(e);}
    store.confirm(large);auto entries=store.entries();check(entries.size()==10000 && entries.front().id=="window-1","P5 bounded active window");
    store.undoLast();entries=store.entries();check(entries.size()==10000 && entries.front().id=="window-0","P5 undo restores history outside prior active window");
    store.clear();
    // Existing tombstones apply before the combined active-window limit. A
    // large append cannot discard an older surviving event prematurely.
    std::string undo="TCL2\tU\tremove-future\t"+std::to_string(learningNow())+"\tfuture-10000";
    {std::ofstream out(path,std::ios::app|std::ios::binary);out<<undo<<'\n';}
    for(int i=0;i<10001;++i){large[i].id="future-"+std::to_string(i);large[i].code=i?u"aa":u"zz";}
    store.confirm(large);check(store.snapshot()->score(u"m",u"zz",u"虎娘",u"")>0,"P5 tombstones precede appended active-window trimming");
    store.clear();
}
static void history() {
    auto l=lex({{u"aa",{u"甲"}}});SentenceDecoder d(l,{},options()),reference(l,{},options());SentenceSession session,refSession;session.start(u"a",1);refSession.start(u"a",1);int commits=0;
    for(int step=1;step<=768;++step) {
        if(step>1){check(session.append(u'a'),"history append");check(refSession.append(u'a'),"reference history append");}
        auto ticket=*session.request(),refTicket=*refSession.request();auto a=d.decode(ticket.raw,20,true,ticket.requiredPrefix,ticket.lockedPrefix);
        auto b=reference.decode(refTicket.raw,20,true,refTicket.requiredPrefix,refTicket.lockedPrefix);
        check(equal(a,b),"P6 frontier pruning preserves exact result");session.apply(ticket,std::move(a));refSession.apply(refTicket,std::move(b));
        auto committed=session.tryAutoCommit(true);check(committed==refSession.tryAutoCommit(true),"P6 same real automatic-commit trajectory");if(committed)++commits;
        d.retainCommittedHistory(session.raw(),session.request()->committedRaw);
    }
    const auto compact=d.memoryStatus(),old=reference.memoryStatus();check(compact.positions<80 && old.positions==769,"P6 old buckets released, coordinates retained");
    check(equal(d.decode(u"aaaaaa",20,true),d.decodeFull(u"aaaaaa",20,true)),"P6 rollback before retained window reconstructs");
    std::cout<<"{\"test\":\"committed_history\",\"raw\":768,\"retained_positions\":"<<compact.positions<<",\"unpruned_positions\":"<<old.positions<<",\"automatic_commits\":"<<commits<<"}\n";
}
static void cancellation() {
    auto l=lex({{u"aa",{u"甲",u"乙"}}});auto m=std::make_shared<Model>();SentenceDecoder d(l,m,options(128));auto flag=std::make_shared<std::atomic<bool>>(true);
    bool cancelled=false;try{d.decode(u"aaaaaa",20,true,u"",{},flag);}catch(const SentenceDecodeCancelled&){cancelled=true;}check(cancelled,"P7 pre-cancelled decode");
    flag->store(false);m->cancel=flag;m->cancelAfter=50;m->calls=0;cancelled=false;
    try{d.decode(std::u16string(80,u'a'),20,true,u"",{},flag);}catch(const SentenceDecodeCancelled&){cancelled=true;}
    check(cancelled && m->calls<2000,"P7 in-flight cancellation at bounded expansion checkpoints");check(!d.memoryStatus().positions,"P7 cancelled partial lattice not cached");
    m->cancel.reset();flag->store(false);check(equal(d.decode(u"aaaaaa",20,true),d.decodeFull(u"aaaaaa",20,true)),"P7 next generation after cancellation");
}
int main(int argc,char** argv) {
    try{if(argc<2)return 2;root=argv[1];lexicalPath=argc>3?fs::path(argv[3]):fs::path{};fs::create_directories(root);std::cout<<std::setprecision(17);
        const std::string selected=argc>2?argv[2]:"all";
        for(const auto& test:std::vector<std::pair<std::string,void(*)()>>{{"correctness",correctness},{"ranking",rankingPriors},{"caching",caching},{"fuzz",fuzz},{"learning",learning},{"journal",journalTests},{"mapped",mapped},{"history",history},{"cancellation",cancellation}})
            if(selected=="all" || selected==test.first){test.second();std::cout<<"{\"group\":\""<<test.first<<"\",\"status\":\"passed\"}\n"<<std::flush;}
        std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"production_model\":false,\"physical_input\":false}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
