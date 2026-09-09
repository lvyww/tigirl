#include "../native/SentenceDecoder.h"
#include "../native/SentenceSupplement.h"
#include "../native/LexiconSerialize.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
std::u16string token(const std::string& h){if(h=="-")return {};std::u16string t;for(std::size_t i=0;i<h.size();i+=4)t+=static_cast<char16_t>(std::stoul(h.substr(i,4),nullptr,16));return t;}
void hex(std::u16string_view t){constexpr char h[]="0123456789abcdef";std::cout<<'"';for(auto c:t)std::cout<<h[(c>>12)&15]<<h[(c>>8)&15]<<h[(c>>4)&15]<<h[c&15];std::cout<<'"';}
std::shared_ptr<const tiger::Dictionary> save(const tiger::ImportedLexicon& data,const std::filesystem::path& path) {
 auto bytes=tiger::serializeImportedLexicon(data);{std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!f)throw std::runtime_error("Write fixture");}return tiger::Dictionary::Open(path);
}
int wmain(int argc,wchar_t** argv) {
 if(argc!=5)return 2;
 bool incremental=std::wstring_view(argv[4])==L"1";
 try {
  std::shared_ptr<const tiger::SentenceLanguageModel> model;if(std::wstring_view(argv[3])!=L"-")model=tiger::SentenceNgram::Open(argv[3]);
  std::ifstream file(argv[1]);if(!file)throw std::runtime_error("Missing fixture");std::filesystem::path output(argv[2]);
  std::vector<tiger::ImportedLexiconEntry> source;std::vector<tiger::SentenceSupplementEntry> supplements;
  tiger::SentenceLexicon::Characters common,white;std::vector<std::pair<std::u16string,std::u16string>> queries;
  tiger::SentenceDecoderOptions options;int id=0,limit=20;std::string line;std::cout<<std::setprecision(17);
  while(std::getline(file,line)) {
   std::istringstream row(line);std::string type,v;row>>type;
   if(type=="B"){row>>id;source.clear();supplements.clear();queries.clear();common.clear();white.clear();}
   if(type=="C" || type=="W")while(row>>v)(type=="C"?common:white).insert(token(v));
   if(type=="E"){row>>v;tiger::ImportedLexiconEntry entry{token(v),{}};while(row>>v)entry.candidates.push_back(token(v));
    auto old=std::find_if(source.begin(),source.end(),[&](const auto& e){return e.code==entry.code;});if(old==source.end())source.push_back(std::move(entry));else old->candidates=std::move(entry.candidates);}
   if(type=="S"){std::int64_t weight;row>>v>>weight;supplements.push_back(tiger::SentenceSupplementEntry::create(token(v),weight));}
   if(type=="Q"){row>>v;auto raw=token(v);v="-";row>>v;queries.push_back({raw,token(v)});}
   if(type=="O")row>>options.beamWidth>>options.rankPenalty>>options.isolationRankThreshold>>options.isolationLambda>>options.isolationUseLogRank>>options.scoreSentenceBoundaries>>options.emittedCharacterReward>>options.wholeInputSingleCharacterReward>>options.allowDuplicateSingleCharacters>>limit;
   if(type!="X")continue;
   auto lexicon=std::make_shared<tiger::SentenceLexicon>(save(tiger::prepareSentenceLexicon(source),output/(std::to_string(id)+".tcs")),common,white);
   auto supplement=std::make_shared<tiger::MappedSentenceSupplement>(save(tiger::SentenceSupplementMatcher(supplements).serializeGraph(),output/(std::to_string(id)+".tss")));
   tiger::SentenceDecoder decoder(lexicon,model,options,supplement);
   int number=0;
   for(const auto& q:queries) {
    int n=number++,queryLimit=n%5==0?1:limit;bool evidence=n%3!=0;
    if(n%17==0)decoder.resetDecodeCache();
    std::cout<<"{\"id\":"<<id<<",\"query\":";hex(q.first);
    try {
     auto result=incremental?decoder.decode(q.first,queryLimit,evidence,q.second):decoder.decodeFull(q.first,queryLimit,evidence,q.second);std::cout<<",\"exists\":["<<(decoder.hasCompleteCandidate(q.first)?"true":"false")<<','<<(decoder.hasCompleteCandidate(q.first,q.second)?"true":"false")<<','<<(decoder.hasCompleteCandidate(q.first,q.second,result.candidates.empty()?std::u16string_view{}:std::u16string_view(result.candidates.front().text),true)?"true":"false")<<"],\"properPrefix\":"<<(decoder.isProperCodePrefix(q.first)?"true":"false")<<",\"raw\":";hex(result.rawCode);std::cout<<",\"expanded\":"<<result.expandedStates<<",\"candidates\":[";bool first=true;
     for(const auto& c:result.candidates) {
      if(!first)std::cout<<',';first=false;std::cout<<"{\"text\":";hex(c.text);std::cout<<",\"segmented\":";hex(c.segmentedCode);
      std::cout<<",\"score\":"<<c.finalScore<<",\"confidence\":"<<c.confidenceScore<<",\"supplement\":"<<c.supplementScore<<",\"rank\":"<<c.maxLexiconRank<<",\"boundaries\":[";bool fb=true;
      for(auto b=c.boundary;b;b=b->previous){if(!fb)std::cout<<',';fb=false;std::cout<<'['<<b->textLength<<','<<b->rawLength<<']';}std::cout<<"]}";
     }std::cout<<"],\"evidence\":{";
     const auto& e=result.earlyCommitEvidence;std::cout<<"\"prefixes\":[";bool fp=true;
     for(const auto& p:e.prefixes){if(!fp)std::cout<<',';fp=false;std::cout<<"{\"text\":";hex(p.text);std::cout<<",\"raw\":"<<p.rawLength<<",\"share\":"<<p.share<<",\"boundaryShare\":"<<p.boundaryShare<<",\"closed\":"<<(p.boundaryClosed?"true":"false")<<'}';}
     std::cout<<"],\"neutralIncomplete\":"<<(e.neutralIncompleteTail?"true":"false")<<",\"merged\":"<<(e.mergedIncompleteTail?"true":"false")<<",\"low\":"<<(e.neutralLowConfidence?"true":"false")<<",\"truncated\":"<<(e.confidenceTruncated?"true":"false")<<",\"proposal\":";hex(e.proposal);
     std::cout<<",\"share\":"<<e.proposalShare<<",\"rawLengths\":{";bool fr=true;for(const auto& r:e.rawLengths){if(!fr)std::cout<<',';fr=false;hex(r.first);std::cout<<':'<<r.second;}std::cout<<"}}}\n";
    }catch(const std::invalid_argument&){std::cout<<",\"error\":\"selector\"}\n";}
    catch(const std::out_of_range&){std::cout<<",\"error\":\"selector\"}\n";}
   }
  }return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
