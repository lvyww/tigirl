#pragma once
#include "SentenceLexicon.h"
#include "SentenceLearning.h"
#include "SentenceNgram.h"
#include "SentenceLexicalPrior.h"
#include "MappedSentenceSupplement.h"
#include <memory>
#include <map>
#include <mutex>
#include <optional>
#include <atomic>
#include <exception>
#include <limits>
namespace tiger {
enum SentenceCandidateSource : unsigned {
    SentenceSourceNone=0,
    SentenceSourceDirect=1,
    SentenceSourceComposed=2
};
struct SentencePathBoundary {
    std::shared_ptr<const SentencePathBoundary> previous;
    int textLength=0,rawLength=0;
    double learningScore=0; // Optimal non-overlapping learning reward up to this boundary.
    double codeScore=0; // Cumulative final-ranking-only shape evidence.
    bool protectsRareCharacter=false;
    int codeLength=0;
    // When this boundary closes a learnt span, retain its exact origin so a
    // later explicit top1 commit can reinforce that preference without
    // guessing text/code segmentation.
    double learningReward=0;
    int learningRawStart=0,learningTextStart=0;
    std::uint64_t textHash=0; // Internal rolling UTF-16 hash; ranking never depends on collisions.
};
struct SentenceLockedPrefix {
    std::u16string rawCode,text;
    std::shared_ptr<const SentencePathBoundary> boundary;
};
struct SentenceCandidate {
    std::u16string text,segmentedCode;
    double baseScore=0,finalScore=0,confidenceScore=0;
    double earlyCommitConfidenceScore=std::numeric_limits<double>::quiet_NaN();
    double supplementScore=0,codeScore=0,lexicalScore=0;
    int maxLexiconRank=1;
    unsigned source=SentenceSourceNone;
    int directRank=std::numeric_limits<int>::max();
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
    // Appended for source compatibility with existing aggregate fixtures.
    double baseShare=std::numeric_limits<double>::quiet_NaN();
};
struct SentenceEarlyCommitEvidence {
    std::vector<SentencePrefixEvidence> prefixes;
    bool neutralIncompleteTail=false,mergedIncompleteTail=false,neutralLowConfidence=false,confidenceTruncated=false;
    std::u16string proposal;
    double proposalShare=0;
    std::map<std::u16string,int> rawLengths;
};
struct SentenceDecodeCancelled : std::exception {
    const char* what() const noexcept override {return "Sentence decode superseded";}
};
struct SentenceDecoderMemory {
    std::size_t positions=0,states=0,stateCapacity=0,stateBytes=0;
    std::size_t boundaryNodes=0,boundaryCapacity=0,boundaryBytes=0,publishedBoundaries=0;
};
struct SentenceDecodeResult {
    std::u16string rawCode;
    std::vector<SentenceCandidate> candidates;
    // Menu truncation must never manufacture confidence. This immutable pool
    // contains all retained completed candidates (before the display limit).
    std::shared_ptr<const std::vector<SentenceCandidate>> confidenceCandidates;
    const std::vector<SentenceCandidate>& confidencePool() const {
        return confidenceCandidates?*confidenceCandidates:candidates;
    }
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
    double canonicalCodeReward=0,canonicalIsolationFactor=1,lexicalPriorWeight=0;
    int canonicalIsolationMinCodeLength=4,lexicalCandidateLimit=5;
    bool allowDuplicateSingleCharacters=false;
    bool preserveTruncatedEarlyCommitEvidence=false;
};
// Decoder owns composition-local lattice state. Large data resources remain
// shared immutable views. Engine commit policy is a separate integration step.
class SentenceDecoder {
public:
    SentenceDecoder(std::shared_ptr<const SentenceLexicon> lexicon,
        std::shared_ptr<const SentenceLanguageModel> model={},SentenceDecoderOptions options={},
        std::shared_ptr<const MappedSentenceSupplement> supplement={},
        std::shared_ptr<const SentenceLexicalPrior> lexicalPrior={});
    ~SentenceDecoder();
    SentenceDecoder(const SentenceDecoder&)=delete;
    SentenceDecoder& operator=(const SentenceDecoder&)=delete;
    SentenceDecodeResult decode(std::u16string_view raw,int candidateLimit=20,
        bool includeEarlyCommitEvidence=false,std::u16string_view requiredTextPrefix={},
        std::shared_ptr<const SentenceLockedPrefix> lockedPrefix={},
        std::shared_ptr<const std::atomic<bool>> cancellation={});
    void resetDecodeCache();
    // Drop only old position buckets, not competing frontier states, their
    // scores/text/boundaries, or committed raw coordinates. Earlier edits rebuild.
    void retainCommittedHistory(std::u16string_view raw,int committedRaw);
    SentenceDecoderMemory memoryStatus() const;
    void setLearning(std::shared_ptr<const SentenceLearningSnapshot> snapshot,std::u16string mode);
    void applyFusionOrdering(std::u16string_view raw,std::vector<SentenceCandidate>& candidates) const;
    bool hasCompleteCandidate(std::u16string_view raw,std::u16string_view requiredTextPrefix={},
        std::optional<std::u16string_view> excludedText={},bool groupEligibleOnly=false,const SentenceLockedPrefix* lockedPrefix=nullptr) const;
    bool isProperCodePrefix(std::u16string_view raw) const;
    SentenceDecodeResult decodeFull(std::u16string_view raw,int candidateLimit=20,
        bool includeEarlyCommitEvidence=false,std::u16string_view requiredTextPrefix={}) const;
    static std::u16string normalizeRawCode(std::u16string_view raw);
private:
    struct Lattice;
    struct Cache;
    void checkCancelled() const;
    const std::atomic<bool>* cancellation_=nullptr; // guarded by decodeMutex_
    int expand(std::u16string_view raw,Lattice& lattice,int from,int minimumEnd=-1) const;
    SentenceDecodeResult emit(std::u16string_view raw,Lattice& lattice,int limit,int expanded,
        bool evidence,std::u16string_view required) const;
    std::shared_ptr<const SentenceLearningSnapshot> learning_;
    std::u16string learningMode_;
    std::unique_ptr<Cache> cache_;
    mutable std::mutex decodeMutex_;
    double transition(Lattice& lattice,std::u16string_view previous2,std::u16string_view previous1,std::u16string_view target) const;
    bool observed(Lattice& lattice,std::u16string_view previous,std::u16string_view target) const;
    double isolation(Lattice& lattice,std::u16string_view text) const;
    double pathIsolation(Lattice& lattice,std::u16string_view text,int boundary) const;
    std::shared_ptr<const SentenceLexicon> lexicon_;
    std::shared_ptr<const SentenceLanguageModel> model_;
    const SentenceNgram* ngram_=nullptr;
    SentenceDecoderOptions options_;
    std::shared_ptr<const MappedSentenceSupplement> supplement_;
    std::shared_ptr<const SentenceLexicalPrior> lexicalPrior_;
};
}
