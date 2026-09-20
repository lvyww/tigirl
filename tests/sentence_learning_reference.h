#pragma once
// Independent pre-index oracle for differential tests.
// Mirrors the manual-correction level policy without using production indexes.
#include "SentenceLearning.h"
namespace tiger::learning_test {
class ReferenceLearningSnapshot {
    struct Choice {std::u16string mode,code,text,context;double weight=0;};
    std::map<std::u16string,std::vector<Choice>> byCode_;
    static int level(double weight) {return std::clamp(static_cast<int>(std::floor(weight+1e-12)),0,10);}
    static double general(double weight) {const int n=level(weight);return n?4+2*n:0;}
    static double exact(double weight) {const int n=level(weight);return n?7+2*n:0;}
public:
    bool empty()const{return byCode_.empty();}
    static std::shared_ptr<const ReferenceLearningSnapshot> build(const std::vector<SentenceLearningEvent>& events,std::int64_t now=learningNow()) {
        (void)now;auto snapshot=std::make_shared<ReferenceLearningSnapshot>();
        for(const auto& event:events) {
            if(event.mode.empty() || event.mode.size()>512 || event.code.empty() || event.code.size()>128 || !learningStaticText(event.text) ||
               (!event.context.empty() && (!learningCharacters(event.context) || learningCharacters(event.context)>2)))continue;
            auto& choices=snapshot->byCode_[event.code];Choice* target=nullptr;
            for(auto& c:choices) {
                if(c.mode!=event.mode || c.context!=event.context)continue;
                if(c.text==event.text)target=&c;else c.weight*=0.25;
            }
            if(!target) {choices.push_back(Choice{event.mode,event.code,event.text,event.context,0});target=&choices.back();}
            target->weight=std::min(10.0,target->weight+1);
        }
        return snapshot;
    }
    double prefixScore(std::u16string_view mode,std::u16string_view code,std::u16string_view text,std::u16string_view context) const {
        if(code.empty() || text.empty())return 0;
        double result=0;unsigned checked=0;
        for(auto it=byCode_.lower_bound(std::u16string(code));it!=byCode_.end() && checked<64;++it,++checked) {
            if(std::u16string_view(it->first).substr(0,code.size())!=code)break;
            if(it->first.size()<=code.size())continue;
            for(const auto& c:it->second) {
                if(c.mode!=mode || c.text.size()<=text.size() || std::u16string_view(c.text).substr(0,text.size())!=text)continue;
                result=std::max(result,score(mode,it->first,c.text,context));
            }
        }
        return result;
    }
    double score(std::u16string_view mode,std::u16string_view code,std::u16string_view text,std::u16string_view context) const {
        auto found=byCode_.find(std::u16string(code));if(found==byCode_.end())return 0;
        double local=0,aggregate=0;
        for(const auto& c:found->second) {
            if(c.mode!=mode || c.text!=text)continue;
            if(c.context==context)local=exact(c.weight);
            aggregate+=c.weight;
        }
        return std::max(local,general(aggregate));
    }
};
}
