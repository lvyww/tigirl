#include "SentenceResources.h"
#include "SentenceCharacterRanks.h"
#include "SentenceImport.h"
#include <stdexcept>
namespace tiger {
std::shared_ptr<const SentenceResources> SentenceResources::Open(const std::filesystem::path& ordinary,
    const std::filesystem::path& model,int commonLimit,SentenceLexicon::Characters whitelist,SentenceDecoderOptions options,
    const std::filesystem::path& sentenceOverride,std::u16string_view expectedRevision) {
    // Opening the ordinary generation also detects a dangling sidecar set.
    auto dictionary=Dictionary::Open(ordinary);
    auto result=std::shared_ptr<SentenceResources>(new SentenceResources);
    if(sentenceOverride.empty()!=expectedRevision.empty())throw std::invalid_argument("Sentence override requires its expected revision");
    auto sentence=Dictionary::Open(sentenceOverride.empty()?sentenceLexiconPath(ordinary):sentenceOverride);
    if(!sentenceOverride.empty() && sentence->value(sentence->find(Section::Split,u"_sentence_revision"),0)!=expectedRevision)
        throw std::runtime_error("Sentence resource revision mismatch");
    result->lexicon_=std::make_shared<SentenceLexicon>(std::move(sentence),sentenceTopCharacters(commonLimit),std::move(whitelist));
    result->supplement_=std::make_shared<MappedSentenceSupplement>(Dictionary::Open(sentenceSupplementPath(ordinary)));
    result->model_=SentenceNgram::Open(model);result->options_=options;return result;
}
std::unique_ptr<SentenceDecoder> SentenceResources::createDecoder() const {
    return std::make_unique<SentenceDecoder>(lexicon_,model_,options_,supplement_);
}
}
