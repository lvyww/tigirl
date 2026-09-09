#include "SentenceImport.h"
#include "LexiconPublish.h"
#include "MappedSentenceSupplement.h"
#include <limits>
#include <map>
namespace tiger {
namespace {
bool space(char16_t c){return (c>=9 && c<=13) || c==32 || c==0x85 || c==0xa0 || c==0x1680 ||
    (c>=0x2000 && c<=0x200a) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000;}
std::u16string_view trim(std::u16string_view s){while(!s.empty() && space(s.front()))s.remove_prefix(1);while(!s.empty() && space(s.back()))s.remove_suffix(1);return s;}
std::u16string comment(std::u16string_view s){std::u16string result;int slashes=0;for(auto c:s){
    if(c==u'#'){if(slashes%2){result.pop_back();result+=c;slashes=0;continue;}break;}
    result+=c;slashes=c==u'\\'?slashes+1:0;
}return result;}
bool weight(std::u16string_view s,std::int64_t& result) {
    auto numericSpace=[](char16_t c){return c==32 || (c>=9 && c<=13);};
    while(!s.empty() && numericSpace(s.front()))s.remove_prefix(1);
    bool negative=false;if(!s.empty() && (s.front()==u'+' || s.front()==u'-')){negative=s.front()==u'-';s.remove_prefix(1);}
    if(s.empty() || s.front()<u'0' || s.front()>u'9')return false;
    std::uint64_t n=0,maximum=negative?(1ull<<63):static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    while(!s.empty() && s.front()>=u'0' && s.front()<=u'9') {
        unsigned digit=s.front()-u'0';if(n>(maximum-digit)/10)return false;n=n*10+digit;s.remove_prefix(1);
    }
    while(!s.empty() && numericSpace(s.front()))s.remove_prefix(1);
    while(!s.empty() && s.front()==0)s.remove_prefix(1);
    if(!s.empty() || negative || !n)return false;result=static_cast<std::int64_t>(n);return true;
}
}
std::vector<SentenceSupplementEntry> parseSentenceSupplements(std::u16string_view text) {
    std::vector<SentenceSupplementEntry> result;std::map<std::u16string,std::size_t> indices;
    while(!text.empty()) {
        auto end=text.find_first_of(u"\r\n");auto cleaned=comment(text.substr(0,end));auto line=trim(cleaned);
        if(end==text.npos)text={};else text.remove_prefix(end+1);
        if(line.empty())continue;
        std::vector<std::u16string_view> parts;
        while(!line.empty()) {
            auto n=line.find_first_of(u" \t");if(n)parts.push_back(line.substr(0,n));
            if(n==line.npos)break;line.remove_prefix(n+1);
        }
        if(parts.empty() || parts.size()>2)continue;
        auto value=trim(parts[0]);if(value.empty())continue;std::int64_t n=1000;
        if(parts.size()==2 && !weight(parts[1],n))continue;
        auto entry=SentenceSupplementEntry::create(std::u16string(value),n);
        auto [it,inserted]=indices.emplace(entry.text,result.size());
        if(inserted)result.push_back(std::move(entry));else result[it->second]=std::move(entry);
    }
    return result;
}
void validateSentenceSidecars(const std::filesystem::path& ordinary) {
    SentenceLexicon lexicon(Dictionary::Open(sentenceLexiconPath(ordinary)));
    MappedSentenceSupplement supplement(Dictionary::Open(sentenceSupplementPath(ordinary)));
}
void publishSentenceSidecars(const ImportedLexicon& ordinary,std::u16string_view text,const std::filesystem::path& path) {
    auto entries=ordinary.main;
    for(auto& entry:entries)for(auto& value:entry.candidates){auto split=value.find(u'\x1e');if(split!=value.npos)value=value.substr(split+1);}
    publishImportedLexicon(prepareSentenceLexicon(entries),sentenceLexiconPath(path));
    publishImportedLexicon(SentenceSupplementMatcher(parseSentenceSupplements(text)).serializeGraph(),sentenceSupplementPath(path));
    validateSentenceSidecars(path);
}
}
