#include "../native/SentenceResources.h"
#include "../native/SentenceSettings.h"
#include <windows.h>
#include <psapi.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <algorithm>
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
std::uint64_t privateBytes(){PROCESS_MEMORY_COUNTERS_EX m{};m.cb=sizeof(m);if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m),sizeof(m)))throw std::runtime_error("memory query");return m.PrivateUsage;}
std::u16string encode(const tiger::SentenceResources& r,std::u16string_view text){std::u16string raw;auto d=r.lexicon()->dictionary();for(auto c:text){auto code=d->value(d->find(tiger::Section::FullCode,std::u16string_view(&c,1)),0);if(code.empty())throw std::runtime_error("sample character has no primary code");raw+=code;}return raw;}
void mapping(const void* address,std::uint64_t bytes){
 SYSTEM_INFO system{};GetSystemInfo(&system);auto base=static_cast<const unsigned char*>(address);
 auto count=(bytes+system.dwPageSize-1)/system.dwPageSize;
 std::vector<PSAPI_WORKING_SET_EX_INFORMATION> pages(static_cast<std::size_t>(count));
 for(std::size_t i=0;i<pages.size();++i)pages[i].VirtualAddress=const_cast<unsigned char*>(base+i*system.dwPageSize);
 if(!QueryWorkingSetEx(GetCurrentProcess(),pages.data(),static_cast<DWORD>(pages.size()*sizeof(pages[0]))))throw std::runtime_error("working set query");
 std::uint64_t resident=0,shared=0,multiple=0;for(auto& p:pages)if(p.VirtualAttributes.Valid){++resident;if(p.VirtualAttributes.Shared)++shared;if(p.VirtualAttributes.ShareCount>1)++multiple;}
 MEMORY_BASIC_INFORMATION info{};if(!VirtualQuery(address,&info,sizeof(info)))throw std::runtime_error("mapping query");
 std::cout<<"{\"bytes\":"<<bytes<<",\"pages\":"<<count<<",\"resident\":"<<resident<<",\"shareable\":"<<shared<<",\"multiple\":"<<multiple<<",\"type\":"<<info.Type<<",\"protect\":"<<info.Protect<<"}";
}
void touch(const void* address,std::uint64_t bytes){SYSTEM_INFO info{};GetSystemInfo(&info);auto p=static_cast<const unsigned char*>(address);volatile unsigned char sum=0;for(std::uint64_t i=0;i<bytes;i+=info.dwPageSize)sum=static_cast<unsigned char>(sum^p[i]);}

std::pair<const void*,std::uint64_t> mappedModel(const std::filesystem::path& path){
 std::pair<const void*,std::uint64_t> best{};std::uintptr_t address=0;MEMORY_BASIC_INFORMATION info{};
 while(VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info))){
  if(info.State==MEM_COMMIT && info.Type==MEM_MAPPED && info.Protect==PAGE_READONLY){
   wchar_t name[32768]{};if(GetMappedFileNameW(GetCurrentProcess(),info.BaseAddress,name,32768) &&
    _wcsicmp(std::filesystem::path(name).filename().c_str(),path.filename().c_str())==0 && info.RegionSize>best.second)best={info.BaseAddress,info.RegionSize};
  }
  const auto next=reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;if(next<=address)break;address=next;
 }
 if(!best.first)throw std::runtime_error("No read-only model mapping");return best;
}
int wmain(int argc,wchar_t** argv){try{
 if(argc!=4)return 2;std::cout<<std::setprecision(10);const auto baseline=privateBytes();auto start=Clock::now();
 tiger::SentenceSettings settings;tiger::SentenceDecoderOptions options;options.emittedCharacterReward=2;options.wholeInputSingleCharacterReward=5;options.allowDuplicateSingleCharacters=settings.allowDuplicateSingleCharacters;
 auto resources=tiger::SentenceResources::Open(argv[1],argv[2],settings.commonCharacterLimit,settings.whitelist(),options);
 double openMs=elapsed(start);const auto loaded=privateBytes();
 if(std::wstring_view(argv[3])==L"--memory"){
  const auto modelMapping=mappedModel(argv[2]);
  touch(modelMapping.first,modelMapping.second);touch(resources->lexicon()->dictionary()->baseAddress(),resources->lexicon()->dictionary()->mappedBytes());
  std::cout<<"ready"<<std::endl;std::cin.get();
  std::cout<<"{\"pid\":"<<GetCurrentProcessId()<<",\"baseline_private\":"<<baseline<<",\"loaded_private\":"<<loaded<<",\"open_ms\":"<<openMs<<",\"model\":";mapping(modelMapping.first,modelMapping.second);
  std::cout<<",\"lexicon\":";mapping(resources->lexicon()->dictionary()->baseAddress(),resources->lexicon()->dictionary()->mappedBytes());std::cout<<"}"<<std::endl;std::cin.get();return 0;
 }
 if(std::wstring_view(argv[3])!=L"--bench" && std::wstring_view(argv[3])!=L"--contexts")return 2;
 std::cout<<"{\"phase\":\"open\",\"baseline_private\":"<<baseline<<",\"loaded_private\":"<<loaded<<",\"open_ms\":"<<openMs<<"}"<<std::endl;
 auto raw=encode(*resources,u"中国人民使用中文输入法今天我们一起学习新的知识这个问题需要进一步分析和解决");
 while(raw.size()<128)raw+=raw;
 if(std::wstring_view(argv[3])==L"--contexts"){
 auto before=privateBytes();std::vector<std::unique_ptr<tiger::SentenceDecoder>> contexts;
 for(int i=0;i<4;++i){contexts.push_back(resources->createDecoder());contexts.back()->decode(raw.substr(0,32),20,true);std::cout<<"{\"phase\":\"contexts\",\"count\":"<<contexts.size()<<",\"baseline_private\":"<<before<<",\"private\":"<<privateBytes()<<"}"<<std::endl;}
 contexts.clear();std::cout<<"{\"phase\":\"released\",\"private\":"<<privateBytes()<<"}"<<std::endl;
 return 0;
 }
 for(int length:{8,16,32,64,128}){
  std::cout<<"{\"phase\":\"starting_full\",\"raw_length\":"<<length<<"}"<<std::endl;
  auto decoder=resources->createDecoder();auto before=privateBytes();start=Clock::now();auto result=decoder->decode(raw.substr(0,length),20,true);double ms=elapsed(start);
  std::cout<<"{\"phase\":\"full\",\"raw_length\":"<<length<<",\"ms\":"<<ms<<",\"private_before\":"<<before<<",\"private_after\":"<<privateBytes()<<",\"candidates\":"<<result.candidates.size()<<",\"expanded\":"<<result.expandedStates<<"}"<<std::endl;
 }
 auto decoder=resources->createDecoder();std::vector<double> latencies;
 for(int length=1;length<=64;++length){start=Clock::now();decoder->decode(raw.substr(0,length),20,true);latencies.push_back(elapsed(start));}
 auto total=0.0;for(auto t:latencies)total+=t;auto last=latencies.back();std::sort(latencies.begin(),latencies.end());
 std::cout<<"{\"phase\":\"incremental\",\"keys\":64,\"mean_ms\":"<<total/64<<",\"p50_ms\":"<<latencies[32]<<",\"p95_ms\":"<<latencies[60]<<",\"max_ms\":"<<latencies.back()<<",\"last_ms\":"<<last<<",\"private\":"<<privateBytes()<<"}"<<std::endl;

 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
