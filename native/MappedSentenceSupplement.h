#pragma once
#include "Dictionary.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace tiger {
// Read-only graph: no node/edge arrays are deserialized into the host heap.
class MappedSentenceSupplement {
    std::shared_ptr<const Dictionary> dictionary_;
    std::uint32_t count_=0;
    struct Meta {std::uint32_t failure,depth;double reward;};
    static std::uint64_t number(std::u16string_view text) {
        if(text.empty() || text.size()>16)throw std::runtime_error("Invalid supplement integer");
        std::uint64_t value=0;
        for(auto c:text){if(!((c>=u'0' && c<=u'9') || (c>=u'a' && c<=u'f')))throw std::runtime_error("Invalid supplement hex");value=(value<<4)|(c<=u'9'?c-u'0':c-u'a'+10);}
        return value;
    }
    static std::u16string key(std::uint32_t id) {
        constexpr char16_t hex[]=u"0123456789abcdef";std::u16string result(8,u'0');
        for(int i=7;i>=0;--i){result[i]=hex[id&15];id>>=4;}return result;
    }
    Meta meta(std::uint32_t id) const {
        const auto entry=dictionary_->at(Section::Main,id);
        const auto value=dictionary_->value(entry,0);
        if(entry.count!=1 || value.size()!=32)throw std::runtime_error("Invalid supplement node");
        const auto bits=number(value.substr(16));double reward;std::memcpy(&reward,&bits,sizeof(reward));
        return {static_cast<std::uint32_t>(number(value.substr(0,8))),static_cast<std::uint32_t>(number(value.substr(8,8))),reward};
    }
    int transition(std::uint32_t id,std::u16string_view element) const {
        auto edge=key(id);edge.append(element);
        const auto entry=dictionary_->find(Section::Pinyin,edge);
        return entry.count?static_cast<int>(number(dictionary_->value(entry,0))):-1;
    }
public:
    explicit MappedSentenceSupplement(std::shared_ptr<const Dictionary> dictionary):dictionary_(std::move(dictionary)) {
        if(!dictionary_ || dictionary_->value(dictionary_->find(Section::Split,u"_sentence_supplement_format"),0)!=u"1")throw std::runtime_error("Not a supplement graph");
        count_=dictionary_->count(Section::Main);
        if(!count_ || count_>static_cast<std::uint32_t>(std::numeric_limits<int>::max()))throw std::runtime_error("Invalid supplement node count");
        for(std::uint32_t i=0;i<count_;++i){
            const auto node=meta(i);
            if(dictionary_->at(Section::Main,i).key!=key(i) || node.failure>=count_ || !std::isfinite(node.reward) || node.reward<0 ||
                (i==0 && (node.failure || node.depth || node.reward!=0)) ||
                (i && (node.depth==0 || node.depth>=count_ || meta(node.failure).depth>=node.depth)))throw std::runtime_error("Invalid supplement failure chain");
        }
        for(std::uint32_t i=0;i<dictionary_->count(Section::Pinyin);++i){
            const auto entry=dictionary_->at(Section::Pinyin,i);const auto value=dictionary_->value(entry,0);
            if(entry.key.size()<=8 || entry.count!=1 || value.size()!=8)throw std::runtime_error("Invalid supplement edge");
            const auto from=number(entry.key.substr(0,8)),to=number(value);
            if(from>=count_ || to>=count_ || meta(static_cast<std::uint32_t>(from)).depth+1!=meta(static_cast<std::uint32_t>(to)).depth)throw std::runtime_error("Invalid supplement transition");
        }
    }
    bool empty() const {return count_<=1;}
    int advance(int state,std::u16string_view element,double& reward) const {
        reward=0;if(empty() || element.empty())return 0;
        auto current=state>=0 && static_cast<std::uint32_t>(state)<count_?static_cast<std::uint32_t>(state):0;
        auto next=transition(current,element);
        while(current && next<0){current=meta(current).failure;next=transition(current,element);}
        if(next>=0)current=static_cast<std::uint32_t>(next);
        reward=meta(current).reward;return static_cast<int>(current);
    }
};
}
