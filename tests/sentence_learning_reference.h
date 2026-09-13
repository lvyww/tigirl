#pragma once
// Pre-index algorithm from PR #12 (6401724), for differential tests.
// Scoring constants/formula updated to the 2026-09-14 correction-weight policy.
// Keep independent of the optimized implementation; never used by production.
#include "SentenceLearning.h"
#include <set>
namespace tiger::learning_test {
class ReferenceLearningSnapshot {
    struct Choice {std::u16string mode,code,text,context;double weight=0;int count=0;std::int64_t time=0;};
    std::map<std::u16string,std::vector<Choice>> byCode_;
public:
    bool empty()const{return byCode_.empty();}
    static std::shared_ptr<const ReferenceLearningSnapshot> build(const std::vector<SentenceLearningEvent>& events,std::int64_t now=learningNow()) {
        auto snapshot=std::make_shared<ReferenceLearningSnapshot>();
        for(const auto& event:events) {
            if(event.mode.empty() || event.mode.size()>512 || event.code.empty() || event.code.size()>128 || !learningStaticText(event.text) ||
               (!event.context.empty() && (!learningCharacters(event.context) || learningCharacters(event.context)>2)))continue;
            auto& choices=snapshot->byCode_[event.code];Choice* target=nullptr;
            auto time=std::min(now,event.time);
            for(auto& c:choices) {
                if(c.mode!=event.mode || c.context!=event.context)continue;
                c.weight*=std::exp2(-static_cast<double>(std::max<std::int64_t>(0,time-c.time))/(30.0*86400));c.time=std::max(c.time,time);
                if(c.text==event.text)target=&c;else c.weight*=0.25;
            }
            if(!target) {choices.push_back(Choice{event.mode,event.code,event.text,event.context,0,0,time});target=&choices.back();}
            target->weight=std::min(std::exp(3.5),target->weight+1);target->count=std::min(3,target->count+1);
        }
        for(auto& row:snapshot->byCode_)for(auto& c:row.second)
            c.weight*=std::exp2(-static_cast<double>(std::max<std::int64_t>(0,now-c.time))/(30.0*86400));
        return snapshot;
    }
    // Bounded lookup for an unfinished, previously learnt fragment. This is
    // ONLY a beam-retention hint, never a final-score or confidence reward.
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
        double exact=0,aggregate=0;int count=0;std::set<std::u16string> contexts;
        for(const auto& c:found->second) {
            if(c.mode!=mode || c.text!=text)continue;
            if(c.context==context)exact=std::clamp(9+2*std::log(std::max(0.001,c.weight)),0.0,16.0);
            aggregate+=c.weight;count+=c.count;
            // Empty context means unknown, not a proven beginning of sentence.
            if(!c.context.empty() && c.weight>=0.1)contexts.insert(c.context);
        }
        double general=learningCharacters(text)>1 && count>=3 && contexts.size()>=2?2*std::min(1.0,aggregate/3):0;
        return std::max(exact,general);
    }
};
}
