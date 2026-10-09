// Automatic rank selection is controlled by one numeric setting.
// Existing confidence/learning fixtures use explicit threshold 1; this group
// verifies the production default and the new 0/2/3/4/long-code boundaries.
static void autoSelectMinimum() {
    const int before=checks;
    SentenceDecoderOptions defaults;
    check(defaults.autoSelectMinCodeLength==3,"auto-select decoder default is three");
    check(SentenceSettings{}.autoSelectMinCodeLength==3,"auto-select settings default is three");
    const std::vector<std::pair<std::u16string,int>> settings={
        {u"",3},{u"自动选重最低码数\t0",0},{u"自动选重最低码数\t1",1},
        {u"自动选重最低码数\t2",2},{u"自动选重最低码数\t3",3},{u"自动选重最低码数\t4",4},
        {u"自动选重最低码数\t128",128},{u"自动选重最低码数\t129",128},
        {u"自动选重最低码数\t-1",0},{u"自动选重最低码数\t+4",4},
        {u"自动选重最低码数\t",3},{u"自动选重最低码数\tbad",3},
        {u"自动选重最低码数\t3.5",3},{u"自动选重最低码数\t１２",3},
        {u"自动选重最低码数\t9999999999999999999999",128},
        {u"自动选重最低码数\t-9999999999999999999999",0},
        {u"自动选重最低码数\t2\n自动选重最低码数\t4",4},
        {u"允许单字重码组句\t否",3},
        {u"允许单字重码组句\t否\n自动选重最低码数\t2",2},
        {u"自动选重最低码数\t0\n允许单字重码组句\t是",0}};
    for(const auto& pair:settings)
        check(parseSentenceSettings(pair.first).autoSelectMinCodeLength==pair.second,"auto-select sole numeric configuration");
    auto l=lex({{u"a",{u"甲",u"乙",u"丙",u"多字"}},
                {u"ab",{u"甲",u"乙",u"丙",u"多字"}},
                {u"abc",{u"甲",u"乙",u"丙",u"多字"}},
                {u"abcd",{u"甲",u"乙",u"丙",u"多字"}},
                {u"abcde",{u"甲",u"乙",u"丙",u"多字"}},
                {u"zz",{u"终"}}});
    auto contains=[](const SentenceDecodeResult& result,std::u16string_view value){
        return std::any_of(result.candidates.begin(),result.candidates.end(),[&](const auto& c){return c.text==value;});
    };
    const std::vector<std::u16string> codes={u"a",u"ab",u"abc",u"abcd",u"abcde"};
    for(int minimum:{0,1,2,3,4,5,128,-1,129}) {
        auto o=options();o.autoSelectMinCodeLength=minimum;SentenceDecoder d(l,{},o);
        const int clamped=std::clamp(minimum,0,128);
        for(const auto& code:codes) {
            const bool autoAllowed=clamped>0 && code.size()>=static_cast<std::size_t>(clamped);
            const auto whole=d.decode(code,40,true);
            check(contains(whole,u"甲") && contains(whole,u"乙") && contains(whole,u"多字"),"whole menu keeps manual candidates");
            for(const auto& c:whole.candidates)if(c.text==u"乙")
                check(c.eligibleDuplicateSinglePath==autoAllowed,"whole short duplicate cannot authorize automatic commit");
            check(d.hasCompleteCandidate(code,u"乙",{},false),"manual whole duplicate remains reachable");
            check(d.hasCompleteCandidate(code,u"乙",{},true)==autoAllowed,"automatic reachability obeys minimum");
            check(!d.hasCompleteCandidate(code,u"多字",{},true),"automatic multi-character word permission is unchanged");
            for(int position=0;position<3;++position) {
                const std::u16string prefix=position?u"zz":u"",suffix=position!=1?u"zz":u"";
                const auto raw=prefix+code+suffix;
                const auto selected=std::u16string(position?u"终":u"")+u"乙"+(position!=1?u"终":u"");
                const auto word=std::u16string(position?u"终":u"")+u"多字"+(position!=1?u"终":u"");
                auto result=d.decode(raw,40,true);
                // One-key internal edges already require an explicit selector.
                const bool allowed=autoAllowed && code.size()>=2;
                check(contains(result,selected)==allowed,"implicit rank threshold in every sentence position");
                check(d.hasCompleteCandidate(raw,selected,{},true)==allowed,"sentence reachability matches decoded gate");
                check(!contains(result,word),"minimum does not introduce implicit multi-character word selection");
                check(equal(result,d.decodeFull(raw,40,true)),"minimum cache/full result parity");
                for(const auto& selection:std::vector<std::pair<std::u16string,std::u16string>>{
                        {u"2",u"乙"},{u";",u"乙"},{u"3",u"丙"},{u"'",u"丙"},{u"4",u"多字"}}) {
                    const auto explicitRaw=prefix+code+selection.first+suffix;
                    const auto target=std::u16string(position?u"终":u"")+selection.second+(position!=1?u"终":u"");
                    auto explicitResult=d.decode(explicitRaw,40,true);
                    check(contains(explicitResult,target),"explicit selector bypasses minimum and disabled state");
                    check(d.hasCompleteCandidate(explicitRaw,target,{},true),"explicit selector complete path is preserved");
                    check(equal(explicitResult,d.decodeFull(explicitRaw,40,true)),"explicit selector cache parity");
                }
            }
            const auto sequence=code+u"zz";
            for(std::size_t n=1;n<=sequence.size();++n) {
                const auto raw=sequence.substr(0,n);
                check(equal(d.decode(raw,40,true),d.decodeFull(raw,40,true)),"default threshold during incremental append");
            }
            for(std::size_t n=sequence.size();n>0;--n) {
                const auto raw=sequence.substr(0,n);
                check(equal(d.decode(raw,40,true),d.decodeFull(raw,40,true)),"default threshold during backspace");
            }
        }
    }
    auto o=defaults;o.rankPenalty=0;o.isolationLambda=0;
    SentenceDecoder production(l,{},o);
    check(!contains(production.decode(u"abzz"),u"乙终"),"default three rejects two-code implicit duplicate");
    check(contains(production.decode(u"abczz"),u"乙终"),"default three admits three-code implicit duplicate");
    check(contains(production.decode(u"ab2zz"),u"乙终"),"default three preserves explicit two-code selection");
    auto locked=std::make_shared<SentenceLockedPrefix>();locked->rawCode=u"zz";locked->text=u"终";
    locked->boundary=production.decode(u"zz").candidates.front().boundary;
    check(!contains(production.decode(u"zzabzz",40,true,u"终",locked),u"终乙终"),"locked prefix cannot bypass short-code gate");
    check(contains(production.decode(u"zzabczz",40,true,u"终",locked),u"终乙终"),"locked prefix admits long-enough automatic selection");
    std::cout<<"{\"test\":\"auto_select_minimum\",\"checks\":"<<(checks-before)<<",\"default\":3,\"zero_disables\":true,\"legacy_key_read\":false}\n";
}
