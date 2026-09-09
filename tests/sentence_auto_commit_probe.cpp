#include "../native/SentenceAutoCommit.h"
#include <fstream>
#include <sstream>
#include <iostream>
using namespace tiger;
std::u16string token(const std::string& h){if(h=="-")return {};std::u16string s;for(std::size_t i=0;i<h.size();i+=4)s+=static_cast<char16_t>(std::stoul(h.substr(i,4),nullptr,16));return s;}
void hex(std::u16string_view s){const char* h="0123456789abcdef";std::cout<<'"';for(auto c:s)std::cout<<h[c>>12]<<h[(c>>8)&15]<<h[(c>>4)&15]<<h[c&15];std::cout<<'"';}
int wmain(int argc,wchar_t** argv){try{if(argc!=2)return 2;std::ifstream file(argv[1]);
 SentenceAutoCommit policy;SentenceAutoCommitInput in;SentenceDecodeResult decoded;std::u16string raw,committed;std::string line;
 while(std::getline(file,line)){std::istringstream row(line);std::string type,v;row>>type;
 if(type=="B"){SentenceAutoCommitOptions o;row>>o.requiredEvidence>>o.requiredStrong>>o.minimumRetained>>o.countMergedTail;policy=SentenceAutoCommit(o);committed.clear();in={};}
 if(type=="S"){decoded={};row>>v;raw=token(v);row>>v;decoded.rawCode=token(v);row>>in.enabled>>in.suspended>>in.matchingLexicon>>in.configuredRetained>>decoded.earlyCommitEvidence.confidenceTruncated>>decoded.earlyCommitEvidence.mergedIncompleteTail>>decoded.earlyCommitEvidence.neutralLowConfidence;}
 if(type=="P"){SentencePrefixEvidence p;row>>v;p.text=token(v);row>>p.rawLength>>p.share>>p.boundaryClosed;decoded.earlyCommitEvidence.prefixes.push_back(std::move(p));}
 if(type=="C"){SentenceCandidate c;row>>v;c.text=token(v);row>>c.supplementScore;while(row>>v){auto n=v.find(':');c.boundary=std::make_shared<SentencePathBoundary>(SentencePathBoundary{c.boundary,std::stoi(v.substr(n+1)),std::stoi(v.substr(0,n))});}decoded.candidates.push_back(std::move(c));}
 if(type=="X"){in.raw=raw;in.committedText=committed;auto preview=policy;auto expected=preview.evaluate(in,decoded);auto result=policy.evaluate(in,decoded);
 if(bool(expected)!=bool(result) || (result && (result->text!=expected->text || result->rawLength!=expected->rawLength)))throw std::runtime_error("Preview mutated original policy evidence");
 std::cout<<"{\"commit\":";
 if(result){hex(std::u16string_view(result->text).substr(committed.size()));committed=result->text;in.committedRaw=in.lastAutoCommitRaw=result->rawLength;}else std::cout<<"null";
 std::cout<<",\"text\":";hex(committed);std::cout<<",\"raw\":"<<in.committedRaw<<",\"last\":"<<in.lastAutoCommitRaw<<"}\n";}
 }return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
