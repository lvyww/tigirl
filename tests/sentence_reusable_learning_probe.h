// Reusable score learning: real boundaries, explicit feedback and legacy isolation.
static void reusableLearningTests(const std::filesystem::path& folder) {
    const auto mode=std::u16string(u"test-v1");
    const std::vector<SentenceLearningBoundary> bare{{3,1},{5,2}},prefixed{{2,1},{5,2},{7,3}};
    auto chosen=sentenceLearningSelectionEvents(u"kzjuy",u"滤掉",u"淦掉",bare,bare,0,mode,{});
    check(chosen.size()==1 && chosen[0].code==u"kzjuy" && chosen[0].text==u"淦掉" && chosen[0].context.empty(),
        "standalone two-character confirmation learns the phrase, not every character");
    sentenceLearningPlanLevels(chosen,{},u"kzjuy",u"滤掉",u"淦掉",bare,bare,-17.98817910318355,-26.322402219101054,true);
    auto known=SentenceLearningSnapshot::build(chosen);
    check(chosen[0].levels==2 && known->score(mode,u"kzjuy",u"淦掉",u"")==11 &&
        known->score(mode,u"kzjuy",u"淦掉",u"没")==8 && known->score(mode,u"kzj",u"淦",u"")==0,
        "gap planner produces two levels and transferable phrase score without broad single reward");
    check(known->confidenceScore(mode,u"kzjuy",u"淦掉",u"")==9,
        "two ranking levels count as one real confirmation");
    auto next=sentenceLearningSelectionEvents(u"gkkzjuy",u"去滤掉",u"去淦掉",prefixed,prefixed,0,mode,known);
    check(next.size()==1 && next[0].code==u"kzjuy" && next[0].text==u"淦掉" &&
        next[0].context==u"去" && next[0].rawStart==2 && next[0].rawEnd==7,
        "existing phrase crosses the smaller changed-character interval");
    auto split=sentenceLearningSelectionEvents(u"jekzjuy",u"侮金掉",u"他淦掉",{{3,1},{5,2},{7,3}},prefixed,0,mode,known);
    check(split.size()==1 && split[0].text==u"淦掉" && split[0].code==u"kzjuy" && split[0].context==u"他",
        "existing phrase replaces a coarse prefix-containing diff");
    auto supplemental=sentenceLearningSelectionEvents(u"gkkzjuy",u"去滤掉",u"去淦掉",prefixed,prefixed,0,mode,{},
        [](std::u16string_view text){return text==u"淦掉";});
    check(supplemental.size()==1 && supplemental[0].text==u"淦掉","supplement word can provide reusable phrase identity");
    auto prefix=event(u"jekzj",u"他淦");prefix.mode=mode;
    auto ambiguous=SentenceLearningSnapshot::build({chosen[0],prefix});
    auto fallback=sentenceLearningSelectionEvents(u"jekzjuy",u"侮金掉",u"他淦掉",{{3,1},{5,2},{7,3}},prefixed,0,mode,ambiguous);
    check(fallback.size()==1 && fallback[0].text==u"他淦" && fallback[0].rawEnd==5,
        "overlapping unrelated equal-length known phrases fall back to the validated diff");
    check(sentenceLearningSelectionEvents(u"kzjuy",u"滤掉",u"淦掉",bare,bare,1,mode,{}).empty(),
        "independent phrase learning never crosses a locked floor");
    auto suffix=sentenceLearningSelectionEvents(u"gkkzjuy",u"去滤掉",u"去淦掉",prefixed,prefixed,2,mode,known);
    check(suffix.size()==1 && suffix[0].rawStart==2,"a learned phrase may start exactly at the locked floor");
    check(sentenceLearningSelectionEvents(u"kzjuy",u"滤掉",u"淦掉",{{3,1}},bare,0,mode,{}).empty(),
        "incomplete competing path does not authorize a fragment");
    check(sentenceLearningSelectionEvents(u"kzjuy",u"淦掉",u"淦掉",bare,bare,0,mode,known).empty(),
        "unchanged ordinary first choice does not reinforce");
    auto unicode=sentenceLearningSelectionEvents(u"aabb",u"甲掉",u"𠀀掉",{{2,1},{4,2}},{{2,2},{4,3}},0,mode,{});
    check(unicode.size()==1 && unicode[0].text==u"𠀀掉" && unicode[0].textEnd==3,
        "standalone phrase size counts Unicode scalars and retains UTF-16 offsets");

    // Corrected candidates exclude ordinary rewards even on their unchanged prefix.
    std::vector<SentenceLearningEvent> prefixHistory;
    for(int i=0;i<4;++i){auto e=event(u"aa",u"前");e.levels=i<3?3:1;prefixHistory.push_back(e);}
    auto prefixSnapshot=SentenceLearningSnapshot::build(prefixHistory);
    auto p=event(u"kzjuy",u"淦掉",u"前");
    std::vector<SentenceLearningEvent> exactPlan{p},correctedPlan{p};
    sentenceLearningPlanLevels(exactPlan,prefixSnapshot,u"aakzjuy",u"前滤掉",u"前淦掉",prefixed,prefixed,10,-25,false);
    sentenceLearningPlanLevels(correctedPlan,prefixSnapshot,u"aakzjuy",u"前滤掉",u"前淦掉",prefixed,prefixed,10,-25,true);
    check(exactPlan[0].levels==3 && correctedPlan[0].levels==1,
        "corrected baseline uses its actual score without phantom ordinary prefix reward");

    // Explicit clicks and a crossed Composed opponent remain valid feedback.
    auto candidate=[](std::u16string text,unsigned source,double score,
                      std::initializer_list<SentenceLearningBoundary> boundaries,int rank=1) {
        SentenceCandidate c;c.text=std::move(text);c.source=source;c.baseScore=c.finalScore=score;c.directRank=rank;
        for(auto b:boundaries)c.boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{c.boundary,b.text,b.raw});
        return c;
    };
    SentenceDecodeResult r;r.rawCode=u"kzjuy";r.learningMode=mode;
    r.candidates={candidate(u"滤掉",SentenceSourceComposed,-17.98817910318355,{{3,1},{5,2}}),
                  candidate(u"淦掉",SentenceSourceComposed,-26.322402219101054,{{3,1},{5,2}})};
    SentenceSession session;session.start(r.rawCode,1);apply(session,r);session.commitCandidate(1);
    const auto clicked=session.takeLearning();
    check(clicked.size()==1 && clicked[0].text==u"淦掉" && clicked[0].levels==2,
        "non-Tab explicit phrase click writes one ordinary two-level event");
    r.rawCode=u"ii";r.candidates={candidate(u"甲",SentenceSourceDirect,0,{{2,1}},1),
        candidate(u"丙丁",SentenceSourceComposed,5,{{1,1},{2,2}}),
        candidate(u"乙",SentenceSourceDirect,-2,{{2,1}},2)};
    session.start(r.rawCode,1);apply(session,r);session.commitCandidate(2);
    const auto crossed=session.takeLearning();
    check(crossed.size()==1 && crossed[0].mode==mode && crossed[0].code==u"ii" && crossed[0].text==u"乙",
        "Direct first does not hide a crossed Composed opponent from learning");
    r.candidates.erase(r.candidates.begin()+1);
    session.start(r.rawCode,1);apply(session,r);session.commitCandidate(1);
    check(session.takeLearning().empty(),"pure Direct-to-Direct click preserves the table learning rule");

    // Retired modes cannot enter either snapshot implementation or the active window.
    auto retired=SentenceFusionPreference::event(mode,u"ii",u"乙",u"丙丁",true,2);
    auto correction=retired;correction.id=learningId();correction.mode=u"exact-correction-v1|"+mode;correction.text=u"E";
    check(SentenceLearningSnapshot::build({retired,correction})->empty(),"both retired pair modes are ignored by full replay");
    SentenceLearningAccumulator accumulator;
    check(accumulator.update({retired,correction})->empty(),"retired pair modes are ignored by incremental replay");
    auto mixed=accumulator.update({retired,correction,chosen[0]});
    check(mixed->score(mode,u"kzjuy",u"淦掉",u"")==11,"ordinary history survives mixed retired events");

    const auto path=folder/"retired-window.txt";
    {
        std::ofstream out(path,std::ios::binary);
        out<<u8"学习\t2026-10-10T00:00:00Z\t淦掉\tkzjuy\t\t2\ttest-v1\tsurviving-ordinary\t\n";
        for(int i=0;i<10002;++i)out<<u8"学习\t2026-10-10T00:00:00Z\tE\t~clegacy\t\t1\t"
            <<(i%2?"fusion-v1|test-v1":"exact-correction-v1|test-v1")<<"\tretired-"<<i<<"\t\n";
    }
    const auto originalBytes=std::filesystem::file_size(path);
    SentenceLearningStore store(path);store.refresh();
    check(store.entries().size()==1 && store.snapshot()->score(mode,u"kzjuy",u"淦掉",u"")==11,
        "over ten thousand retired records cannot evict valid ordinary history");
    check(std::filesystem::file_size(path)==originalBytes,"reading old records does not rewrite the user's journal");
    store.confirm({retired,correction});
    check(store.entries().size()==1 && std::filesystem::file_size(path)==originalBytes,
        "new legacy-shaped events are not appended");
    check(store.undoLast() && store.entries().empty() && store.snapshot()->empty(),
        "undo skips retired rows and targets the last ordinary confirmation");
}
