#pragma once
#include "SentenceNgram.h"
namespace tiger {
// Native TCSKNM03 v2 Q8 query reader (version 1 is not supported). The model is file-mapped and shared; bounded
// mutable caches are private to each decoder's query session.
class SentenceFivegram final : public SentenceHistoryLanguageModel {
public:
    static std::shared_ptr<const SentenceFivegram> Open(const std::filesystem::path&);
    ~SentenceFivegram();
    std::shared_ptr<const SentenceLanguageModel> querySession() const override;
    SentenceLmHistory beginHistory() const override;
    double step(SentenceLmHistory&,std::u16string_view) const override;
    double logProbability(std::u16string_view,std::u16string_view,std::u16string_view,bool=true) const override;
    bool hasObservedBigram(std::u16string_view,std::u16string_view) const override;
    std::uint64_t mappedBytes() const override;
    const void* baseAddress() const override;
private:
    struct Data;
    struct Cache;
    explicit SentenceFivegram(std::shared_ptr<const Data>);
    std::shared_ptr<const Data> data_;
    std::unique_ptr<Cache> cache_;
};
}
