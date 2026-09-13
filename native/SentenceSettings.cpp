#include "SentenceSettings.h"
#include "Grapheme.h"
#include <algorithm>
#include <map>
namespace tiger {
namespace {
bool space(char16_t c){return (c>=9 && c<=13) || c==32 || c==0x85 || c==0xa0 || c==0x1680 ||
    (c>=0x2000 && c<=0x200a) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000;}
std::u16string_view trim(std::u16string_view s){while(!s.empty() && space(s.front()))s.remove_prefix(1);while(!s.empty() && space(s.back()))s.remove_suffix(1);return s;}
bool boolean(std::u16string value,bool fallback) {
    for(auto& c:value)if(c>=u'A' && c<=u'Z')c+=u'a'-u'A';
    if(value==u"是" || value==u"true" || value==u"on" || value==u"1")return true;
    if(value==u"否" || value==u"false" || value==u"off" || value==u"0")return false;
    return fallback;
}
int nonnegativeInteger(std::u16string_view text) {
    if(text.empty())return 0;
    bool negative=false;if(text.front()==u'+' || text.front()==u'-'){negative=text.front()==u'-';text.remove_prefix(1);}
    if(text.empty() || text.front()<u'0' || text.front()>u'9')return 0;
    std::uint64_t n=0,limit=negative?2147483648ull:2147483647ull;
    while(!text.empty() && text.front()>=u'0' && text.front()<=u'9') {
        n=n*10+text.front()-u'0';if(n>limit)return 0;text.remove_prefix(1);
    }
    while(!text.empty() && (text.front()==32 || (text.front()>=9 && text.front()<=13)))text.remove_prefix(1);
    while(!text.empty() && text.front()==0)text.remove_prefix(1);
    return text.empty() && !negative?static_cast<int>(n):0;
}
}
SentenceSettings parseSentenceSettings(std::u16string_view text) {
    std::map<std::u16string,std::u16string> values;
    while(!text.empty()) {
        auto end=text.find_first_of(u"\r\n");auto line=text.substr(0,end);
        if(end==text.npos)text={};else text.remove_prefix(end+1);
        while(!line.empty() && space(line.front()))line.remove_prefix(1);
        if(line.empty() || line.front()==u'#')continue;
        auto separator=line.find_first_of(u"\t ,");if(separator==line.npos || separator==0)continue;
        values[std::u16string(trim(line.substr(0,separator)))]=trim(line.substr(separator+1));
    }
    SentenceSettings result;
    for(const auto& item:values) {
        const auto& key=item.first;const auto& value=item.second;
        if(key==u"自动启用整句模式")result.autoEnableBySchema=boolean(value,true);
        else if(key==u"整句Tab自学习")result.selfLearning=boolean(value,true);
        else if(key==u"整句自动提前上屏")result.autoCommit=boolean(value,true);
        else if(key==u"允许单字重码组句")result.allowDuplicateSingleCharacters=boolean(value,true);
        else if(key==u"保留最少编码数量")result.minimumRetainedRaw=std::min(32,nonnegativeInteger(value));
        else if(key==u"高频字仅使用最优码组句")result.commonCharacterLimit=nonnegativeInteger(value);
        else if(key==u"整句允许全码组句白名单")result.fullCodeWhitelist=value;
        else if(key==u"整句语言模型")result.modelPath=value;
    }
    return result;
}
SentenceLexicon::Characters SentenceSettings::whitelist() const {
    SentenceLexicon::Characters result;
    for(auto& element:wordTextElements(trim(fullCodeWhitelist)))if(!std::all_of(element.begin(),element.end(),space))result.insert(std::move(element));
    return result;
}
}
