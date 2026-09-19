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
// Immutable mapped Kneser-Ney reader. TCSKNM01 retains the strict legacy full
// validation. TCSKNM02 validates its compact header/unigram/sparse indexes at
// open and bounds-checks each variable-length context page while querying it.
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
    bool mobileFormat() const {return format_==Format::Mobile;}
private:
    SentenceNgram()=default;
    void map(const std::filesystem::path& path);
    void validate();
    void validateLegacy();
    void validateMobile();
    template<class T> T read(std::uint64_t offset) const;
    template<class T> T checkedRead(std::uint64_t offset) const;
    struct Index {std::uint64_t offset=0,count=0;bool wide=false;};
    std::uint64_t lowerBound(const Index& index,std::uint64_t key) const;
    float lookup(const Index& index,std::uint64_t key,float fallback) const;
    enum class Format {Legacy,Mobile};
    struct MobileSection {
        std::uint64_t contexts=0,blocks=0,index=0;
        std::uint32_t pages=0;
        std::uint64_t keyLimit=0;
    };
    struct MobileLookup {float backoff=1,probability=0;bool observed=false;};
    float mobileUnigram(std::uint32_t key,float fallback) const;
    MobileLookup mobileContext(const MobileSection&,std::uint64_t context,std::uint32_t target) const;
    std::array<Index,5> indices_{};
    Format format_=Format::Legacy;
    std::uint32_t mobileStride_=0,mobileUnigrams_=0;
    std::uint64_t mobileUnigramOffset_=0;
    MobileSection mobileBigram_{},mobileTrigram_{};
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
