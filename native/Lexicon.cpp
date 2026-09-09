#include "Lexicon.h"
#include "Text.h"
#include <algorithm>
#include <stdexcept>

namespace tiger {
namespace {
bool starts(std::u16string_view text, std::u16string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}
}
Lexicon::Lexicon(std::shared_ptr<const Dictionary> dictionary) : dictionary_(std::move(dictionary)) {
    if (!dictionary_) throw std::invalid_argument("Lexicon requires a dictionary");
    quick_ = dictionary_->quickSymbols();
}
bool Lexicon::equivalent(const Lexicon& other) const {
    if(dictionary_!=other.dictionary_ || edits_.size()!=other.edits_.size() || addedCodes_!=other.addedCodes_)return false;
    return std::equal(edits_.begin(),edits_.end(),other.edits_.begin(),[](const auto& a,const auto& b) {
        return a.first==b.first && (a.second==b.second || *a.second==*b.second);
    });
}
std::uint32_t Lexicon::lowerBound(std::u16string_view code) const {
    std::uint32_t low = 0, high = dictionary_->count(Section::Main);
    while (low < high) {
        const auto mid = low + (high-low)/2;
        if (dictionary_->at(Section::Main, mid).key < code) low = mid+1; else high = mid;
    }
    return low;
}
bool Lexicon::hasLonger(std::u16string_view code, bool includeEmpty) const {
    for (auto i = lowerBound(code); i < dictionary_->count(Section::Main); ++i) {
        const auto entry = dictionary_->at(Section::Main, i);
        if (!starts(entry.key, code)) break;
        if (entry.key.size() <= code.size()) continue;
        auto edited = edits_.find(entry.key);
        if (edited != edits_.end()) {
            if (includeEmpty || !edited->second->empty()) return true;
        } else if (entry.count || (includeEmpty && (entry.flags & 8))) return true;
    }
    for (auto it = edits_.lower_bound(code); it != edits_.end() && starts(it->first,code); ++it)
        if (it->first.size() > code.size() && (includeEmpty || !it->second->empty())) return true;
    return false;
}
Lexicon::Match Lexicon::find(Section section, std::u16string_view key) const {
    Match match;
    match.entry = dictionary_->find(section,key);
    match.count = match.entry.count; match.flags = match.entry.flags;
    if (section != Section::Main || edits_.empty()) return match;
    auto edited = edits_.find(key);
    if (edited != edits_.end()) {
        match.edited = edited->second.get();
        match.count = static_cast<std::uint32_t>(match.edited->size());
    }
    match.flags = hasLonger(key,false) ? 2u : 0u;
    if (match.count == 1 && !(match.flags & 2)) match.flags |= 1;
    if (!key.empty() && match.count == 1) {
        const auto head = key.front();
        const bool quick = (head==u';' && (quick_&1)) || (head==u'/' && (quick_&2)) ||
            (head==u'[' && (quick_&4)) || (head==u'z' && (quick_&8));
        if (quick && !hasLonger(key,true)) match.flags |= 4;
    }
    return match;
}
std::u16string_view Lexicon::value(const Match& match, std::uint32_t index) const {
    if (index >= match.count) return {};
    return match.edited ? std::u16string_view((*match.edited)[index]) : dictionary_->value(match.entry,index);
}
std::uint32_t Lexicon::quickSymbols() const { return quick_; }
void Lexicon::rebuildQuickSymbols(std::u16string_view addedKey) {
    auto observe = [&](std::u16string_view key) {
        if (key.empty()) return;
        quick_ |= (key.front()==u';'?1u:0u)|(key.front()==u'/'?2u:0u)|(key.front()==u'['?4u:0u);
        hasA_ |= key.front()==u'a';
        zInside_ |= key.front()!=u'z' && key.find(u'z')!=std::u16string_view::npos;
    };
    if (!quickReady_) {
        for (std::uint32_t i=0; i<dictionary_->count(Section::Main); ++i) {
            auto entry=dictionary_->at(Section::Main,i);
            if (entry.flags&8) observe(entry.key);
        }
        for (const auto& edit:edits_) observe(edit.first);
        quickReady_=true;
    }
    // Original Core retains exact keys even after their last candidate is deleted.
    // Consequently this key inventory only grows; user edits need no table scan.
    observe(addedKey);
    quick_=(quick_&7u)|(!zInside_&&hasA_?8u:0u);
}
std::shared_ptr<const Lexicon> Lexicon::changed(const UserChange& change, bool* didChange) const {
    auto result=std::make_shared<Lexicon>(*this);
    const bool changed=result->applyChange(change);
    if(didChange)*didChange=changed;
    return result;
}
bool Lexicon::applyChange(const UserChange& change) {
    const auto code=normalizeCode(change.code);
    if (code.empty() || change.text.empty()) throw std::invalid_argument("User change requires code and text");
    auto values=std::make_shared<Values>();
    auto current=find(Section::Main,code);
    for (std::uint32_t i=0;i<current.count;++i) values->emplace_back(value(current,i));
    const auto identity=commitText(change.text);
    auto found=std::find_if(values->begin(),values->end(),[&](const auto& text){return commitText(text)==identity;});
    bool changed=false;
    switch(change.kind) {
    case ChangeKind::Add:
        values->erase(std::remove_if(values->begin(),values->end(),[&](const auto& text){return commitText(text)==identity;}),values->end());
        values->push_back(change.text); changed=true; break;
    case ChangeKind::Delete:
        if(found!=values->end()) { values->erase(std::remove_if(values->begin(),values->end(),[&](const auto& text){return commitText(text)==identity;}),values->end()); changed=true; }
        break;
    case ChangeKind::Top:
        if(found!=values->begin() || found==values->end()) {
            std::u16string stored=found==values->end()?std::u16string(identity):*found;
            if(found!=values->end()) values->erase(found);
            values->insert(values->begin(),std::move(stored)); changed=true;
        }
        break;
    case ChangeKind::Advance:
        if(found!=values->end() && found!=values->begin()) { std::iter_swap(found,found-1); changed=true; }
        break;
    }
    if(changed) {
        const auto base=dictionary_->find(Section::Main,code);
        if(!edits_.count(code) && !base.count && !(base.flags&8))addedCodes_.push_back(code);
        edits_[code]=std::move(values); rebuildQuickSymbols(code);
    }
    return changed;
}
}
