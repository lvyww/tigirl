#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
namespace tiger {
class SentenceLanguageModel {
public:
    virtual ~SentenceLanguageModel()=default;
    virtual double logProbability(std::u16string_view previous2,std::u16string_view previous1,
        std::u16string_view target,bool includeUnigram=true) const=0;
    virtual bool hasObservedBigram(std::u16string_view previous,std::u16string_view target) const=0;
};
// Immutable view of the upstream TCSKNM01 model. No whole-model heap copy or
// mutable shared query cache; decoder-local caches can be bounded separately.
class SentenceNgram final:public SentenceLanguageModel {
public:
    static std::shared_ptr<const SentenceNgram> Open(const std::filesystem::path& path);
    ~SentenceNgram();
    SentenceNgram(const SentenceNgram&)=delete;
    SentenceNgram& operator=(const SentenceNgram&)=delete;
    double logProbability(std::u16string_view previous2,std::u16string_view previous1,
        std::u16string_view target,bool includeUnigram=true) const override;
    bool hasObservedBigram(std::u16string_view previous,std::u16string_view target) const override;
    std::uint64_t mappedBytes() const {return length_;}
    const void* baseAddress() const {return data_;}
private:
    SentenceNgram()=default;
    void map(const std::filesystem::path& path);
    void validate();
    template<class T> T read(std::uint64_t offset) const;
    struct Index {std::uint64_t offset=0,count=0;bool wide=false;};
    std::uint64_t lowerBound(const Index& index,std::uint64_t key) const;
    float lookup(const Index& index,std::uint64_t key,float fallback) const;
    std::array<Index,5> indices_{};
    const unsigned char* data_=nullptr;
    std::uint64_t length_=0;
    float unknown_=0;
#ifdef _WIN32
    void* file_=reinterpret_cast<void*>(static_cast<std::intptr_t>(-1));
    void* mapping_=nullptr;
#else
    int file_=-1;
#endif
};
}
