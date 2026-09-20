#include "../native/SentenceSession.h"
#include <iostream>
#include <stdexcept>
using namespace tiger;
int checks=0;
void check(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
SentenceDecodeResult fixture() {
    SentenceDecodeResult r;r.rawCode=u"aabb";
    for(auto text:{u"中国",u"中华",u"人民"}) {
        SentenceCandidate c;c.text=text;c.segmentedCode=u"aa bb";c.maxLexiconRank=r.candidates.empty()?1:2;
        c.boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{
            std::make_shared<SentencePathBoundary>(SentencePathBoundary{{},1,2}),2,4});
        r.candidates.push_back(std::move(c));
    }
    return r;
}
SentenceDecodeResult autoFixture(std::u16string raw,bool merged=false) {
    auto r=fixture();r.rawCode=raw;
    for(auto& c:r.candidates)c.segmentedCode=raw;
    r.earlyCommitEvidence.prefixes.push_back({u"中",2,1,1,true});
    r.earlyCommitEvidence.mergedIncompleteTail=merged;
    if(merged)r.candidates.clear();
    return r;
}
int wmain() {
 try {
    SentenceSession a;a.start(u"AAbb",7);auto old=*a.request();
    check(a.apply(old,fixture()),"initial result");check(a.displayCode()==u"AA bb","literal casing");
    a.moveSelection(1,5);check(a.selectedIndex()==1 && a.autoCommitSuspended(),"manual selection");
    check(!a.apply(old,fixture()) && a.selectedIndex()==1,"duplicate result must preserve selection");
    auto preview=a;preview.append(u'C');check(a.raw()==u"AAbb" && a.current(),"preview isolation");
    check(!a.apply(*preview.request(),fixture()),"preview result cannot mutate real generation");
    a.append(u'C');check(!a.current() && a.displayCode()==u"AA bbc","pending append display");
    check(!a.apply(old,fixture()),"reject old append result");check(!a.commitCandidate(0),"pending candidate must not commit stale text");
    a.backspace();check(a.displayCode()==u"AA bb","pending delete display");
    auto beforeReload=*a.request();a.changeResources(8);check(a.displayCode()==u"AAbb","hide previous resources");
    check(!a.apply(beforeReload,fixture()),"reject old resources");check(a.apply(*a.request(),fixture()),"new resources");
    SentenceSession other;other.start(u"AAbb",8);check(!other.apply(*a.request(),fixture()),"context isolation");
    auto beforeClear=*a.request();a.clear();a.start(u"AAbb",8);check(!a.apply(beforeClear,fixture()),"same raw new composition");
    a.apply(*a.request(),fixture());check(a.commitCandidate(2)==std::optional<std::u16string>(u"人民") && !a.active(),"chosen commit");
    a.start(u"aabb",8);a.apply(*a.request(),fixture());
    check(!a.commitPrefix(u"中",3),"prefix must end at actual boundary");
    check(a.commitPrefix(u"中",2)==std::optional<std::u16string>(u"中"),"prefix commit");
    check(a.liveRaw()==u"bb" && a.displayCode()==u"bb" && a.result().candidates.size()==2,"prefix projection");
    check(a.candidateText(0)==u"国" && a.request()->requiredPrefix==u"中","retained decoder context");
    check(!a.commitPrefix(u"中",2),"prefix cannot commit twice");
    check(a.commitCandidate(1)==std::optional<std::u16string>(u"华"),"final commit excludes prefix");
    a.start(u"aabb",8);a.apply(*a.request(),fixture());a.commitPrefix(u"中",2);
    a.backspace();check(a.liveRaw()==u"b","delete live tail");a.backspace();check(!a.active(),"do not backspace committed text");
    auto ranked=fixture();ranked.candidates.back().boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{{},2,4});
    a.start(u"aabb",8,true);a.apply(*a.request(),ranked);check(a.result().candidates.size()==2,"continuation keeps segmented ranks and hides whole-input non-first edge");
    a.start(u"aabb2",8,true);a.apply(*a.request(),ranked);check(a.result().candidates.size()==3,"explicit selector bypasses rank filter");
    a.moveSelection(-1,2);check(a.selectedIndex()==1,"reverse selection wraps visible list");
    check(a.commitWithSuffix(u"。")==u"中华。","punctuation commits selection");
    a.start(u"Ab",8);check(a.commitWithSuffix(u"！")==u"Ab！","no-candidate punctuation literal");
    a.start(u"Ab",8);check(a.commitRaw(false)==u"Ab" && !a.active(),"enter literal");
    a.start(u"Ab",8);check(a.commitRaw(true).empty() && !a.active(),"enter clear");
    a.start(u"a",8);for(int i=1;i<128;++i)check(a.append(u'a'),"append within limit");
    check(!a.append(u'a') && a.raw().size()==128,"live input limit");
    a.clear();check(!a.request(),"inactive no work");
    a.start(u"aaaaa",9);a.apply(*a.request(),autoFixture(a.raw()));
    check(!a.tryAutoCommit(true),"One strong observation cannot auto commit");
    check(!a.tryAutoCommit(true),"Duplicate observation counted twice");
    a.append(u'a');a.apply(*a.request(),autoFixture(a.raw()));auto beforeAuto=*a.request();
    auto autoPreview=a;
    check(autoPreview.tryAutoCommit(true)==std::optional<std::u16string>(u"中") && a.liveRaw()==u"aaaaaa","Auto preview mutated source");
    check(a.tryAutoCommit(true)==std::optional<std::u16string>(u"中"),"Current evidence prefix commit");
    check(a.liveRaw()==u"aaaa" && a.current() && a.request()->requiredPrefix==u"中","Auto prefix lost raw/current context");
    check(!a.apply(beforeAuto,autoFixture(u"aaaaaa")),"Pre-commit ticket accepted after prefix");
    check(a.candidateText(1)==u"华" && a.result().candidates[1].maxLexiconRank==2,"Auto prefix incorrectly enforced first-rank continuation");
    check(!a.tryAutoCommit(true) && a.commitCandidate(1)==std::optional<std::u16string>(u"华"),"Auto prefix was committed twice");
    a.start(u"aaaaa",9);a.apply(*a.request(),autoFixture(a.raw()));a.tryAutoCommit(true);
    a.append(u'a');a.apply(*a.request(),autoFixture(a.raw()));a.append(u'a');auto pending=*a.request();
    check(a.tryAutoCommit(true)==std::optional<std::u16string>(u"中") && !a.current() && a.liveRaw()==u"aaaaa","Previous-generation evidence cannot commit");
    check(!a.apply(pending,autoFixture(a.raw())),"Pending pre-prefix ticket accepted");
    check(a.apply(*a.request(),autoFixture(a.raw())) && a.commitCandidate(0)==std::optional<std::u16string>(u"国"),"Previous-generation suffix commit duplicated prefix");
    a.start(u"aaaaa",9);a.apply(*a.request(),autoFixture(a.raw(),true));a.tryAutoCommit(true);
    a.append(u'a');a.apply(*a.request(),autoFixture(a.raw(),true));
    check(a.tryAutoCommit(true)==std::optional<std::u16string>(u"中") && a.result().candidates.empty(),"Merged-tail evidence incorrectly requires a visible candidate");
    a.backspace();check(a.liveRaw()==u"aaa","Auto tail backspace touched committed raw");
    a.start(u"aaaaa",9);a.apply(*a.request(),autoFixture(a.raw()));a.tryAutoCommit(true);
    a.changeResources(10);a.append(u'a');a.apply(*a.request(),autoFixture(a.raw()));
    check(!a.tryAutoCommit(true),"Evidence survived resource invalidation");
    a.append(u'a');a.apply(*a.request(),autoFixture(a.raw()));a.moveSelection(1,5);
    check(!a.tryAutoCommit(true),"Manual selection did not suspend auto commit");
    a.start(u"aaaaa",9);a.apply(*a.request(),autoFixture(a.raw()));a.tryAutoCommit(true,8);
    a.append(u'a');a.apply(*a.request(),autoFixture(a.raw()));check(!a.tryAutoCommit(true,8),"Configured retained raw ignored");
    check(!a.tryAutoCommit(false),"Disabled policy committed a prefix");

    a.start(u"aaaaa",9);a.apply(*a.request(),autoFixture(a.raw()));a.tryAutoCommit(true);
    a.append(u'a');a.apply(*a.request(),autoFixture(a.raw()));
    auto unprotected=a;
    check(unprotected.tryAutoCommit(true)==std::optional<std::u16string>(u"中"),"Baseline retained raw no longer matures");
    SentencePathQueries crossing;
    crossing.competingBoundaryEnd=[](std::u16string_view raw,int committed,int proposed,int target) {
        check(raw==u"aaaaaa" && committed==0 && proposed==2 && target==1,
            "Cross-boundary query lost aligned coordinates");
        return proposed+2;
    };
    check(!a.tryAutoCommit(true,0,crossing),"Cross-boundary split did not extend retained lookahead");
    a.append(u'a');a.apply(*a.request(),autoFixture(a.raw()));
    crossing.competingBoundaryEnd=[](std::u16string_view,int,int proposed,int target){
        check(target==1,"Cross-boundary release lost target element count");return proposed+2;};
    check(a.tryAutoCommit(true,0,crossing)==std::optional<std::u16string>(u"中"),
        "Cross-boundary split did not release after three keys beyond competing boundary");

    a.start(u"aaaaa",12);a.apply(*a.request(),autoFixture(a.raw()));
    check(!a.tryAutoCommit(true),"Low-confidence reset fixture first strong committed");
    a.append(u'a');auto low=autoFixture(a.raw());
    low.earlyCommitEvidence.neutralLowConfidence=true;
    low.earlyCommitEvidence.prefixes.front().share=.5;
    low.earlyCommitEvidence.prefixes.front().baseShare=.5;
    a.apply(*a.request(),std::move(low));
    check(!a.tryAutoCommit(true),"Low-confidence gap committed stale tracker");
    a.append(u'a');a.apply(*a.request(),autoFixture(a.raw()));
    check(!a.tryAutoCommit(true),"First fresh strong after low-confidence gap reused stale maturity");
    a.append(u'a');a.apply(*a.request(),autoFixture(a.raw()));
    check(a.tryAutoCommit(true)==std::optional<std::u16string>(u"中"),
        "Second fresh strong after low-confidence gap did not mature");

    bool full=false,proper=false,alternative=false;int fullCalls=0,uniqueCalls=0;
    SentencePathQueries queries;
    queries.complete=[&](std::u16string_view raw,std::u16string_view prefix,std::optional<std::u16string_view> excluded,bool grouped,const SentenceLockedPrefix* locked){
        if(excluded){++uniqueCalls;check(raw==u"aabb" && prefix.empty() && *excluded==u"中国" && grouped,"uniqueness query lost captured context");return alternative;}
        ++fullCalls;check(!grouped,"full path query must include all ranks");return full;
    };
    queries.properPrefix=[&](std::u16string_view raw){check(raw.substr(0,2)==u"bb","proper prefix must include last captured segment");return proper;};
    auto prepare=[&]{a.start(u"aabb",11);a.apply(*a.request(),fixture());full=proper=alternative=false;fullCalls=uniqueCalls=0;};
    prepare();auto emptyPreview=a;
    check(emptyPreview.appendAutomatic(u'c',true,0,queries)==std::optional<std::u16string>(u"中国"),"empty-code commit missing");
    check(a.raw()==u"aabb" && a.current(),"empty-code preview changed original");
    auto oldEmpty=*a.request();
    check(a.appendAutomatic(u'c',true,0,queries)==std::optional<std::u16string>(u"中国") && a.liveRaw()==u"c","empty-code live suffix");
    check(a.raw()==u"aabbc" && a.request()->requiredPrefix==u"中国" && !a.current(),"empty-code discarded decoder context");
    check(!a.apply(oldEmpty,fixture()),"empty-code accepted old ticket");
    auto continuation=fixture();continuation.candidates[0].text=u"中国人";continuation.candidates[1].text=u"中国民";
    auto duplicate=continuation.candidates[1];duplicate.text=u"中国刍";
    continuation.candidates[1].boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{{},3,5});
    continuation.candidates.insert(continuation.candidates.begin(),duplicate);
    a.apply(*a.request(),continuation);check(a.result().candidates.size()==2 && a.commitCandidate(0)==std::optional<std::u16string>(u"刍"),"empty-code continuation retains segmented duplicate but hides whole-input non-first edge");
    prepare();proper=true;check(!a.appendAutomatic(u'c',true,0,queries) && uniqueCalls==0,"proper prefix committed too early");
    proper=false;check(a.appendAutomatic(u'd',true,0,queries)==std::optional<std::u16string>(u"中国") && a.liveRaw()==u"cd","pending prefix not retained across keys");
    prepare();check(!a.appendAutomatic(u'c',true,2,queries),"minimum retained ignored");
    check(a.appendAutomatic(u'd',true,2,queries)==std::optional<std::u16string>(u"中国"),"retained threshold not released");
    prepare();alternative=true;check(!a.appendAutomatic(u'c',true,0,queries) && uniqueCalls==1,"hidden alternative ignored");
    alternative=false;check(!a.appendAutomatic(u'd',true,0,queries) && uniqueCalls==1,"rejected pending survived");
    prepare();full=true;check(!a.appendAutomatic(u'c',true,0,queries) && uniqueCalls==0,"complete continuation committed");
    full=false;check(!a.appendAutomatic(u'd',true,0,queries),"complete continuation retained pending");
    prepare();proper=true;a.appendAutomatic(u'c',true,0,queries);a.backspace();a.append(u'c');proper=false;
    check(!a.appendAutomatic(u'd',true,0,queries),"backspace retained pending");
    prepare();proper=true;a.appendAutomatic(u'c',true,0,queries);a.appendAutomatic(u'2',true,0,queries);proper=false;
    check(!a.appendAutomatic(u'd',true,0,queries),"selector retained pending");
    prepare();a.moveSelection(1,5);check(!a.appendAutomatic(u'c',true,0,queries) && fullCalls==0,"manual selection did not suspend empty code");
    prepare();check(!a.appendAutomatic(u'c',false,0,queries) && fullCalls==0,"disabled empty code queried decoder");
    prepare();auto ambiguous=fixture();ambiguous.candidates[1].maxLexiconRank=1;a.apply(*a.request(),ambiguous);
    // Fresh session: duplicate publication is intentionally rejected.
    a.start(u"aabb",11);a.apply(*a.request(),ambiguous);
    check(!a.appendAutomatic(u'c',true,0,queries) && fullCalls==0,"ambiguous visible paths committed");
    ambiguous.earlyCommitEvidence.prefixes.push_back({u"中",2,1,1,true});
    ambiguous.candidates[1].confidenceScore=-20;a.start(u"aabb",11);a.apply(*a.request(),ambiguous);
    check(a.appendAutomatic(u'c',true,0,queries)==std::optional<std::u16string>(u"中国") && uniqueCalls==0,"strong visible top requires uniqueness unnecessarily");
    ambiguous.earlyCommitEvidence.confidenceTruncated=true;a.start(u"aabb",11);a.apply(*a.request(),ambiguous);
    check(!a.appendAutomatic(u'c',true,0,queries),"truncated confidence committed ambiguous path");
    std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"append_limit\":128}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
