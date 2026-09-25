#include "SentenceFivegram.h"
#include "SentenceDecoder.h"
#include "SentenceSession.h"
#include "SentenceCharacterRanks.h"
#include "SentenceSupplement.h"
#include "LexiconSerialize.h"
#include "Text.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <future>
#include <atomic>
using namespace tiger;
namespace {
int checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
std::shared_ptr<const Dictionary> save(const ImportedLexicon& data,const std::filesystem::path& path) {
    auto bytes=serializeImportedLexicon(data);{std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
    return Dictionary::Open(path);
}
void same(const SentenceDecodeResult& a,const SentenceDecodeResult& b) {
    require(a.candidates.size()==b.candidates.size(),"candidate count differs");
    for(std::size_t i=0;i<a.candidates.size();++i) {
        const auto& x=a.candidates[i];const auto& y=b.candidates[i];
        require(x.text==y.text && x.segmentedCode==y.segmentedCode,"candidate order differs");
        require(std::abs(x.finalScore-y.finalScore)<1e-9 && std::abs(x.confidenceScore-y.confidenceScore)<1e-9,"candidate score differs");
    }
}
class OldPrior final:public SentenceHistoryLanguageModel {
    std::shared_ptr<const SentenceLanguageModel> five_,prior_;
    const SentenceHistoryLanguageModel* history_;
public:
    OldPrior(std::shared_ptr<const SentenceLanguageModel> five,std::shared_ptr<const SentenceLanguageModel> prior):five_(std::move(five)),prior_(std::move(prior)),history_(dynamic_cast<const SentenceHistoryLanguageModel*>(five_.get())){}
    std::shared_ptr<const SentenceLanguageModel> querySession() const override{return std::make_shared<OldPrior>(five_->querySession(),prior_);}
    SentenceLmHistory beginHistory() const override{return history_->beginHistory();}
    double step(SentenceLmHistory& h,std::u16string_view text) const override{return history_->step(h,text);}
    double logProbability(std::u16string_view,std::u16string_view,std::u16string_view,bool=true) const override{throw std::logic_error("history required");}
    bool hasObservedBigram(std::u16string_view a,std::u16string_view b) const override{return prior_->hasObservedBigram(a,b);}
};
void tests(const std::filesystem::path& path,const std::filesystem::path& work) {
    auto model=SentenceFivegram::Open(path);require(model==SentenceFivegram::Open(path),"model mapping not shared");
    struct FrozenScore {const char16_t* text;double total;};
    // Frozen from the independent Python reference reader, Q8 model 756f6c92cf43.
    const FrozenScore scores[]={
        {u"中国人民今天一起学习",-36.852852225659745},
        {u"马云雷军入选大亨名单",-65.71360926100293},
        {u"甲乙丙丁戊己庚辛",-27.05397090819455},
        {u"龘",-19.69900789553629},
        {u"😀",-37.422274335091686},
        {u"",-13.870029427041857},
        {u"的",-15.559373613006994},
    };
    for(const auto& sample:scores) {
        auto query=model->querySession();auto lm=dynamic_cast<const SentenceHistoryLanguageModel*>(query.get());auto history=lm->beginHistory();double actual=0;
        for(const auto& element:wordTextElements(sample.text))actual+=lm->step(history,element);
        actual+=lm->step(history,u"\x03");require(std::abs(actual-sample.total)<1e-9,"frozen TCSKNM03 score mismatch");
    }
    struct FrozenBigram {const char16_t* previous;const char16_t* target;bool observed;};
    const FrozenBigram bigrams[]={
        {u"中",u"国",true},{u"人",u"民",true},{u"今",u"天",true},{u"学",u"习",true},
        {u"国",u"人",true},{u"马",u"云",true},{u"雷",u"军",true},
        {u"龘",u"𰻞",false},{u"中",u"龘",false},{u"😀",u"中",false},{u"",u"中",false}
    };
    int hits=0,misses=0;for(const auto& item:bigrams){require(model->hasObservedBigram(item.previous,item.target)==item.observed,"frozen observed bigram mismatch");item.observed?++hits:++misses;}
    require(hits && misses,"observed cases must cover both branches");
    ImportedLexicon data;data.main={{u"aa",{u"中",u"人",u"龘"}},{u"bb",{u"国",u"民",u"𰻞"}},{u"cc",{u"人民",u"今天"}},{u"dd",{u"学习",u"工作"}}};
    auto lexicon=std::make_shared<SentenceLexicon>(save(prepareSentenceLexicon(data.main),work/"fixture.tcd"));
    SentenceDecoderOptions options;options.beamWidth=200;options.allowDuplicateSingleCharacters=true;options.emittedCharacterReward=2;options.wholeInputSingleCharacterReward=5;
    SentenceDecoder incremental(lexicon,model,options),full(lexicon,model,options);
    for(auto raw:{u"aabbccddaabb",u"aabbaabbccddaabb"}) {
        const std::u16string input(raw);
        for(std::size_t n=0;n<=input.size();++n)same(incremental.decode(input.substr(0,n),20,true),full.decodeFull(input.substr(0,n),20,true));
        for(std::size_t n=input.size();n>0;--n)same(incremental.decode(input.substr(0,n),20,true),full.decodeFull(input.substr(0,n),20,true));
    }
    const std::u16string raw=u"aabbccdd";auto before=incremental.decode(raw,20,true);require(!before.candidates.empty(),"no test candidates");
    auto lock=std::make_shared<SentenceLockedPrefix>();lock->rawCode=raw;lock->text=before.candidates.back().text;lock->boundary=before.candidates.back().boundary;
    same(incremental.decode(raw+u"aabb",20,true,{},lock),full.decode(raw+u"aabb",20,true,{},lock));
    for(const auto& c:incremental.decode(raw+u"aabb",20,true,{},lock).candidates)require(c.text.substr(0,lock->text.size())==lock->text,"lock lost");
    auto cancelled=std::make_shared<std::atomic<bool>>(true);bool rejected=false;
    try{incremental.decode(raw,20,true,{},{},cancelled);}catch(const SentenceDecodeCancelled&){rejected=true;}
    require(rejected,"cancellation ignored");same(incremental.decode(raw,20,true),full.decodeFull(raw,20,true));
    incremental.retainCommittedHistory(raw,4);same(incremental.decode(raw+u"aabb",20,true),full.decodeFull(raw+u"aabb",20,true));
    std::u16string longRaw;for(int i=0;i<45;++i)longRaw+=u"aa";
    incremental.decode(longRaw,20,true);const auto positions=incremental.memoryStatus().positions;
    incremental.retainCommittedHistory(longRaw,80);require(incremental.memoryStatus().positions<positions,"history trim not exercised");
    same(incremental.decode(longRaw+u"bb",20,true),full.decodeFull(longRaw+u"bb",20,true));
    auto pureOptions=options;pureOptions.rankPenalty=0;pureOptions.isolationRankThreshold=0;pureOptions.wholeInputSingleCharacterReward=0;
    SentenceDecoder pure(lexicon,model,pureOptions);
    for(const auto& c:pure.decode(u"aabbccddaabb").candidates){
        auto h=model->beginHistory();double score=0;for(const auto& e:wordTextElements(c.text))score+=model->step(h,e)+pureOptions.emittedCharacterReward;
        score+=model->step(h,u"\x03");require(std::abs(score-c.finalScore)<1e-9,"decoder lost full model history");
    }
    {std::ifstream in(path,std::ios::binary);char header[512]{};in.read(header,sizeof(header));std::ofstream out(work/"truncated.bin",std::ios::binary);out.write(header,in.gcount());}
    rejected=false;try{SentenceFivegram::Open(work/"truncated.bin");}catch(const std::exception&){rejected=true;}require(rejected,"truncated model accepted");
    ImportedLexicon duplicateData;duplicateData.main={{u"aa",{u"中"}},{u"bb",{u"国"}},{u"aabb",{u"中国"}},{u"cc",{u"人"}}};
    auto duplicateLexicon=std::make_shared<SentenceLexicon>(save(prepareSentenceLexicon(duplicateData.main),work/"duplicates.tcd"));
    SentenceDecoder duplicates(duplicateLexicon,model,pureOptions);
    auto merged=duplicates.decode(u"aabbcc",20,true);
    require(merged.candidates.size()==1 && merged.candidates[0].text==u"中国人","equivalent segmented paths were not merged");
    require(std::abs(merged.candidates[0].confidenceScore-(merged.candidates[0].finalScore+std::log(2.0)))<1e-9,"duplicate path probability mass lost");
    options.scoreSentenceBoundaries=false;rejected=false;try{SentenceDecoder invalid(lexicon,model,options);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"boundary option accepted");
    std::vector<std::future<bool>> tasks;
    for(int n=0;n<4;++n)tasks.push_back(std::async(std::launch::async,[model]{auto query=model->querySession();auto h=dynamic_cast<const SentenceHistoryLanguageModel*>(query.get())->beginHistory();for(int i=0;i<1000;++i)dynamic_cast<const SentenceHistoryLanguageModel*>(query.get())->step(h,u"中");return true;}));
    for(auto& task:tasks)require(task.get(),"concurrent query failed");
    auto survivor=model->querySession();model.reset();auto lm=dynamic_cast<const SentenceHistoryLanguageModel*>(survivor.get());auto h=lm->beginHistory();require(std::isfinite(lm->step(h,u"中")),"query lease lost");
    for(auto file:{work/"missing.bin",work/"bad.bin"}) {
        if(file.filename()=="bad.bin"){std::ofstream out(file);out<<"bad model";}
        rejected=false;try{SentenceFivegram::Open(file);}catch(const std::exception&){rejected=true;}require(rejected,"invalid model accepted");
    }
    std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"mapped_bytes\":"<<survivor->mappedBytes()<<"}\n";
}
std::vector<std::string> fields(std::string line) {std::vector<std::string> out;std::istringstream in(line);for(std::string s;std::getline(in,s,'\t');)out.push_back(s);return out;}
void oracle(const std::filesystem::path& modelPath,const std::filesystem::path& cases) {
    auto model=SentenceFivegram::Open(modelPath);std::ifstream in(cases);require(bool(in),"missing oracle cases");
    unsigned count=0;double maximum=0;
    for(std::string line;std::getline(in,line);){
        if(line.empty()||line[0]=='#')continue;
        if(line.back()=='\r')line.pop_back();auto f=fields(line);require(f.size()>=2,"invalid oracle case");
        auto h=model->beginHistory();double actual=0;
        for(std::size_t i=1;i<f.size();++i)actual=model->step(h,utf16(f[i]));
        const auto error=std::abs(actual-std::stod(f[0]));maximum=std::max(maximum,error);
        require(error<1e-9,"Q8 oracle score mismatch");++count;
    }
    require(count>0,"oracle coverage missing");
    std::cout<<"{\"status\":\"passed\",\"queries\":"<<count<<",\"max_error\":"<<maximum<<"}\n";
}
void evaluate(const std::filesystem::path& modelPath,const std::filesystem::path& priorPath,const std::filesystem::path& fixture,const std::filesystem::path& cases,const std::filesystem::path& output,const std::filesystem::path& work) {
    ImportedLexicon data;std::map<std::u16string,std::size_t> indices;std::ifstream codefile(fixture/"tiger_sentence.codes.txt");
    for(std::string line;std::getline(codefile,line);){if(line.empty() || line[0]=='#')continue;std::istringstream row(line);std::string text,code;if(!(row>>text>>code))continue;auto c=utf16(code),t=utf16(text);auto found=indices.find(c);if(found==indices.end()){indices[c]=data.main.size();data.main.push_back({c,{t}});}else {auto& v=data.main[found->second].candidates;if(std::find(v.begin(),v.end(),t)==v.end())v.push_back(t);}}
    require(!data.main.empty(),"empty evaluation code table");
    SentenceLexicon::Characters whitelist;std::ifstream whitefile(fixture/"tiger_sentence.full_code_whitelist.txt");std::string white((std::istreambuf_iterator<char>(whitefile)),{});for(auto& c:wordTextElements(utf16(white)))whitelist.insert(c);
    auto lexicon=std::make_shared<SentenceLexicon>(save(prepareSentenceLexicon(data.main),work/"eval.tcd"),sentenceTopCharacters(1500),whitelist);
    std::vector<SentenceSupplementEntry> entries;std::ifstream supplementFile(fixture/"tiger_sentence.supplement.txt");
    for(std::string line;std::getline(supplementFile,line);){line=line.substr(0,line.find('#'));std::istringstream row(line);std::string text;long long weight=1000;if(row>>text){row>>weight;entries.push_back(SentenceSupplementEntry::create(utf16(text),weight));}}
    auto supplement=std::make_shared<MappedSentenceSupplement>(save(SentenceSupplementMatcher(entries).serializeGraph(),work/"supplement.tcd"));
    auto lexical=SentenceLexicalPrior::Open(fixture/"tiger_sentence.lexical.bin");
    auto model=SentenceFivegram::Open(modelPath);auto prior=SentenceNgram::Open(priorPath);auto old=std::make_shared<OldPrior>(model,prior);
    SentenceDecoderOptions options;options.emittedCharacterReward=2;options.wholeInputSingleCharacterReward=5;options.allowDuplicateSingleCharacters=true;options.canonicalCodeReward=2;options.canonicalIsolationFactor=0;options.canonicalIsolationMinCodeLength=4;options.lexicalPriorWeight=.1;options.lexicalCandidateLimit=5;
    SentenceDecoder unified(lexicon,model,options,supplement,lexical),control(lexicon,old,options,supplement,lexical);
    std::ifstream input(cases);std::ofstream out(output);require(bool(input)&&bool(out),"evaluation paths unavailable");out<<"id\tsource\tcode\ttarget\tcontrol\tunified\tcontrol_rank\tunified_rank\tcontrol_ms\tunified_ms\n";
    unsigned rows=0;
    for(std::string line;std::getline(input,line);){if(!line.empty()&&line.back()=='\r')line.pop_back();auto f=fields(line);if(f.size()<4 || f[0]=="id")continue;auto code=utf16(f[2]),target=utf16(f[3]);control.resetDecodeCache();unified.resetDecodeCache();
        auto start=std::chrono::steady_clock::now();auto a=control.decode(code);auto middle=std::chrono::steady_clock::now();auto b=unified.decode(code);auto end=std::chrono::steady_clock::now();
        auto rank=[&](const SentenceDecodeResult& result){for(std::size_t i=0;i<result.candidates.size();++i)if(result.candidates[i].text==target)return int(i)+1;return 0;};
        out<<f[0]<<'\t'<<f[1]<<'\t'<<f[2]<<'\t'<<f[3]<<'\t'<<(a.candidates.empty()?"":utf8(a.candidates[0].text))<<'\t'<<(b.candidates.empty()?"":utf8(b.candidates[0].text))<<'\t'<<rank(a)<<'\t'<<rank(b)<<'\t'<<std::chrono::duration<double,std::milli>(middle-start).count()<<'\t'<<std::chrono::duration<double,std::milli>(end-middle).count()<<'\n';
        if(++rows%250==0){out.flush();std::cout<<"rows "<<rows<<std::endl;}
    }
}
int run(const std::vector<std::filesystem::path>& args) {
    try{std::cout<<std::setprecision(17);if(args.size()==4 && args[1]=="test"){std::filesystem::create_directories(args[3]);tests(args[2],args[3]);return 0;}
        if(args.size()==4 && args[1]=="oracle"){oracle(args[2],args[3]);return 0;}
        if(args.size()==3 && args[1]=="reject"){bool rejected=false;try{SentenceFivegram::Open(args[2]);}catch(const std::exception&){rejected=true;}require(rejected,"unsupported model accepted");std::cout<<"rejected unsupported model\n";return 0;}
        if(args.size()==8 && args[1]=="eval"){std::filesystem::create_directories(args[7]);evaluate(args[2],args[3],args[4],args[5],args[6],args[7]);return 0;}
        std::cerr<<"test MODEL WORK | oracle MODEL CASES | reject MODEL | eval MODEL PRIOR FIXTURE CASES OUTPUT WORK\n";return 2;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){return run(std::vector<std::filesystem::path>(argv,argv+argc));}
#else
int main(int argc,char** argv){return run(std::vector<std::filesystem::path>(argv,argv+argc));}
#endif
