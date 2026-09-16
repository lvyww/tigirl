#pragma once
#include "SentenceDecoder.h"
namespace tiger {
struct SentenceAutoCommitOptions {
    int requiredEvidence=3,requiredStrong=2,minimumRetained=3;
    bool countMergedTail=true;
};
struct SentenceAutoCommitInput {
    bool enabled=false,suspended=false,matchingLexicon=true;
    std::u16string_view raw,committedText;
    int committedRaw=0,lastAutoCommitRaw=0,configuredRetained=0;
};
struct SentencePrefixCommit {std::u16string text;int rawLength=0;};
// Copyable policy state for Engine previews. A returned decision consumes its
// evidence; the caller must apply its prefix/text bookkeeping atomically.
class SentenceAutoCommit {
public:
    explicit SentenceAutoCommit(SentenceAutoCommitOptions options={}):options_(options){}
    std::optional<SentencePrefixCommit> evaluate(const SentenceAutoCommitInput&,const SentenceDecodeResult&);
    void reset(){trackers_.clear();lastSeen_.clear();}
private:
    struct Tracker {std::u16string text;int rawLength=0,evidence=0,strong=0,gap=0;double share=0;};
    std::optional<SentencePrefixCommit> mature(const SentenceAutoCommitInput&,std::u16string_view,const std::u16string*);
    SentenceAutoCommitOptions options_;
    std::vector<Tracker> trackers_;
    std::u16string lastSeen_;
};
}
