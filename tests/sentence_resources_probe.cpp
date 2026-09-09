#include "../native/SentenceResources.h"
#include "../native/SentenceImport.h"
#include "../native/LexiconDecode.h"
#include <iostream>
#include <iomanip>
void hex(std::u16string_view t){const char* h="0123456789abcdef";std::cout<<'"';for(auto c:t)std::cout<<h[c>>12]<<h[(c>>8)&15]<<h[(c>>4)&15]<<h[c&15];std::cout<<'"';}
int wmain(int argc,wchar_t** argv) {
 try {
  std::cout<<std::setprecision(17);
  if(argc==3 && std::wstring_view(argv[1])==L"--supplement") {
   auto entries=tiger::parseSentenceSupplements(tiger::readLexiconText(argv[2]));std::cout<<'[';bool first=true;
   for(const auto& e:entries){if(!first)std::cout<<',';first=false;std::cout<<"{\"text\":";hex(e.text);std::cout<<",\"weight\":"<<e.weight<<",\"reward\":"<<e.reward<<'}';}std::cout<<"]\n";return 0;
  }
  if(argc!=3 && argc!=5)return 2;
  tiger::SentenceDecoderOptions options;options.emittedCharacterReward=2;options.wholeInputSingleCharacterReward=5;options.allowDuplicateSingleCharacters=true;
  const auto overridePath=argc==5?std::filesystem::path(argv[3]):std::filesystem::path{};
  const auto revision=argc==5?std::u16string_view(reinterpret_cast<const char16_t*>(argv[4])):std::u16string_view{};
  auto resources=tiger::SentenceResources::Open(argv[1],argv[2],0,{},options,overridePath,revision);
  auto second=tiger::SentenceResources::Open(argv[1],argv[2],0,{},options,overridePath,revision);
  if(resources->model()!=second->model() || resources->lexicon()->dictionary()!=second->lexicon()->dictionary())throw std::runtime_error("Mappings not shared");
  auto decoder=resources->createDecoder();auto result=decoder->decode(u"aabb");
  std::cout<<"{\"mapped_bytes\":"<<resources->model()->mappedBytes()<<",\"aa\":[";bool first=true;
  for(const auto& c:resources->lexicon()->candidates(u"aa")){if(!first)std::cout<<',';first=false;hex(c.text);}
  std::cout<<"],\"decoded\":[";first=true;for(const auto& c:result.candidates){if(!first)std::cout<<',';first=false;hex(c.text);}std::cout<<"]}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
