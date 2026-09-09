#pragma once
#include "SentenceDecoder.h"
namespace tiger {
// Loaded on a resource worker, never on the TSF key path. A resource generation
// owns immutable mappings; each composition obtains its own bounded decoder.
class SentenceResources {
public:
    // Override files are adopted only with their matching snapshot revision.
    // The caller computes that revision from its own current base/user snapshot.
    static std::shared_ptr<const SentenceResources> Open(const std::filesystem::path& ordinary,
        const std::filesystem::path& model,int commonLimit,SentenceLexicon::Characters whitelist,
        SentenceDecoderOptions options,const std::filesystem::path& sentenceOverride={},
        std::u16string_view expectedRevision={});
    std::unique_ptr<SentenceDecoder> createDecoder() const;
    const std::shared_ptr<const SentenceLexicon>& lexicon() const{return lexicon_;}
    const std::shared_ptr<const SentenceNgram>& model() const{return model_;}
private:
    std::shared_ptr<const SentenceLexicon> lexicon_;
    std::shared_ptr<const SentenceNgram> model_;
    std::shared_ptr<const MappedSentenceSupplement> supplement_;
    SentenceDecoderOptions options_;
};
}
