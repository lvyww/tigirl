#pragma once
// PR #12 performance regressions. Synthetic fixtures; not a live TSF benchmark.
#include "sentence_learning_reference.h"
#include "SentenceLearningStore.h"
#include "SentenceDecoder.h"
#include "LexiconSerialize.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
namespace tiger::learning_test {
struct PerformanceChecks {
    int count=0;
    void require(bool ok,const char* message){++count;if(!ok)throw std::runtime_error(message);}
    void equal(double a,double b){require(std::abs(a-b)<1e-12,"indexed score differs from pre-index reference");}
};
inline SentenceLearningEvent sample(std::u16string code,std::u16string text,std::u16string context={},std::int64_t time=1700000000) {
    SentenceLearningEvent e;e.id=learningId();e.mode=u"perf";e.code=std::move(code);e.text=std::move(text);e.context=std::move(context);e.time=time;return e;
}
inline void snapshotCacheTests(PerformanceChecks& test,const std::filesystem::path& folder) {
    const auto path=folder/"initially-absent"/".tigirl-learning-v1.log";
    SentenceLearningStore store(path);const auto initial=store.snapshot();
    for(int i=0;i<5;++i){store.refresh();test.require(store.snapshot()==initial,"missing file changed empty snapshot identity");}
    test.require(!std::filesystem::exists(path.parent_path()),"missing-file reads created files or locks");

    const auto prepared=prepareSentenceLexicon({{u"a",{u"甲",u"乙"}},{u"aa",{u"中",u"国"}},{u"aaa",{u"人"}},{u"aaaa",{u"民"}}});
    auto data=serializeImportedLexicon(prepared);const auto dictionary=folder/"perf.tcd";
    {std::ofstream out(dictionary,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));if(!out)throw std::runtime_error("write perf dictionary");}
    auto lex=std::make_shared<SentenceLexicon>(Dictionary::Open(dictionary));SentenceDecoderOptions options;
    options.allowDuplicateSingleCharacters=true;options.isolationRankThreshold=0;
    SentenceDecoder stable(lex,{},options),refreshed(lex,{},options);int baseline=0,actual=0;
    for(int n=1;n<=8;++n) {
        const auto raw=std::u16string(n,u'a');stable.setLearning(initial,u"perf");const auto a=stable.decode(raw,20,true);
        store.refresh();refreshed.setLearning(store.snapshot(),u"perf");const auto b=refreshed.decode(raw,20,true);
        test.require(a.expandedStates==b.expandedStates,"empty learning refresh discarded incremental lattice");
        test.require(a.candidates.size()==b.candidates.size() && a.learningAffected==b.learningAffected,"empty learning changed candidate set");
        for(std::size_t i=0;i<a.candidates.size();++i) {
            test.require(a.candidates[i].text==b.candidates[i].text,"empty learning changed candidate order");
            test.equal(a.candidates[i].finalScore,b.candidates[i].finalScore);
        }
        baseline=a.expandedStates;actual=b.expandedStates;
    }
    std::cout<<"{\"test\":\"empty_learning_cache\",\"baseline_expanded\":"<<baseline<<",\"refresh_expanded\":"<<actual<<"}\n";
    std::filesystem::create_directories(path.parent_path());
    {std::ofstream out(path);}
    store.refresh();test.require(store.snapshot()==initial,"new empty file changed empty snapshot identity");
    std::filesystem::last_write_time(path,std::filesystem::last_write_time(path)+std::chrono::seconds(1));
    store.refresh();test.require(store.snapshot()==initial,"modified empty file changed empty snapshot identity");
    {std::ofstream out(path);out<<"invalid journal\n";}
    store.refresh();test.require(store.snapshot()==initial,"invalid-only file changed empty snapshot identity");

