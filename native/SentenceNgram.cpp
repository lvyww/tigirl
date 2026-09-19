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
constexpr std::uint64_t mobileHeader=104;
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
template<class T>T SentenceNgram::checkedRead(std::uint64_t offset) const {
    if(offset>length_ || sizeof(T)>length_-offset)invalid();
    return read<T>(offset);
}
void SentenceNgram::validate() {
    const std::uint32_t endian=1;if(*reinterpret_cast<const unsigned char*>(&endian)!=1)invalid();
    if(length_<12)invalid();
    if(!std::memcmp(data_,"TCSKNM01",8)){format_=Format::Legacy;validateLegacy();return;}
    if(!std::memcmp(data_,"TCSKNM02",8)){format_=Format::Mobile;validateMobile();return;}
    invalid();
}
void SentenceNgram::validateLegacy() {
    if(length_<16 || read<std::uint32_t>(8)!=1)invalid();
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
            if((n && key<=previous) || !std::isfinite(score) || score<0)invalid();
            previous=key;
        }
    }
    unknown_=lookup(indices_[0],0,0);
    if(!std::isfinite(unknown_) || unknown_<=0 || unknown_>1)invalid();
}
void SentenceNgram::validateMobile() {
    if(length_<mobileHeader || read<std::uint32_t>(8)!=1 || read<std::uint32_t>(12)!=mobileHeader ||
       read<std::uint64_t>(16)!=length_)invalid();
    mobileStride_=read<std::uint32_t>(24);const auto reserved0=read<std::uint32_t>(28);
    mobileUnigrams_=read<std::uint32_t>(32);const auto reserved1=read<std::uint32_t>(36);
    mobileUnigramOffset_=read<std::uint64_t>(40);
    mobileBigram_.contexts=read<std::uint32_t>(48);mobileBigram_.pages=read<std::uint32_t>(52);
    mobileBigram_.blocks=read<std::uint64_t>(56);mobileBigram_.index=read<std::uint64_t>(64);
    mobileBigram_.keyLimit=std::uint64_t{1}<<21;
    mobileTrigram_.contexts=read<std::uint64_t>(72);mobileTrigram_.pages=read<std::uint32_t>(80);
    const auto reserved2=read<std::uint32_t>(84);
    mobileTrigram_.blocks=read<std::uint64_t>(88);mobileTrigram_.index=read<std::uint64_t>(96);
    mobileTrigram_.keyLimit=std::uint64_t{1}<<42;
    if(mobileStride_<16 || mobileStride_>65536 || reserved0 || reserved1 || reserved2 ||
       !mobileUnigrams_ || mobileUnigramOffset_!=mobileHeader)invalid();
    const auto unigramEnd=mobileUnigramOffset_+static_cast<std::uint64_t>(mobileUnigrams_)*8;
    if(unigramEnd<mobileUnigramOffset_ || mobileBigram_.blocks!=unigramEnd ||
       mobileBigram_.blocks>=mobileBigram_.index || mobileBigram_.index>length_ ||
       mobileTrigram_.blocks!=mobileBigram_.index+static_cast<std::uint64_t>(mobileBigram_.pages)*16 ||
       mobileTrigram_.blocks>=mobileTrigram_.index || mobileTrigram_.index>length_ ||
       length_!=mobileTrigram_.index+static_cast<std::uint64_t>(mobileTrigram_.pages)*16)invalid();
    const auto pages=[&](std::uint64_t contexts){return (contexts+mobileStride_-1)/mobileStride_;};
    if(pages(mobileBigram_.contexts)!=mobileBigram_.pages || pages(mobileTrigram_.contexts)!=mobileTrigram_.pages)invalid();
    std::uint32_t previous=0;
    for(std::uint32_t n=0;n<mobileUnigrams_;++n) {
        const auto offset=mobileUnigramOffset_+static_cast<std::uint64_t>(n)*8;
        const auto key=read<std::uint32_t>(offset);const auto score=read<float>(offset+4);
        if((n && key<=previous) || key>=0x200000 || !std::isfinite(score) || score<0)invalid();
        previous=key;
    }
    if(read<std::uint32_t>(mobileUnigramOffset_)!=0)invalid();
    unknown_=read<float>(mobileUnigramOffset_+4);
    if(!std::isfinite(unknown_) || unknown_<=0 || unknown_>1)invalid();
    const auto validateIndex=[&](const MobileSection& section) {
        std::uint64_t previousKey=0,previousOffset=0;
        for(std::uint32_t page=0;page<section.pages;++page) {
            const auto at=section.index+static_cast<std::uint64_t>(page)*16;
            const auto key=checkedRead<std::uint64_t>(at),offset=checkedRead<std::uint64_t>(at+8);
            if((page && (key<=previousKey || offset<=previousOffset)) || key>=section.keyLimit ||
               offset<section.blocks || offset>=section.index)invalid();
            if(!page && offset!=section.blocks)invalid();
            previousKey=key;previousOffset=offset;
        }
    };
    validateIndex(mobileBigram_);validateIndex(mobileTrigram_);
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
float SentenceNgram::mobileUnigram(std::uint32_t key,float fallback) const {
    std::uint32_t low=0,high=mobileUnigrams_;
    while(low<high){const auto mid=low+(high-low)/2;
        const auto at=mobileUnigramOffset_+static_cast<std::uint64_t>(mid)*8;
        if(read<std::uint32_t>(at)<key)low=mid+1;else high=mid;}
    if(low==mobileUnigrams_)return fallback;
    const auto at=mobileUnigramOffset_+static_cast<std::uint64_t>(low)*8;
    return read<std::uint32_t>(at)==key?read<float>(at+4):fallback;
}
SentenceNgram::MobileLookup SentenceNgram::mobileContext(const MobileSection& section,
    std::uint64_t context,std::uint32_t target) const {
    if(!section.pages || context>=section.keyLimit || target>=0x200000)return {};
    std::uint32_t low=0,high=section.pages;
    while(low<high){const auto mid=low+(high-low)/2;
        const auto key=checkedRead<std::uint64_t>(section.index+static_cast<std::uint64_t>(mid)*16);
        if(key<=context)low=mid+1;else high=mid;}
    if(!low)return {};
    const auto page=low-1;
    const auto indexAt=section.index+static_cast<std::uint64_t>(page)*16;
    std::uint64_t position=checkedRead<std::uint64_t>(indexAt+8);
    const std::uint64_t pageEnd=page+1<section.pages?checkedRead<std::uint64_t>(indexAt+24):section.index;
    if(position<section.blocks || position>=pageEnd || pageEnd>section.index)invalid();
    const auto first=static_cast<std::uint64_t>(page)*mobileStride_;
    const auto remaining=static_cast<std::uint32_t>(std::min<std::uint64_t>(mobileStride_,section.contexts-first));
    std::uint64_t previous=0;
    for(std::uint32_t i=0;i<remaining;++i) {
        if(position>pageEnd || 16>pageEnd-position)invalid();
        const auto key=read<std::uint64_t>(position);
        const auto backoff=read<float>(position+8);
        const auto successors=read<std::uint32_t>(position+12);
        if((i && key<=previous) || key>=section.keyLimit || !std::isfinite(backoff) || backoff<0)invalid();
        previous=key;position+=16;
        const auto successorBytes=static_cast<std::uint64_t>(successors)*8;
        if(successorBytes>pageEnd-position)invalid();
        if(key==context) {
            std::uint32_t left=0,right=successors;
            while(left<right){const auto mid=left+(right-left)/2;
                const auto at=position+static_cast<std::uint64_t>(mid)*8;
                if(read<std::uint32_t>(at)<target)left=mid+1;else right=mid;}
            if(left<successors) {
                const auto at=position+static_cast<std::uint64_t>(left)*8;
                if(read<std::uint32_t>(at)==target) {
                    const auto probability=read<float>(at+4);
                    if(!std::isfinite(probability) || probability<0)invalid();
                    return {backoff,probability,true};
                }
            }
            return {backoff,0,false};
        }
        if(key>context)return {};
        position+=successorBytes;
    }
    return {};
}
double SentenceNgram::logProbability(std::u16string_view previous2,std::u16string_view previous1,
    std::u16string_view target,bool includeUnigram) const {
    const auto a=scalar(previous2),b=scalar(previous1),c=scalar(target);
    if(format_==Format::Mobile) {
        const double unigram=includeUnigram?mobileUnigram(c,unknown_):0;
        const auto big=mobileContext(mobileBigram_,b,c);
        const double bigram=big.probability+big.backoff*unigram;
        const auto tri=mobileContext(mobileTrigram_,pair(a,b),c);
        const double trigram=tri.probability+tri.backoff*bigram;
        return std::log(std::max(trigram,1e-300));
    }
    const double unigram=includeUnigram?lookup(indices_[0],c,unknown_):0;
    double bigram=lookup(indices_[1],pair(b,c),0);
    const double bigramLambda=lookup(indices_[2],b,1);bigram+=bigramLambda*unigram;
    double trigram=lookup(indices_[3],triple(a,b,c),0);
    const double trigramLambda=lookup(indices_[4],pair(a,b),1);trigram+=trigramLambda*bigram;
    return std::log(std::max(trigram,1e-300));
}
bool SentenceNgram::hasObservedBigram(std::u16string_view previous,std::u16string_view target) const {
    const auto a=scalar(previous),b=scalar(target);
    if(format_==Format::Mobile)return mobileContext(mobileBigram_,a,b).observed;
    const auto& index=indices_[1];const auto key=pair(a,b);const auto n=lowerBound(index,key);
    return n<index.count && read<std::uint64_t>(index.offset+n*12)==key;
}
}
