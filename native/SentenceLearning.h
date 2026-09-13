#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <tuple>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
namespace tiger {
inline std::int64_t learningNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
inline std::u16string learningConfigurationHash(std::u16string_view text) {
    std::uint64_t hash=14695981039346656037ull;
    for(auto c:text){hash^=c&255;hash*=1099511628211ull;hash^=c>>8;hash*=1099511628211ull;}
    std::u16string result(16,u'0');constexpr char16_t digits[]=u"0123456789abcdef";
    for(int i=15;i>=0;--i){result[i]=digits[hash&15];hash>>=4;}return result;
}
inline std::string learningId() {
    static const auto nonce=[] {
        std::ostringstream s;
        s<<std::hex<<static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
         ;try{std::random_device r;s<<r()<<r();}catch(...){s<<reinterpret_cast<std::uintptr_t>(&s);}
        return s.str();
    }();
    static std::atomic<std::uint64_t> sequence{0};
    return nonce+"-"+std::to_string(++sequence);
}
// Returns 0 for malformed UTF-16. Offsets below are UTF-16 offsets; length
// limits/context suffixes are Unicode scalar counts, never surrogate halves.
inline std::size_t learningCharacters(std::u16string_view text) {
    std::size_t count=0;
    for(std::size_t i=0;i<text.size();++i,++count) {
        auto c=text[i];
        if(c>=0xd800 && c<=0xdbff) {
            if(++i>=text.size() || text[i]<0xdc00 || text[i]>0xdfff)return 0;
        }else if(c>=0xdc00 && c<=0xdfff)return 0;
    }
    return count;
}
inline std::u16string learningContext(std::u16string_view prefix) {
    auto start=prefix.size();
    for(int n=0;n<2 && start;n++) {
        --start;if(prefix[start]>=0xdc00 && prefix[start]<=0xdfff && start)--start;
    }
    return std::u16string(prefix.substr(start));
}
inline bool learningStaticText(std::u16string_view text) {
    auto n=learningCharacters(text);if(!n || n>16)return false;
    for(auto c:text) {
        // Commands/dynamic expansions and packed display aliases are never learnt.
        if(c<32 || c==127 || c==u'{' || c==u'}' || c==u'\t' ||
           (c>=0xe000 && c<=0xf8ff))return false;
    }
    return true;
}
struct SentenceLearningEvent {
    std::string id;
    std::int64_t time=0;
    std::u16string mode,code,text,context;
    int rawStart=0,rawEnd=0,textStart=0,textEnd=0;
};
struct SentenceLearningBoundary {int raw=0,text=0;};
// Compare only intervals delimited by raw boundaries shared by BOTH paths.
// No substring-to-code guesses and no learning of unchanged sentence prefixes.
inline std::vector<SentenceLearningEvent> sentenceLearningDiff(
    std::u16string_view raw,std::u16string_view before,std::u16string_view selected,
    const std::vector<SentenceLearningBoundary>& a,const std::vector<SentenceLearningBoundary>& b,
    int floorRaw) {
    std::vector<SentenceLearningEvent> result;
    if(before==selected || a.empty() || b.empty())return result;
    std::map<int,int> left,right;
    left[0]=right[0]=0;
    auto fill=[&](auto& map,const auto& values,std::u16string_view text) {
        int oldRaw=0,oldText=0;
        for(auto p:values) {
            if(p.raw<=oldRaw || p.text<=oldText || p.raw>static_cast<int>(raw.size()) || p.text>static_cast<int>(text.size()))return false;
            if(!learningCharacters(text.substr(oldText,p.text-oldText)))return false;
            map[p.raw]=p.text;oldRaw=p.raw;oldText=p.text;
        }
        return oldRaw==static_cast<int>(raw.size()) && oldText==static_cast<int>(text.size());
    };
    if(!fill(left,a,before) || !fill(right,b,selected))return result;
    int previous=0;
    for(const auto& edge:left) {
        int end=edge.first;
        if(!end || !right.count(end))continue;
        auto first=right[previous],last=right[end];
        auto chosen=selected.substr(first,last-first);
        auto old=before.substr(left[previous],edge.second-left[previous]);
        if(previous>=floorRaw && chosen!=old && learningStaticText(chosen)) {
            SentenceLearningEvent event;event.id=learningId();event.time=learningNow();
            event.code=raw.substr(previous,end-previous);
            for(auto& c:event.code)if(c>=u'A' && c<=u'Z')c+=u'a'-u'A';
            event.text=chosen;event.context=learningContext(selected.substr(0,first));
            event.rawStart=previous;event.rawEnd=end;event.textStart=first;event.textEnd=last;
            result.push_back(std::move(event));
        }
        previous=end;
    }
    return result;
}
class SentenceLearningSnapshot {
    using ContextScores=std::map<std::u16string,double,std::less<>>;
    struct Scores {
        ContextScores exact;
        double general=0;
        double forContext(std::u16string_view context) const {
            const auto it=exact.find(context);
            return it==exact.end()?general:std::max(general,it->second);
        }
    };
    using TextScores=std::map<std::u16string,Scores,std::less<>>;
    using ModeScores=std::map<std::u16string,TextScores,std::less<>>;
    // Immutable query index: each text occurs once per (code, mode), regardless
    // of the number of contexts/events. Transparent lookups allocate no keys.
    std::map<std::u16string,ModeScores,std::less<>> byCode_;
public:
    bool empty()const{return byCode_.empty();}
    static std::shared_ptr<const SentenceLearningSnapshot> build(const std::vector<SentenceLearningEvent>& events,std::int64_t now=learningNow()) {
        struct Choice {double weight=0;int count=0;std::int64_t time=0;};
        // Replay competitors only within the same (code, mode, context).
        // Different contexts must not make snapshot construction quadratic.
        using Key=std::tuple<std::u16string,std::u16string,std::u16string>;
        std::map<Key,std::map<std::u16string,Choice>> groups;
        for(const auto& event:events) {
            if(event.mode.empty() || event.mode.size()>512 || event.code.empty() || event.code.size()>128 || !learningStaticText(event.text) ||
               (!event.context.empty() && (!learningCharacters(event.context) || learningCharacters(event.context)>2)))continue;
            auto& choices=groups[{event.code,event.mode,event.context}];
            const auto time=std::min(now,event.time);
            for(auto& entry:choices) {
                auto& c=entry.second;
                c.weight*=std::exp2(-static_cast<double>(std::max<std::int64_t>(0,time-c.time))/(30.0*86400));c.time=std::max(c.time,time);
                if(entry.first!=event.text)c.weight*=0.25;
            }
            auto& target=choices.try_emplace(event.text,Choice{0,0,time}).first->second;
            // One correction equals supplement weight 1000; cap at the weight for 16 points.
            target.weight=std::min(std::exp(3.5),target.weight+1);target.count=std::min(3,target.count+1);
        }
        struct Summary {ContextScores exact;double weight=0;int count=0;unsigned contexts=0;};
        std::map<Key,Summary> summaries;
        for(const auto& group:groups) {
            const auto& code=std::get<0>(group.first);
            const auto& mode=std::get<1>(group.first);
            const auto& context=std::get<2>(group.first);
            for(const auto& entry:group.second) {
                const auto& c=entry.second;
                const double weight=c.weight*std::exp2(-static_cast<double>(std::max<std::int64_t>(0,now-c.time))/(30.0*86400));
                auto& summary=summaries[{code,mode,entry.first}];
                summary.exact[context]=std::clamp(9+2*std::log(std::max(0.001,weight)),0.0,16.0);
                summary.weight+=weight;summary.count=std::min(3,summary.count+c.count);
                // Each context appears only once here. Empty means unknown,
                // not a proven sentence start; weak contexts do not qualify.
                if(!context.empty() && weight>=0.1)++summary.contexts;
            }
        }
        auto snapshot=std::make_shared<SentenceLearningSnapshot>();
        for(auto& entry:summaries) {
            const auto& code=std::get<0>(entry.first);
            const auto& mode=std::get<1>(entry.first);
            const auto& text=std::get<2>(entry.first);
            auto& summary=entry.second;
            auto& scores=snapshot->byCode_[code][mode][text];
            scores.general=learningCharacters(text)>1 && summary.count>=3 && summary.contexts>=2?2*std::min(1.0,summary.weight/3):0;
            scores.exact=std::move(summary.exact);
        }
        return snapshot;
    }
    // Bounded lookup for an unfinished, previously learnt fragment. This is
    // ONLY a beam-retention hint, never a final-score or confidence reward.
    double prefixScore(std::u16string_view mode,std::u16string_view code,std::u16string_view text,std::u16string_view context) const {
        if(code.empty() || text.empty())return 0;
        double result=0;unsigned checked=0;
        for(auto it=byCode_.lower_bound(code);it!=byCode_.end() && checked<64;++it,++checked) {
            if(std::u16string_view(it->first).substr(0,code.size())!=code)break;
            if(it->first.size()<=code.size())continue;
            const auto modes=it->second.find(mode);if(modes==it->second.end())continue;
            const auto& texts=modes->second;
            for(auto choice=texts.lower_bound(text);choice!=texts.end();++choice) {
                if(std::u16string_view(choice->first).substr(0,text.size())!=text)break;
                if(choice->first.size()<=text.size())continue;
                result=std::max(result,choice->second.forContext(context));
            }
        }
        return result;
    }
    double score(std::u16string_view mode,std::u16string_view code,std::u16string_view text,std::u16string_view context) const {
        const auto codes=byCode_.find(code);if(codes==byCode_.end())return 0;
        const auto modes=codes->second.find(mode);if(modes==codes->second.end())return 0;
        const auto texts=modes->second.find(text);if(texts==modes->second.end())return 0;
        return texts->second.forContext(context);
    }
};
}
