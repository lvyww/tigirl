#pragma once
#include "SentenceDecoder.h"
#include "SentenceAutoCommit.h"
#include <cstdint>
#include <optional>
#include <functional>
namespace tiger {
struct SentencePathQueries {
    std::function<bool(std::u16string_view,std::u16string_view,std::optional<std::u16string_view>,bool,const SentenceLockedPrefix*)> complete;
    std::function<bool(std::u16string_view)> properPrefix;
};
// Value-type composition state, safe to copy for TSF OnTestKeyDown. Scheduling,
// decoding and document edits are performed by the owning adapter, not here.
struct SentenceDecodeTicket {
    std::uint64_t session=0,generation=0,resources=0;
    std::u16string raw,requiredPrefix;
    std::shared_ptr<const SentenceLockedPrefix> lockedPrefix;
    int committedRaw=0;
};
class SentenceSession {
public:
    SentenceSession();
    void start(std::u16string raw,std::uint64_t resources,bool continuation=false);
    bool append(char16_t code);
    std::optional<std::u16string> appendAutomatic(char16_t code,bool enabled,int minimumRetained,
        const SentencePathQueries& queries);
    void resetAutomaticState();
    std::vector<SentenceLearningEvent> takeLearning();
    void resetEmptyCodePending();
    void backspace();
    void clear();
    void invalidatePending(bool discardResult=false);
    void changeResources(std::uint64_t resources);
    std::optional<SentenceDecodeTicket> request() const;
    // Duplicate completions do not reset a manual selection.
    bool apply(const SentenceDecodeTicket& ticket,SentenceDecodeResult result);
    void moveSelection(int delta,int pageSize,bool tab=false);
    std::optional<std::u16string> commitCandidate(int index);
    std::u16string commitWithSuffix(std::u16string_view suffix);
    std::u16string commitRaw(bool clearOnly);
    // Called only after the engine's evidence/retained-tail policy approves a
    // prefix. The raw context stays available to subsequent decoder requests.
    std::optional<std::u16string> commitPrefix(std::u16string_view text,int rawLength);
    // Evaluate current or immediately previous same-resource evidence and
    // atomically commit its approved boundary, retaining the full raw context.
    std::optional<std::u16string> tryAutoCommit(bool enabled,int minimumRetained=0);
    std::u16string displayCode() const;
    std::u16string liveRaw() const;
    std::u16string candidateText(int index) const;
    const SentenceDecodeResult& result() const {return *result_;}
    const std::u16string& raw() const {return raw_;}
    int selectedIndex() const {return selected_;}
    bool active() const {return !raw_.empty();}
    bool current() const;
    bool tabSelectionPending() const {return tabPending_;}
    bool autoCommitSuspended() const {return suspended_;}
private:
    void edited();
    void captureLearning(int index);
    void captureFusionLearning(int index);
    std::vector<SentenceLearningEvent> learningThrough(std::u16string_view text,int rawEnd);
    std::vector<SentenceLearningEvent> reinforceLearning(const SentenceCandidate& selected,int rawEnd) const;
    std::optional<SentenceCandidate> learningBaseline_;
    std::vector<SentenceLearningEvent> pendingLearning_,readyLearning_;
    std::u16string applyPrefix(std::u16string_view text,int rawLength);
    bool matches(const SentenceDecodeTicket& ticket) const;
    std::uint64_t identity_=0,generation_=0,resources_=0,appliedGeneration_=0,resultResources_=0;
    std::u16string raw_,committed_;
    int committedRaw_=0,selected_=0;
    int lastAutoCommitRaw_=0;
    SentenceAutoCommit autoCommit_;
    struct EmptyCodePending {
        std::u16string text,committed;
        int baseRawLength=0,lastSegmentStart=0;
        bool uniqueness=false;
    };
    std::optional<EmptyCodePending> emptyCodePending_;
    std::shared_ptr<const SentenceLockedPrefix> activeLock() const {return locks_.empty()?nullptr:locks_.back();}
    std::vector<std::shared_ptr<const SentenceLockedPrefix>> locks_;
    bool tabPending_=false;
    bool suspended_=false,continuation_=false,hasResult_=false;
    std::shared_ptr<const SentenceDecodeResult> result_=std::make_shared<SentenceDecodeResult>();
};
}
