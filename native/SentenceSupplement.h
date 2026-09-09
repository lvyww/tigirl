#pragma once
#include "Grapheme.h"
#include "LexiconImport.h"
#include <cstring>
#include <limits>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <queue>
#include <string>
#include <string_view>
#include <vector>
namespace tiger {
struct SentenceSupplementEntry {
    std::u16string text;
    std::int64_t weight=0;
    double reward=0;
    static SentenceSupplementEntry create(std::u16string text,std::int64_t weight) {
        weight=std::clamp<std::int64_t>(weight,1,1000000000);
        return {std::move(text),weight,std::clamp(9.0+2.0*std::log(weight/1000.0),0.0,16.0)};
    }
};
// Import-time automaton construction. Node IDs follow source insertion order.
// Advance never mutates the compiled graph, allowing shared ownership.
class SentenceSupplementMatcher {
    struct Node {
        std::map<std::u16string,int,std::less<>> transitions;
        int failure=0,depth=0;
        double reward=0;
    };
    std::vector<Node> nodes_{1};
public:
    explicit SentenceSupplementMatcher(const std::vector<SentenceSupplementEntry>& entries={}) {
        for(const auto& entry:entries) {
            if(entry.text.empty() || entry.reward<=0)continue;
            int state=0;
            for(const auto& element:wordTextElements(entry.text)) {
                auto found=nodes_[state].transitions.find(element);
                if(found==nodes_[state].transitions.end()) {
                    if(nodes_.size()>=static_cast<std::size_t>(std::numeric_limits<int>::max()))throw std::length_error("Supplement graph too large");
                    const auto next=static_cast<int>(nodes_.size());
                    const auto depth=nodes_[state].depth+1;
                    nodes_[state].transitions.emplace(element,next);nodes_.emplace_back();nodes_.back().depth=depth;state=next;
                }else state=found->second;
            }
            nodes_[state].reward=std::max(nodes_[state].reward,entry.reward);
        }
        std::queue<int> queue;
        for(const auto& item:nodes_[0].transitions)queue.push(item.second);
        while(!queue.empty()) {
            const auto current=queue.front();queue.pop();
            for(const auto& item:nodes_[current].transitions) {
                auto fallback=nodes_[current].failure;
                while(fallback && !nodes_[fallback].transitions.count(item.first))fallback=nodes_[fallback].failure;
                auto found=nodes_[fallback].transitions.find(item.first);
                const auto failure=found!=nodes_[fallback].transitions.end() && found->second!=item.second?found->second:0;
                nodes_[item.second].failure=failure;
                nodes_[item.second].reward=std::max(nodes_[item.second].reward,nodes_[failure].reward);
                queue.push(item.second);
            }
        }
    }
    ImportedLexicon serializeGraph() const {
        const auto hex=[](std::uint64_t value,int width) {
            constexpr char16_t digits[]=u"0123456789abcdef";std::u16string text(width,u'0');
            for(int i=width-1;i>=0;--i){text[i]=digits[value&15];value>>=4;}return text;
        };
        ImportedLexicon result;result.splits[u"_sentence_supplement_format"]=u"1";
        for(std::size_t i=0;i<nodes_.size();++i) {
            const auto& node=nodes_[i];const auto id=hex(i,8);std::uint64_t bits;
            static_assert(sizeof(bits)==sizeof(node.reward));std::memcpy(&bits,&node.reward,sizeof(bits));
            result.main.push_back({id,{hex(node.failure,8)+hex(node.depth,8)+hex(bits,16)}});
            result.indexedMain.push_back({id,8,i});
            for(const auto& edge:node.transitions)result.pinyin.push_back({id+edge.first,{hex(edge.second,8)}});
        }
        return result;
    }
    bool empty() const {return nodes_.size()<=1;}
    int advance(int state,std::u16string_view element,double& reward) const {
        reward=0;if(empty() || element.empty())return 0;
        auto current=state>=0 && static_cast<std::size_t>(state)<nodes_.size()?state:0;
        while(current && !nodes_[current].transitions.count(element))current=nodes_[current].failure;
        const auto found=nodes_[current].transitions.find(element);
        if(found!=nodes_[current].transitions.end())current=found->second;
        reward=nodes_[current].reward;return current;
    }
};
}
