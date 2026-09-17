#include "SentenceNgram.h"
#include "FileCachePath.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
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
[[noreturn]] void invalid(){throw std::runtime_error("Invalid TigerClaw sentence n-gram model");}
std::uint32_t scalar(std::u16string_view token) {
    if(token.size()==1)return token[0];
    if(token.size()==2 && token[0]>=0xd800 && token[0]<=0xdbff && token[1]>=0xdc00 && token[1]<=0xdfff)
        return 0x10000+((token[0]-0xd800)<<10)+(token[1]-0xdc00);
    return 0;
}
std::uint64_t pair(std::uint32_t a,std::uint32_t b){return (static_cast<std::uint64_t>(a)<<21)|(b&0x1fffff);}
std::uint64_t triple(std::uint32_t a,std::uint32_t b,std::uint32_t c){return (static_cast<std::uint64_t>(a)<<42)|(static_cast<std::uint64_t>(b)<<21)|(c&0x1fffff);}
}
std::shared_ptr<const SentenceNgram> SentenceNgram::Open(const std::filesystem::path& path) {
    static std::mutex mutex;static std::map<std::filesystem::path,std::weak_ptr<const SentenceNgram>> cache;
    const auto canonical=fileCachePath(path);
    std::lock_guard<std::mutex> lock(mutex);
    if(auto live=cache[canonical].lock())return live;
    auto model=std::shared_ptr<SentenceNgram>(new SentenceNgram);
    model->map(canonical);model->validate();
    for(auto it=cache.begin();it!=cache.end();)if(it->second.expired())it=cache.erase(it);else ++it;
    cache[canonical]=model;return model;
}
void SentenceNgram::map(const std::filesystem::path& path) {
#ifdef _WIN32
    file_=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file_==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open sentence model");
    LARGE_INTEGER size{};if(!GetFileSizeEx(file_,&size) || size.QuadPart<=0)invalid();length_=static_cast<std::uint64_t>(size.QuadPart);
#else
    file_=::open(path.c_str(),O_RDONLY|O_CLOEXEC);if(file_<0)throw std::runtime_error("Cannot open sentence model");
    struct stat info{};if(fstat(file_,&info) || info.st_size<=0)invalid();length_=static_cast<std::uint64_t>(info.st_size);
#endif
    if(length_>(std::uint64_t{4}<<30) || length_>std::numeric_limits<std::size_t>::max())invalid();
#ifdef _WIN32
    mapping_=CreateFileMappingW(file_,nullptr,PAGE_READONLY,0,0,nullptr);
    if(!mapping_)throw std::runtime_error("Cannot create sentence model mapping");
    data_=static_cast<const unsigned char*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,0));
    if(!data_)throw std::runtime_error("Cannot map sentence model");
#else
    void* view=mmap(nullptr,static_cast<std::size_t>(length_),PROT_READ,MAP_SHARED,file_,0);
    if(view==MAP_FAILED)throw std::runtime_error("Cannot map sentence model");data_=static_cast<const unsigned char*>(view);
#endif
}
SentenceNgram::~SentenceNgram(){
#ifdef _WIN32
    if(data_)UnmapViewOfFile(data_);if(mapping_)CloseHandle(mapping_);if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);
#else
    if(data_)munmap(const_cast<unsigned char*>(data_),static_cast<std::size_t>(length_));if(file_>=0)close(file_);
#endif
}
template<class T>T SentenceNgram::read(std::uint64_t offset) const {T value;std::memcpy(&value,data_+offset,sizeof(T));return value;}
void SentenceNgram::validate() {
    const std::uint32_t endian=1;if(*reinterpret_cast<const unsigned char*>(&endian)!=1)invalid();
    if(length_<16 || std::memcmp(data_,"TCSKNM01",8) || read<std::uint32_t>(8)!=1)invalid();
    std::uint64_t position=12;
    for(std::size_t i=0;i<indices_.size();++i) {
        auto& index=indices_[i];index.wide=i==1 || i==3 || i==4;
        const std::uint64_t countBytes=index.wide?8:4;
        if(position>length_ || countBytes>length_-position)invalid();
        index.count=index.wide?read<std::uint64_t>(position):read<std::uint32_t>(position);
        position+=countBytes;index.offset=position;
        const std::uint64_t stride=index.wide?12:8;
        if(index.count>(length_-position)/stride)invalid();position+=index.count*stride;
    }
    if(position!=length_ || !indices_[0].count)invalid();
    for(const auto& index:indices_) {
        std::uint64_t previous=0;const std::uint64_t stride=index.wide?12:8;
        for(std::uint64_t n=0;n<index.count;++n) {
            const auto offset=index.offset+n*stride;
            const auto key=index.wide?read<std::uint64_t>(offset):read<std::uint32_t>(offset);
            const auto score=read<float>(offset+(index.wide?8:4));
            // Binary search requires strict ordering; NaN/Inf or negative
            // probabilities/backoff weights would also poison decoder ordering.
            if((n && key<=previous) || !std::isfinite(score) || score<0)invalid();
            previous=key;
        }
    }
    unknown_=lookup(indices_[0],0,0);
    if(!std::isfinite(unknown_) || unknown_<=0 || unknown_>1)invalid();
}
std::uint64_t SentenceNgram::lowerBound(const Index& index,std::uint64_t key) const {
    std::uint64_t low=0,high=index.count;const std::uint64_t stride=index.wide?12:8;
    while(low<high){const auto mid=low+(high-low)/2;const auto offset=index.offset+mid*stride;
        const auto value=index.wide?read<std::uint64_t>(offset):read<std::uint32_t>(offset);
        if(value<key)low=mid+1;else high=mid;}
    return low;
}
float SentenceNgram::lookup(const Index& index,std::uint64_t key,float fallback) const {
    const auto n=lowerBound(index,key);if(n==index.count)return fallback;
    const auto offset=index.offset+n*(index.wide?12:8);
    const auto value=index.wide?read<std::uint64_t>(offset):read<std::uint32_t>(offset);
    return value==key?read<float>(offset+(index.wide?8:4)):fallback;
}
double SentenceNgram::logProbability(std::u16string_view previous2,std::u16string_view previous1,std::u16string_view target,bool includeUnigram) const {
    const auto a=scalar(previous2),b=scalar(previous1),c=scalar(target);
    const double unigram=includeUnigram?lookup(indices_[0],c,unknown_):0;
    double bigram=lookup(indices_[1],pair(b,c),0);
    const double bigramLambda=lookup(indices_[2],b,1);
    bigram+=bigramLambda*unigram;
    double trigram=lookup(indices_[3],triple(a,b,c),0);
    const double trigramLambda=lookup(indices_[4],pair(a,b),1);
    trigram+=trigramLambda*bigram;
    return std::log(std::max(trigram,1e-300));
}
bool SentenceNgram::hasObservedBigram(std::u16string_view previous,std::u16string_view target) const {
    const auto& index=indices_[1];const auto key=pair(scalar(previous),scalar(target));
    const auto n=lowerBound(index,key);return n<index.count && read<std::uint64_t>(index.offset+n*12)==key;
}
}
