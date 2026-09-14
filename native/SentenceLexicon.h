#pragma once
#include "Dictionary.h"
#include "LexiconImport.h"
#include <set>
#include <vector>
#include <mutex>
#include <deque>
#include <map>
namespace tiger {
// Import-time preparation from source encounter order, before ordinary TCD
// serialization sorts keys. Input candidates already contain commit text.
ImportedLexicon prepareSentenceLexicon(const std::vector<ImportedLexiconEntry>& source);
struct SentenceLexiconCandidate {
    std::u16string_view text;
    std::uint32_t rank=0;
    double logRank=0;
    std::vector<std::u16string> textElements;
    bool optimalSingleCharacterCode=false;
};
// The backing file reuses the validated six-section TCD container, with a
// sentence-format marker and sentence-specific metadata section meanings.
class SentenceLexicon {
public:
    using Characters=std::set<std::u16string,std::less<>>;
    SentenceLexicon(std::shared_ptr<const Dictionary> dictionary,Characters common={},Characters whitelist={});
    std::vector<SentenceLexiconCandidate> candidates(std::u16string_view code) const;
    // Immutable shared metadata; bounded to 256 codes / 1 MiB estimated payload.
    // Oversized entries bypass caching. Public value-returning API stays compatible.
    std::shared_ptr<const std::vector<SentenceLexiconCandidate>> candidateView(std::u16string_view code) const;
    bool isProperCodePrefix(std::u16string_view code) const;
    const std::vector<int>& codeLengths() const {return codeLengths_;}
    // Mapped source inventory, including empty exact keys, for rebuilding after
    // user edits. No per-host copy of the complete ordering is needed.
    std::uint32_t sourceCodeCount() const { return dictionary_->count(Section::Comment); }
    std::u16string_view sourceCode(std::uint32_t index) const;
    const std::shared_ptr<const Dictionary>& dictionary() const {return dictionary_;}
private:
    bool allowed(std::u16string_view code,std::u16string_view text) const;
    bool hasAllowed(const Dictionary::Entry& entry) const;
    std::shared_ptr<const Dictionary> dictionary_;
    Characters common_,whitelist_;
    std::vector<int> codeLengths_;
    struct CachedCandidates {
        std::shared_ptr<const std::vector<SentenceLexiconCandidate>> values;
        std::size_t bytes=0;
    };
    mutable std::mutex metadataMutex_;
    mutable std::map<std::u16string,CachedCandidates,std::less<>> metadata_;
    mutable std::deque<std::u16string> metadataOrder_;
    mutable std::size_t metadataBytes_=0;
    mutable std::map<std::u16string,bool,std::less<>> prefixes_;
    mutable std::deque<std::u16string> prefixOrder_;
    bool properPrefixUncached(std::u16string_view code) const;
};
}