    auto e=sample(u"aabb",u"虎娘",u"设置",learningNow());store.confirm({e});const auto learned=store.snapshot();
    test.require(learned!=initial && !learned->empty(),"first record did not publish a new snapshot");
    store.refresh();test.require(store.snapshot()==learned,"unchanged existing file discarded snapshot");
    std::ifstream input(path,std::ios::binary);const std::string content((std::istreambuf_iterator<char>(input)),{});input.close();
    const auto stamp=std::filesystem::last_write_time(path);
    std::filesystem::remove(path);store.refresh();const auto removed=store.snapshot();
    test.require(removed!=learned && removed->empty(),"deleted journal retained learned preference");
    for(int i=0;i<5;++i){store.refresh();test.require(store.snapshot()==removed,"repeated missing refresh invalidated cache");}
    {std::ofstream out(path,std::ios::binary);out<<content;}
    std::filesystem::last_write_time(path,stamp);store.refresh();
    test.require(!store.snapshot()->empty() && store.snapshot()!=removed,"recreated same-signature file was not reloaded");
    test.require(store.snapshot()->score(e.mode,e.code,e.text,e.context)>0,"recreated file lost preference");
    test.require(learned->score(e.mode,e.code,e.text,e.context)>0,"publishing mutated a retained snapshot");
    store.clear();const auto cleared=store.snapshot();test.require(cleared->empty(),"clear retained learning");
    std::filesystem::last_write_time(path,std::filesystem::last_write_time(path)+std::chrono::seconds(1));
    store.refresh();test.require(store.snapshot()==cleared,"cleared journal refresh invalidated empty cache");
    {std::ofstream out(path,std::ios::trunc);}
    store.refresh();test.require(store.snapshot()==cleared,"truncated journal refresh invalidated empty cache");
}
inline void indexedScoreTests(PerformanceChecks& test) {
    const std::vector<std::u16string> modes={u"perf",u"smart",u"other"};
    const std::vector<std::u16string> codes={u"a",u"aa",u"aab",u"aabb",u"ab",u"abc",u"b",u"zz"};
    const std::vector<std::u16string> texts={u"甲",u"虎",u"虎娘",u"虎爪",u"虎娘们",u"𰻞娘",u"𰻞",u"甲乙"};
    const std::vector<std::u16string> contexts={u"",u"甲",u"乙",u"设置",u"前文",u"𰻞",u"𰻞娘"};
    std::mt19937 random(120640);std::vector<SentenceLearningEvent> events;
    for(int i=0;i<600;++i) {
        auto e=sample(codes[random()%codes.size()],texts[random()%texts.size()],contexts[random()%contexts.size()]);
        e.mode=modes[random()%modes.size()];e.time+=static_cast<std::int64_t>(random()%(180*86400))-150*86400;
        events.push_back(std::move(e));
    }
    // Include caps, future/unsorted timestamps, unknown/supplementary contexts,
    // duplicate input events and invalid rows, independently of journal parsing.
    for(int i=0;i<8;++i)events.push_back(sample(u"aabb",u"虎娘",u"设置"));
    for(int kind=0;kind<9;++kind) {
        auto e=sample(u"aa",u"虎娘");
        switch(kind){case 0:e.mode.clear();break;case 1:e.mode.assign(513,u'x');break;case 2:e.code.clear();break;
            case 3:e.code.assign(129,u'a');break;case 4:e.text=u"{日期}";break;case 5:e.text.assign(17,u'字');break;
            case 6:e.context=u"三字符";break;case 7:e.context.assign(1,0xd800);break;case 8:e.text.clear();break;}
        events.push_back(std::move(e));
    }
    auto queryModes=modes;queryModes.push_back(u"missing");auto queryCodes=codes;queryCodes.push_back(u"");queryCodes.push_back(u"aac");
    auto queryTexts=texts;queryTexts.push_back(u"");queryTexts.push_back(u"不存在");auto queryContexts=contexts;queryContexts.push_back(u"未见");
    for(auto now:{1700000000LL,1700000001LL,1702592000LL,1710368000LL}) {
        const auto reference=ReferenceLearningSnapshot::build(events,now);const auto indexed=SentenceLearningSnapshot::build(events,now);
        test.require(reference->empty()==indexed->empty(),"index emptiness differs");
        for(const auto& mode:queryModes)for(const auto& code:queryCodes)for(const auto& text:queryTexts)for(const auto& context:queryContexts) {
            test.equal(indexed->score(mode,code,text,context),reference->score(mode,code,text,context));
            test.equal(indexed->prefixScore(mode,code,text,context),reference->prefixScore(mode,code,text,context));
        }
    }
    std::vector<SentenceLearningEvent> bounded={sample(u"a",u"虎娘")};
    for(int i=0;i<70;++i){auto code=u"a"+std::u16string{static_cast<char16_t>(u'a'+i/26),static_cast<char16_t>(u'a'+i%26)};auto e=sample(code,u"虎娘");e.mode=u"other";bounded.push_back(e);}
    auto inside=bounded[63];inside.mode=u"perf";bounded.push_back(inside);
    auto outside=bounded[64];outside.mode=u"only-outside";bounded.push_back(outside);
    auto reference=ReferenceLearningSnapshot::build(bounded,1700000000);auto indexed=SentenceLearningSnapshot::build(bounded,1700000000);
    test.require(indexed->prefixScore(u"perf",u"a",u"虎",u"")==6,"64-row prefix bound lost inside choice");
    test.require(indexed->prefixScore(u"only-outside",u"a",u"虎",u"")==0,"mode index bypassed global 64-code-row bound");
    for(const auto& e:bounded){test.equal(indexed->prefixScore(e.mode,u"a",u"虎",e.context),reference->prefixScore(e.mode,u"a",u"虎",e.context));}
    test.require(indexed->prefixScore(u"perf",inside.code,u"虎",u"")==0,"exact code used as unfinished prefix");
    test.require(indexed->prefixScore(u"perf",u"a",u"虎娘",u"")==0,"exact text used as unfinished prefix");
}
using PerfClock=std::chrono::steady_clock;
template<class Snapshot> double prefixMicros(const Snapshot& snapshot) {
    volatile double sink=0;
    const auto query=[&](int i){return snapshot.prefixScore(u"perf",u"aa",u"虎",i%2?u"未见":u"一");};
    const auto start=PerfClock::now();sink=query(0);
    const auto single=std::chrono::duration<double,std::micro>(PerfClock::now()-start).count();
    // Calibrate repetitions: even the intentionally slow negative control is
    // bounded. Medians and a relative, very loose gate avoid machine-specific ms.
    const int rounds=static_cast<int>(std::max(1.0,std::min(2048.0,2000/std::max(0.001,single))));
    std::vector<double> samples;
    for(int trial=0;trial<5;++trial){const auto begin=PerfClock::now();for(int i=0;i<rounds;++i)sink=query(i);
        samples.push_back(std::chrono::duration<double,std::micro>(PerfClock::now()-begin).count()/rounds);}
    (void)sink;std::sort(samples.begin(),samples.end());return samples[2];
}
inline void indexedPerformanceTests(PerformanceChecks& test) {
#if defined(__SANITIZE_ADDRESS__)
    constexpr bool sanitized=true;
#elif defined(__clang__)
    constexpr bool sanitized=__has_feature(address_sanitizer);
#else
    constexpr bool sanitized=false;
#endif
    // The frozen quadratic reference is deliberately NOT timed under ASan/O0.
    // Differential correctness still runs above; release/MSVC enforce speed.

    for(int n:{100,1000,10000}) {
        std::vector<SentenceLearningEvent> events;events.reserve(n);
        for(int i=0;i<n;++i)events.push_back(sample(u"aabb",u"虎娘",std::u16string(1,static_cast<char16_t>(0x4e00+i))));
        const auto start=PerfClock::now();auto indexed=SentenceLearningSnapshot::build(events,1700000000);
        const double buildUs=std::chrono::duration<double,std::micro>(PerfClock::now()-start).count();
        const double indexedUs=prefixMicros(*indexed);
        double referenceUs=0;
        if(!sanitized && n<=1000){auto reference=ReferenceLearningSnapshot::build(events,1700000000);referenceUs=prefixMicros(*reference);
            test.equal(indexed->score(u"perf",u"aabb",u"虎娘",u"未见"),reference->score(u"perf",u"aabb",u"虎娘",u"未见"));}
        std::cout<<"{\"test\":\"indexed_learning_prefix\",\"contexts\":"<<n<<",\"build_us\":"<<buildUs<<",\"indexed_us\":"<<indexedUs<<",\"reference_us\":";
        if(!sanitized && n<=1000)std::cout<<referenceUs;else std::cout<<"null";
        std::cout<<"}"<<std::endl;
        if(!sanitized && n==1000)test.require(indexedUs*8<referenceUs,"prefix query regressed to context-wide rescans");
    }
}
inline int runPerformanceTests(const std::filesystem::path& folder) {
    std::filesystem::create_directories(folder);PerformanceChecks test;
    snapshotCacheTests(test,folder);indexedScoreTests(test);indexedPerformanceTests(test);
    std::cout<<"{\"test\":\"learning_performance_regressions\",\"checks\":"<<test.count<<",\"status\":\"passed\"}"<<std::endl;return test.count;
}
}
