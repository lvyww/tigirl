#pragma once
#include "Dictionary.h"
#include <map>
#include <string>
#include <vector>

namespace tiger {
enum class ChangeKind { Add, Delete, Top, Advance };
struct UserChange {
    ChangeKind kind;
    std::u16string code;
    // Packed display/commit entry, not a UI label and not backslash escaped.
    std::u16string text;
};

// Immutable view of a mapped base plus only the codes affected by user edits.
// Copying Engine or retaining an old composition keeps this snapshot stable.
class Lexicon final {
public:
    explicit Lexicon(std::shared_ptr<const Dictionary> dictionary);
    struct Match {
        std::uint32_t count = 0;
        std::uint32_t flags = 0;
        bool unique() const { return (flags & 1) != 0; }
        bool prefix() const { return (flags & 2) != 0; }
        bool autoSymbol() const { return (flags & 4) != 0; }
    private:
        friend class Lexicon;
        Dictionary::Entry entry;
        const std::vector<std::u16string>* edited = nullptr;
    };
    Match find(Section section, std::u16string_view key) const;
    std::u16string_view value(const Match& match, std::uint32_t index) const;
    std::shared_ptr<const Lexicon> changed(const UserChange& change, bool* didChange = nullptr) const;
    std::uint32_t quickSymbols() const;
    std::size_t editedCodes() const { return edits_.size(); }
    // Exact codes introduced by user operations, in first-insertion order.
    // Empty codes retain their place after deleting their last candidate.
    const std::vector<std::u16string>& addedCodes() const { return addedCodes_; }
    // Visit only the small user overlay, in deterministic key order. References
    // remain owned by this immutable snapshot; no base-table expansion occurs.
    template<class Visitor> void visitUserEdits(Visitor visitor) const {
        for(const auto& edit:edits_)visitor(std::u16string_view(edit.first),*edit.second);
    }
    bool equivalent(const Lexicon& other) const;
    const std::shared_ptr<const Dictionary>& dictionary() const { return dictionary_; }

private:
    friend class UserStore;
    // UserStore alone may build a fresh, unpublished view in place. Published
    // snapshots and Engine previews continue to use the immutable changed API.
    bool applyChange(const UserChange& change);
    using Values = std::vector<std::u16string>;
    using Edits = std::map<std::u16string, std::shared_ptr<const Values>, std::less<>>;
    bool hasLonger(std::u16string_view code, bool includeEmpty) const;
    std::uint32_t lowerBound(std::u16string_view code) const;
    void rebuildQuickSymbols(std::u16string_view addedKey);
    std::shared_ptr<const Dictionary> dictionary_;
    Edits edits_;
    std::vector<std::u16string> addedCodes_;
    std::uint32_t quick_ = 0;
    bool quickReady_ = false, hasA_ = false, zInside_ = false;
};
}
