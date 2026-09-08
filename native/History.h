#pragma once
#include "Grapheme.h"
#include <array>
#include <memory>
namespace tiger {
// Persistent chunks retain every commit element. Previews share the old chunks;
// appending copies only the partial head when another history still observes it.
class History {
    static constexpr std::size_t capacity=64;
    struct Node {
        std::u16string text;
        std::array<std::size_t,capacity> ends{};
        std::shared_ptr<Node> previous;
        explicit Node(std::shared_ptr<Node> tail):previous(std::move(tail)) {}
        ~Node() {
            while(previous && previous.use_count()==1) {
                auto tail=std::move(previous->previous);
                previous.reset();previous=std::move(tail);
            }
        }
        std::u16string_view element(std::size_t index) const {
            const auto start=index?ends[index-1]:0;
            return std::u16string_view(text).substr(start,ends[index]-start);
        }
    };
    std::shared_ptr<Node> head_;
    std::size_t used_=0;
public:
    bool empty() const { return !head_; }
    std::u16string_view last() const { return head_?head_->element(used_-1):std::u16string_view{}; }
    void append(std::u16string_view text) {
        // Segment each call separately, preserving the original commit boundaries.
        for(const auto& element:wordTextElements(text)) {
            if(!head_ || used_==capacity) {
                auto next=std::make_shared<Node>(head_);
                next->text=element;next->ends[0]=element.size();
                head_=std::move(next);used_=1;
            } else {
                auto next=head_.use_count()==1?head_:std::make_shared<Node>(*head_);
                next->text.resize(next->ends[used_-1]);
                next->text.append(element);next->ends[used_]=next->text.size();
                head_=std::move(next);++used_;
            }
        }
    }
    std::u16string pop() {
        if(!head_)return {};
        std::u16string text(last());
        if(!--used_) {auto previous=head_->previous;head_=std::move(previous);used_=head_?capacity:0;}
        return text;
    }
    std::vector<std::u16string> recent(std::size_t count=20) const {
        std::vector<std::u16string> result;
        auto node=head_;auto used=used_;
        while(node && result.size()<count) {
            while(used && result.size()<count)result.emplace_back(node->element(--used));
            node=node->previous;used=capacity;
        }
        std::reverse(result.begin(),result.end());return result;
    }
    std::u16string text() const {
        std::u16string result;for(const auto& item:recent())result+=item;return result;
    }
};
}
