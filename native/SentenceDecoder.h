#pragma once
#include "SentenceLexicon.h"
#include "SentenceLearning.h"
#include "SentenceNgram.h"
#include "MappedSentenceSupplement.h"
#include <memory>
#include <map>
#include <mutex>
#include <optional>
namespace tiger {
struct SentencePathBoundary {
    std::shared_ptr<const SentencePathBoundary> previous;
    int textLength=0,rawLength=0;
    double learningScore=0; // Optimal non-overlapping learning reward up to this boundary.
};
struct SentenceLockedPrefix {
    std::u16string rawCode,text;
    std::shared_ptr<const SentencePathBoundary> boundary;
};
struct SentenceCandidate {
    std::u16string text,segmentedCode;
    double baseScore=0,finalScore=0,confidenceScore=0,supplementScore=0;
    int maxLexiconRank=1;
    std::shared_ptr<const SentencePathBoundary> boundary;
    // Eligibility from the producing decoder's settings, retained with its snapshot.
    bool eligibleDuplicateSinglePath=false;
    double learningScore=0;
};
struct SentencePrefixEvidence {
    std::u16string text;
    int rawLength=0;
    double share=0,boundaryShare=0;
    bool boundaryClosed=false;
};
struct SentenceEarlyCommitEvidence {
    std::vector<SentencePrefixEvidence> prefixes;
    bool neutralIncompleteTail=false,mergedIncompleteTail=false,neutralLowConfidence=false,confidenceTruncated=false;
    std::u16string proposal;
    double proposalShare=0;
    std::map<std::u16string,int> rawLengths;
};
struct SentenceDecodeResult {
    std::u16string rawCode;
    std::vector<SentenceCandidate> candidates;
    int expandedStates=0;
    bool learningAffected=false;
    std::u16string learningMode;
    SentenceEarlyCommitEvidence earlyCommitEvidence;
};
struct SentenceDecoderOptions {
    int beamWidth=2000;
    double rankPenalty=0.03;
    int isolationRankThreshold=3000;
    double isolationLambda=2;
    bool isolationUseLogRank=false;
    bool scoreSentenceBoundaries=true;
    double emittedCharacterReward=0,wholeInputSingleCharacterReward=0;
    bool allowDuplicateSingleCharacters=false;
};
// Decoder owns composition-local lattice state. Large data resources remain
// shared immutable views. Engine commit policy is a separate integration step.
class SentenceDecoder {
public:
    SentenceDecoder(std::shared_ptr<const SentenceLexicon> lexicon,
        std::shared_ptr<const SentenceLanguageModel> model={},SentenceDecoderOptions options={},
        std::shared_ptr<const MappedSentenceSupplement> supplement={});
    ~SentenceDecoder();
    SentenceDecoder(const SentenceDecoder&)=delete;
    SentenceDecoder& operator=(const SentenceDecoder&)=delete;
    SentenceDecodeResult decode(std::u16string_view raw,int candidateLimit=20,
        bool includeEarlyCommitEvidence=false,std::u16string_view requiredTextPrefix={},
        std::shared_ptr<const SentenceLockedPrefix> lockedPrefix={});
    void resetDecodeCache();
    void setLearning(std::shared_ptr<const SentenceLearningSnapshot> snapshot,std::u16string mode);
    bool hasCompleteCandidate(std::u16string_view raw,std::u16string_view requiredTextPrefix={},
        std::optional<std::u16string_view> excludedText={},bool groupEligibleOnly=false,const SentenceLockedPrefix* lockedPrefix=nullptr) const;
    bool isProperCodePrefix(std::u16string_view raw) const;
    SentenceDecodeResult decodeFull(std::u16string_view raw,int candidateLimit=20,
        bool includeEarlyCommitEvidence=false,std::u16string_view requiredTextPrefix={}) const;
    static std::u16string normalizeRawCode(std::u16string_view raw);
private:
    struct Lattice;
    struct Cache;
    int expand(std::u16string_view raw,Lattice& lattice,int from,int minimumEnd=-1) const;
    SentenceDecodeResult emit(std::u16string_view raw,Lattice& lattice,int limit,int expanded,
        bool evidence,std::u16string_view required) const;
    std::shared_ptr<const SentenceLearningSnapshot> learning_;
    std::u16string learningMode_;
    std::unique_ptr<Cache> cache_;
    mutable std::mutex decodeMutex_;
    double transition(Lattice& lattice,std::u16string_view previous2,std::u16string_view previous1,std::u16string_view target) const;
    double isolation(std::u16string_view text) const;
    std::shared_ptr<const SentenceLexicon> lexicon_;
    std::shared_ptr<const SentenceLanguageModel> model_;
    const SentenceNgram* ngram_=nullptr;
    SentenceDecoderOptions options_;
    std::shared_ptr<const MappedSentenceSupplement> supplement_;
};
}
