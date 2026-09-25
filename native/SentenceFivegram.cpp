#include "SentenceFivegram.h"
#include "FileCachePath.h"
#include "Text.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace tiger {
namespace {
constexpr std::uint64_t headerSize=256,bucketCount=256,bucketMetaSize=40,indexEntrySize=16;
constexpr double ln10=2.3025850929940456840179914546843642;
[[noreturn]] void invalid(){throw std::runtime_error("Invalid TCSKNM03 v2 Q8 fivegram model");}
template<class T>T load(const unsigned char* p){T value;std::memcpy(&value,p,sizeof(value));return value;}
int compareIds(const std::uint16_t* a,const std::uint16_t* b,int count){for(int i=0;i<count;++i){if(a[i]<b[i])return -1;if(a[i]>b[i])return 1;}return 0;}
struct Quant {double pmin=0,pstep=0,bmin=0,bstep=0;};
struct Bucket {std::uint64_t blocks=0,bytes=0,index=0,records=0;std::uint32_t indexes=0,contexts=0;};
struct Lookup {double probability=0,backoff=0;bool observed=false;};
struct Token {std::uint16_t id=0;bool found=false;};
}
struct SentenceFivegram::Data {
    const unsigned char* data=nullptr;std::uint64_t length=0;
#ifdef _WIN32
    void* file=reinterpret_cast<void*>(static_cast<std::intptr_t>(-1));void* mapping=nullptr;
#else
    int file=-1;
#endif
    std::uint32_t vocabCount=0,indexStride=0;std::uint16_t unknown=0,bos=0,eos=0;
    std::array<Quant,5> quant{};std::array<std::array<Bucket,bucketCount>,4> buckets{};
    std::vector<double> unigrams;std::unordered_map<std::string_view,std::uint16_t> tokenIds;
    explicit Data(const std::filesystem::path& path){map(path);try{validate();}catch(...){unmap();throw;}}
    ~Data(){unmap();}
    Data(const Data&)=delete;Data& operator=(const Data&)=delete;
    template<class T>T read(std::uint64_t offset) const {if(offset>length || sizeof(T)>length-offset)invalid();return load<T>(data+offset);}
    void map(const std::filesystem::path& path){
#ifdef _WIN32
        file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open TCSKNM03 fivegram model");
        LARGE_INTEGER size{};if(!GetFileSizeEx(file,&size)||size.QuadPart<=0)invalid();length=static_cast<std::uint64_t>(size.QuadPart);
        mapping=CreateFileMappingW(file,nullptr,PAGE_READONLY,0,0,nullptr);if(!mapping)throw std::runtime_error("Cannot map TCSKNM03 fivegram model");
        data=static_cast<const unsigned char*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,0));if(!data)throw std::runtime_error("Cannot map TCSKNM03 fivegram model");
#else
        file=::open(path.c_str(),O_RDONLY|O_CLOEXEC);if(file<0)throw std::runtime_error("Cannot open TCSKNM03 fivegram model");
        struct stat info{};if(fstat(file,&info)||info.st_size<=0)invalid();length=static_cast<std::uint64_t>(info.st_size);
        void* view=mmap(nullptr,static_cast<std::size_t>(length),PROT_READ,MAP_SHARED,file,0);if(view==MAP_FAILED)throw std::runtime_error("Cannot map TCSKNM03 fivegram model");data=static_cast<const unsigned char*>(view);
#endif
    }
    void unmap() noexcept {
#ifdef _WIN32
        if(data)UnmapViewOfFile(data);if(mapping)CloseHandle(mapping);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);data=nullptr;mapping=nullptr;file=INVALID_HANDLE_VALUE;
#else
        if(data)munmap(const_cast<unsigned char*>(data),static_cast<std::size_t>(length));if(file>=0)close(file);data=nullptr;file=-1;
#endif
    }
    double probability(int order,std::uint16_t value) const {const auto& q=quant[order-1];return q.pmin+value*q.pstep;}
    double backoff(int contextOrder,std::uint16_t value) const {if(!value)return 0;const auto& q=quant[contextOrder-1];return q.bmin+(value-1)*q.bstep;}
    void validate(){
        const std::uint32_t endian=1;if(*reinterpret_cast<const unsigned char*>(&endian)!=1||length<headerSize||std::memcmp(data,"TCSKNM03",8))invalid();
        if(read<std::uint32_t>(8)!=2||read<std::uint32_t>(12)!=headerSize||read<std::uint64_t>(16)!=length||read<std::uint32_t>(24)!=5)invalid();
        vocabCount=read<std::uint32_t>(28);if(!vocabCount||vocabCount>=65535||read<std::uint32_t>(32)!=bucketCount)invalid();
        indexStride=read<std::uint32_t>(36);if(indexStride<16||indexStride>65536)invalid();
        const auto vocabOffset=read<std::uint64_t>(40),vocabBytes=read<std::uint64_t>(48);unknown=read<std::uint16_t>(56);bos=read<std::uint16_t>(58);eos=read<std::uint16_t>(60);
        if(read<std::uint16_t>(62)||unknown>=vocabCount||bos>=vocabCount||eos>=vocabCount||vocabOffset>length||vocabBytes>length-vocabOffset)invalid();
        for(int i=0;i<5;++i){const auto at=160+static_cast<std::uint64_t>(i)*16;quant[i].pmin=read<std::int32_t>(at)/1e7;quant[i].pstep=read<std::uint32_t>(at+4)/1e9;quant[i].bmin=read<std::int32_t>(at+8)/1e7;quant[i].bstep=read<std::uint32_t>(at+12)/1e9;if(!std::isfinite(quant[i].pmin)||!std::isfinite(quant[i].pstep)||!std::isfinite(quant[i].bmin)||!std::isfinite(quant[i].bstep))invalid();}
        unigrams.resize(vocabCount);tokenIds.reserve(vocabCount*2);std::uint64_t position=vocabOffset,end=vocabOffset+vocabBytes;
        for(std::uint32_t id=0;id<vocabCount;++id){if(position>end||4>end-position)invalid();const auto size=read<std::uint16_t>(position);position+=2;if(size>end-position||2>end-position-size)invalid();std::string_view token(reinterpret_cast<const char*>(data+position),size);position+=size;const auto pq=read<std::uint8_t>(position);position+=1;read<std::uint8_t>(position);position+=1;unigrams[id]=probability(1,pq);if(!tokenIds.emplace(token,static_cast<std::uint16_t>(id)).second)invalid();}
        if(position!=end)invalid();auto special=[&](std::string_view text,std::uint16_t id){auto it=tokenIds.find(text);return it!=tokenIds.end()&&it->second==id;};if(!special("<unk>",unknown)||!special("<s>",bos)||!special("</s>",eos))invalid();
        for(int section=0;section<4;++section){const int order=section+2;const auto sh=64+static_cast<std::uint64_t>(section)*24;const auto directory=read<std::uint64_t>(sh);const auto totalContexts=read<std::uint64_t>(sh+8),totalRecords=read<std::uint64_t>(sh+16);if(directory>length||bucketCount*bucketMetaSize>length-directory)invalid();std::uint64_t contexts=0,records=0;
            for(std::uint32_t bucket=0;bucket<bucketCount;++bucket){const auto at=directory+static_cast<std::uint64_t>(bucket)*bucketMetaSize;auto& m=buckets[section][bucket];m.blocks=read<std::uint64_t>(at);m.bytes=read<std::uint64_t>(at+8);m.index=read<std::uint64_t>(at+16);m.indexes=read<std::uint32_t>(at+24);m.contexts=read<std::uint32_t>(at+28);m.records=read<std::uint64_t>(at+32);contexts+=m.contexts;records+=m.records;if(!m.contexts){if(m.bytes||m.indexes||m.records||m.blocks>length||m.index!=m.blocks)invalid();continue;}if(!m.indexes||m.indexes!=(m.contexts+indexStride-1)/indexStride||m.blocks>length||m.bytes>length-m.blocks||m.index!=m.blocks+m.bytes||m.index>length||static_cast<std::uint64_t>(m.indexes)*indexEntrySize>length-m.index)invalid();
                std::array<std::uint16_t,4> previous{};std::uint64_t previousOffset=0;bool have=false;for(std::uint32_t n=0;n<m.indexes;++n){const auto ix=m.index+static_cast<std::uint64_t>(n)*indexEntrySize;std::array<std::uint16_t,4> key{};for(int k=0;k<order-1;++k){key[k]=read<std::uint16_t>(ix+k*2);if(key[k]>=vocabCount)invalid();}const auto offset=read<std::uint64_t>(ix+8);if(static_cast<std::uint32_t>(key[0]&0xffu)!=bucket||offset<m.blocks||offset>=m.index||(n==0&&offset!=m.blocks)||(have&&(compareIds(previous.data(),key.data(),order-1)>=0||offset<=previousOffset)))invalid();previous=key;previousOffset=offset;have=true;}}
            if(contexts!=totalContexts||records!=totalRecords)invalid();}
    }
    Token token(std::u16string_view text) const {
        if(text==u"\x03")return {eos,true};if(text==u"\x02")return {bos,true};
        try{auto encoded=utf8(text);auto it=tokenIds.find(encoded);if(it!=tokenIds.end())return {it->second,true};}catch(const std::invalid_argument&){}
        return {unknown,false};
    }
    Lookup lookup(int order,const std::array<std::uint16_t,4>& context,std::uint16_t target) const {
        if(order<2||order>5||target>=vocabCount)invalid();const int contextLength=order-1,section=order-2;const auto& m=buckets[section][context[0]&0xff];if(!m.contexts)return {};
        std::uint32_t low=0,high=m.indexes;while(low<high){const auto mid=low+(high-low)/2;const auto at=m.index+static_cast<std::uint64_t>(mid)*indexEntrySize;std::array<std::uint16_t,4> key{};for(int i=0;i<contextLength;++i)key[i]=read<std::uint16_t>(at+i*2);if(compareIds(key.data(),context.data(),contextLength)<=0)low=mid+1;else high=mid;}if(!low)return {};
        const auto page=low-1;const std::uint64_t indexAt=m.index+static_cast<std::uint64_t>(page)*indexEntrySize;std::uint64_t position=read<std::uint64_t>(indexAt+8);const std::uint64_t pageEnd=page+1<m.indexes?read<std::uint64_t>(indexAt+indexEntrySize+8):m.index;if(position<m.blocks||position>=pageEnd||pageEnd>m.index)invalid();
        const auto blockHeader=static_cast<std::uint64_t>(contextLength)*2+3;for(std::uint32_t n=0;n<indexStride&&position<pageEnd;++n){if(blockHeader>pageEnd-position)invalid();std::array<std::uint16_t,4> key{};for(int i=0;i<contextLength;++i)key[i]=read<std::uint16_t>(position+i*2);const auto compared=compareIds(key.data(),context.data(),contextLength);const auto bq=read<std::uint8_t>(position+contextLength*2);const std::uint32_t count=read<std::uint16_t>(position+contextLength*2+1);const auto successors=position+blockHeader;const auto successorBytes=static_cast<std::uint64_t>(count)*3;if(successorBytes>pageEnd-successors)invalid();if(compared==0){std::uint32_t left=0,right=count;while(left<right){const auto mid=left+(right-left)/2;const auto id=read<std::uint16_t>(successors+static_cast<std::uint64_t>(mid)*3);if(id<target)left=mid+1;else right=mid;}const auto bow=backoff(contextLength,bq);if(left<count){const auto at=successors+static_cast<std::uint64_t>(left)*3;if(read<std::uint16_t>(at)==target)return {probability(order,read<std::uint8_t>(at+2)),bow,true};}return {0,bow,false};}if(compared>0)return {};position=successors+successorBytes;}return {};
    }
    double score(const SentenceLmHistory& history,std::uint16_t target) const {
        if(history.count>4)throw std::invalid_argument("Invalid fivegram history");double total=0;const int maximum=static_cast<int>(std::min<std::uint32_t>(4,history.count));for(int contextSize=maximum;contextSize>=1;--contextSize){std::array<std::uint16_t,4> context{};for(int i=0;i<contextSize;++i){const auto id=history.tokens[contextSize-1-i];if(id>=vocabCount)throw std::invalid_argument("Invalid fivegram token");context[i]=static_cast<std::uint16_t>(id);}const auto found=lookup(contextSize+1,context,target);if(found.observed)return (total+found.probability)*ln10;total+=found.backoff;}return (total+unigrams[target])*ln10;
    }
};
struct SentenceFivegram::Cache {
    struct Hash{std::size_t operator()(const std::array<std::uint32_t,6>& key)const{std::size_t h=2166136261u;for(auto n:key){h^=n;h*=16777619u;}return h;}};
    std::mutex mutex;std::unordered_map<std::u16string,Token> tokens;std::unordered_map<std::array<std::uint32_t,6>,double,Hash> scores;std::unordered_map<std::uint64_t,bool> observed;
    Token token(const Data& data,std::u16string_view text){std::u16string key(text);auto it=tokens.find(key);if(it!=tokens.end())return it->second;auto value=data.token(text);if(tokens.size()>=32768)tokens.clear();tokens.emplace(std::move(key),value);return value;}
};
SentenceFivegram::SentenceFivegram(std::shared_ptr<const Data> data):data_(std::move(data)),cache_(std::make_unique<Cache>()){}
SentenceFivegram::~SentenceFivegram()=default;
std::shared_ptr<const SentenceFivegram> SentenceFivegram::Open(const std::filesystem::path& path){static std::mutex mutex;using Key=std::tuple<std::filesystem::path,std::uintmax_t,std::filesystem::file_time_type>;static std::map<Key,std::weak_ptr<const SentenceFivegram>> models;const Key key{fileCachePath(path),std::filesystem::file_size(path),std::filesystem::last_write_time(path)};std::lock_guard<std::mutex> lock(mutex);for(auto it=models.begin();it!=models.end();)if(it->second.expired())it=models.erase(it);else ++it;if(auto existing=models[key].lock())return existing;auto model=std::shared_ptr<SentenceFivegram>(new SentenceFivegram(std::make_shared<Data>(std::get<0>(key))));auto history=model->beginHistory();model->step(history,u"\x03");models[key]=model;return model;}
std::shared_ptr<const SentenceLanguageModel> SentenceFivegram::querySession() const{return std::shared_ptr<SentenceFivegram>(new SentenceFivegram(data_));}
SentenceLmHistory SentenceFivegram::beginHistory() const{return {{{data_->bos,0,0,0}},1};}
double SentenceFivegram::step(SentenceLmHistory& history,std::u16string_view target) const {if(history.count>4)throw std::invalid_argument("Invalid fivegram history");std::lock_guard<std::mutex> lock(cache_->mutex);const auto token=cache_->token(*data_,target);const std::array<std::uint32_t,6> key{history.tokens[0],history.tokens[1],history.tokens[2],history.tokens[3],history.count,token.id};auto found=cache_->scores.find(key);double score;if(found!=cache_->scores.end())score=found->second;else{score=data_->score(history,token.id);if(!std::isfinite(score))throw std::runtime_error("Non-finite fivegram score");if(cache_->scores.size()>=8192)cache_->scores.clear();cache_->scores.emplace(key,score);}for(std::size_t i=3;i>0;--i)history.tokens[i]=history.tokens[i-1];history.tokens[0]=token.id;history.count=std::min<std::uint32_t>(4,history.count+1);return score;}
double SentenceFivegram::logProbability(std::u16string_view,std::u16string_view,std::u16string_view,bool) const {throw std::logic_error("Fivegram scoring requires full history");}
bool SentenceFivegram::hasObservedBigram(std::u16string_view previous,std::u16string_view target) const {if(previous.empty()||target.empty())return false;std::lock_guard<std::mutex> lock(cache_->mutex);const auto a=cache_->token(*data_,previous),b=cache_->token(*data_,target);if(!a.found||!b.found)return false;const auto key=(static_cast<std::uint64_t>(a.id)<<16)|b.id;auto it=cache_->observed.find(key);if(it!=cache_->observed.end())return it->second;std::array<std::uint16_t,4> context{a.id,0,0,0};const bool value=data_->lookup(2,context,b.id).observed;if(cache_->observed.size()>=8192)cache_->observed.clear();cache_->observed.emplace(key,value);return value;}
std::uint64_t SentenceFivegram::mappedBytes() const{return data_->length;}
const void* SentenceFivegram::baseAddress() const{return data_->data;}
}
