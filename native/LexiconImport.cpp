#define NOMINMAX
#include <windows.h>
#include "LexiconImport.h"
#include "AddWord.h"
#include "Text.h"
#include "OrdinalCase.h"
#include <cstdint>
#include <algorithm>
#include <map>
#include <unordered_set>
namespace tiger {
std::vector<ImportedLexiconEntry> mergeCodedLexiconRows(const std::vector<CodedLexiconRow>& rows) {
    std::vector<const CodedLexiconRow*> ordered;
    ordered.reserve(rows.size());for(const auto& row:rows) ordered.push_back(&row);
    std::stable_sort(ordered.begin(),ordered.end(),[](auto a,auto b){return a->frequency>b->frequency;});
    auto less=[](const std::u16string& a,const std::u16string& b) {
        return ordinalCompareIgnoreCase(a,b)<0;
    };
    struct Group {std::vector<std::u16string> candidates;std::unordered_set<std::u16string> seen;std::size_t order=0;};
    std::map<std::u16string,Group,decltype(less)> groups(less);
    for(const auto row:ordered) {
        auto code=normalizeAddedCode(row->code);if(code.empty()) continue;
        auto inserted=groups.try_emplace(code);
        auto& group=inserted.first->second;
        if(inserted.second) group.order=groups.size()-1;
        if(group.seen.insert(row->text).second) group.candidates.push_back(row->text);
    }
    // Auxiliary-map construction also observes the original dictionary's first
    // insertion order. Ordinal record sorting belongs to binary serialization.
    std::vector<ImportedLexiconEntry> result(groups.size());
    for(auto& entry:groups) result[entry.second.order]={entry.first,std::move(entry.second.candidates)};
    return result;
}
namespace {
void buildMainIndex(ImportedLexicon& result) {
    std::map<std::u16string,std::size_t> exact;
    std::map<std::u16string,std::u16string> prefixes;
    std::unordered_set<std::u16string> heads;
    bool internalZ=false;
    for(std::size_t i=0;i<result.main.size();++i) {
        const auto& entry=result.main[i];exact.emplace(ordinalCaseKey(entry.code),i);
        const auto head=ordinalCaseKey(std::u16string_view(entry.code).substr(0,1));heads.insert(head);
        if(head!=u"Z" && entry.code.find(u'z')!=entry.code.npos) internalZ=true;
        if(!entry.candidates.empty()) for(std::size_t length=1;length<entry.code.size();++length) {
            const auto prefix=entry.code.substr(0,length);prefixes.emplace(ordinalCaseKey(prefix),prefix);
        }
    }
    result.quickSymbols=(heads.count(u";")?1u:0u)|(heads.count(u"/")?2u:0u)|
        (heads.count(u"[")?4u:0u)|(!internalZ && heads.count(u"A")?8u:0u);
    std::map<std::u16string,std::uint64_t> symbolCounts;
    for(const auto& entry:result.main) {
        const auto head=ordinalCaseKey(std::u16string_view(entry.code).substr(0,1));
        if((head==u";" && (result.quickSymbols&1)) || (head==u"/" && (result.quickSymbols&2)) ||
           (head==u"[" && (result.quickSymbols&4)) || (head==u"Z" && (result.quickSymbols&8)))
            symbolCounts.emplace(ordinalCaseKey(entry.code),0);
    }
    for(const auto& entry:result.main) if(symbolCounts.count(ordinalCaseKey(entry.code))) {
        for(std::size_t length=1;length<=entry.code.size();++length) {
            const auto prefix=ordinalCaseKey(std::u16string_view(entry.code).substr(0,length));
            const auto count=symbolCounts.find(prefix);
            if(count!=symbolCounts.end()) count->second+=result.main[exact.at(prefix)].candidates.size();
        }
    }
    std::map<std::u16string,bool> keys;
    for(const auto& entry:result.main) keys.emplace(entry.code,true);
    for(const auto& prefix:prefixes) keys.emplace(prefix.second,true);
    for(const auto& entry:keys) {
        const auto folded=ordinalCaseKey(entry.first);const auto source=exact.find(folded);
        const bool terminal=source!=exact.end(),prefix=prefixes.count(folded)!=0;
        const auto count=symbolCounts.find(folded);
        std::uint32_t flags=terminal?8u:0u;
        if(terminal && result.main[source->second].candidates.size()==1 && !prefix) flags|=1;
        if(prefix) flags|=2;
        if(count!=symbolCounts.end() && count->second==1) flags|=4;
        result.indexedMain.push_back({entry.first,flags,terminal?source->second:static_cast<std::size_t>(-1)});
    }
}
std::map<std::u16string,std::u16string> constructLookup(const std::vector<ImportedLexiconEntry>& main,const ParsedLexiconRows* explicitRows) {
    std::map<std::u16string,std::u16string> result,best;
    if(explicitRows) for(const auto& row:explicitRows->coded) {
        const auto code=normalizeAddedCode(row.code);const auto text=commitText(row.text);
        if(!code.empty() && wordTextElements(text).size()==1) result.emplace(text,code);
    }
    auto rank=[](std::u16string_view code) {return !u_isalpha(code.front())?0:code.front()==u'o'?1:code.front()==u'z'?2:3;};
    for(const auto& entry:main) {
        const auto code=normalizeAddedCode(entry.code);if(code.size()<2) continue;
        for(const auto& packed:entry.candidates) {
            const auto text=std::u16string(commitText(packed));
            if(text.empty() || wordTextElements(text).size()!=1 || result.count(text)) continue;
            const auto found=best.find(text);
            if(found==best.end()) best.emplace(text,code);
            else if(rank(code)>rank(found->second) || (rank(code)==rank(found->second) && code<found->second)) found->second=code;
        }
    }
    result.insert(best.begin(),best.end());return result;
}
}
ImportedLexicon buildImportedLexicon(ParsedLexiconRows rows,const ParsedLexiconRows* construction,std::u16string_view adjustments) {
    const auto initial=mergeCodedLexiconRows(rows.coded);
    const auto lookup=constructLookup(initial,construction);
    if(construction) rows.uncoded.insert(rows.uncoded.end(),construction->uncoded.begin(),construction->uncoded.end());
    for(const auto& row:rows.uncoded) {
        if(row.text.empty()) continue;
        const auto code=normalizeAddedCode(constructWordCode(commitText(row.text),[&](std::u16string_view element)->std::u16string_view {
            const auto found=lookup.find(std::u16string(element));return found==lookup.end()?std::u16string_view{}:found->second;
        }));
        if(!code.empty()) rows.coded.push_back({code,row.text,row.frequency});
    }
    ImportedLexicon result;result.main=mergeCodedLexiconRows(rows.coded);
    result.construct=constructLookup(result.main,construction);
    applyImportedAdjustments(result.main,adjustments);
    for(const auto& entry:result.main) for(const auto& packed:entry.candidates) {
        const auto text=std::u16string(commitText(packed));if(text.empty()) continue;
        const auto found=result.fullCodes.find(text);
        if(found==result.fullCodes.end()) result.fullCodes.emplace(text,entry.code);
        else if(found->second.size()<entry.code.size()) found->second=entry.code;
    }
    buildMainIndex(result);
    return result;
}
namespace {
bool whitespace(char16_t c) {
    return (c>=9 && c<=13) || c==32 || c==0x85 || c==0xa0 || c==0x1680 ||
        (c>=0x2000 && c<=0x200a) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000;
}
std::u16string_view trim(std::u16string_view s) {
    while(!s.empty() && whitespace(s.front())) s.remove_prefix(1);
    while(!s.empty() && whitespace(s.back())) s.remove_suffix(1);
    return s;
}
std::vector<std::u16string_view> split(std::u16string_view text,bool tabs) {
    std::vector<std::u16string_view> parts;
    while(!text.empty()) {
        const auto end=tabs?text.find_first_of(u" \t"):text.find(u' ');
        if(end) parts.push_back(text.substr(0,end));
        if(end==text.npos) break;
        text.remove_prefix(end+1);
    }
    return parts;
}
bool integer(std::u16string_view s,int& value) {
    auto numericSpace=[](char16_t c){return c==32 || (c>=9 && c<=13);};
    while(!s.empty() && numericSpace(s.front())) s.remove_prefix(1);
    bool negative=false;
    if(!s.empty() && (s.front()==u'-' || s.front()==u'+')) {negative=s.front()==u'-';s.remove_prefix(1);}
    if(s.empty() || s.front()<u'0' || s.front()>u'9') return false;
    std::uint64_t n=0;
    while(!s.empty() && s.front()>=u'0' && s.front()<=u'9') {
        n=n*10+s.front()-u'0';s.remove_prefix(1);
        if(n>(negative?2147483648ull:2147483647ull)) return false;
    }
    while(!s.empty() && numericSpace(s.front())) s.remove_prefix(1);
    while(!s.empty() && !s.front()) s.remove_prefix(1);
    if(!s.empty()) return false;
    value=static_cast<int>(negative?-static_cast<std::int64_t>(n):static_cast<std::int64_t>(n));return true;
}
std::u16string withoutComment(std::u16string_view line) {
    std::u16string result;std::size_t slashes=0;
    for(auto c:line) {
        if(c==u'#') {
            if(slashes%2) {result.pop_back();result+=c;slashes=0;continue;}
            break;
        }
        result+=c;slashes=c==u'\\'?slashes+1:0;
    }
    return std::u16string(trim(result));
}
bool likelyCode(std::u16string_view token) {
    token=trim(token);if(token.empty()) return false;
    for(auto c:token) if(!((c>=u'a' && c<=u'z') || (c>=u'A' && c<=u'Z') ||
        (c>=u'0' && c<=u'9') || std::u16string_view(u";/[]'-=").find(c)!=std::u16string_view::npos)) return false;
    return true;
}
}
void appendImportedAnnotations(std::map<std::u16string,std::u16string>& target,std::u16string_view text,bool splitMap) {
    while(!text.empty()) {
        const auto end=text.find_first_of(u"\r\n");const auto line=trim(text.substr(0,end));
        if(end==text.npos) text={};else text.remove_prefix(end+1);
        if(line.empty() || line.front()==u'#') continue;
        const auto parts=split(line,true);if(parts.size()<2) continue;
        const auto key=decodeLexiconEscapes(parts[0]),value=decodeLexiconEscapes(parts[1]);
        if(key.empty() || value.empty()) continue;
        const auto found=target.find(key);
        if(found==target.end()) target.emplace(key,value);
        else if(splitMap) found->second=value;
        else found->second+=u" "+value;
    }
}
void applyImportedAdjustments(std::vector<ImportedLexiconEntry>& main,std::u16string_view adjustments) {
    std::map<std::u16string,std::size_t> indices;
    for(std::size_t i=0;i<main.size();++i) indices.emplace(ordinalCaseKey(main[i].code),i);
    while(!adjustments.empty()) {
        const auto end=adjustments.find_first_of(u"\r\n");auto line=trim(adjustments.substr(0,end));
        if(end==adjustments.npos) adjustments={};else adjustments.remove_prefix(end+1);
        if(line.empty() || line.front()!=u'{') continue;
        int action=-1,index=0;
        for(auto prefix:{u"{添加}",u"{删除}",u"{置顶}",u"{前移}"}) {
            if(line.substr(0,4)==prefix) action=index;
            ++index;
        }
        if(action<0) continue;
        line.remove_prefix(4);
        // Adjustment payloads split only on tabs, ignoring empty fields. Spaces
        // and '#' are literal payload characters; no inline-comment stripping.
        std::vector<std::u16string_view> parts;
        while(!line.empty() && parts.size()<2) {
            const auto tab=line.find(u'\t');if(tab) parts.push_back(line.substr(0,tab));
            if(tab==line.npos) break;line.remove_prefix(tab+1);
        }
        if(parts.size()<2) continue;
        const auto code=normalizeAddedCode(parts[0]);const auto text=parseAddedWord(parts[1]);
        if(code.empty() || text.empty()) continue;
        const auto key=ordinalCaseKey(code);auto found=indices.find(key);
        if(found==indices.end()) {
            if(action==1 || action==3) continue;
            found=indices.emplace(key,main.size()).first;main.push_back({code,{}});
        }
        auto& candidates=main[found->second].candidates;
        auto equal=[&](const auto& item){return commitText(item)==commitText(text);};
        if(action==1) {candidates.erase(std::remove_if(candidates.begin(),candidates.end(),equal),candidates.end());continue;}
        const auto position=std::find_if(candidates.begin(),candidates.end(),equal);
        if(action==3) {
            if(position!=candidates.end() && position!=candidates.begin()) std::iter_swap(position,position-1);
            continue;
        }
        auto stored=position==candidates.end()?text:*position;
        if(position!=candidates.end()) candidates.erase(position);
        if(action==2) candidates.insert(candidates.begin(),std::move(stored));
        else candidates.push_back(std::move(stored));
    }
}
ParsedLexiconRows parseLexiconRows(std::u16string_view text,bool yaml) {
    ParsedLexiconRows rows;bool body=!yaml;
    while(!text.empty()) {
        const auto end=text.find_first_of(u"\r\n");auto raw=trim(text.substr(0,end));
        if(end==text.npos) text={};else {const bool crlf=text[end]==u'\r' && end+1<text.size() && text[end+1]==u'\n';text.remove_prefix(end+1+crlf);}
        if(raw.empty() || raw.front()==u'#') continue;
        if(!body) {if(raw==u"...") body=true;continue;}
        const auto line=withoutComment(raw);
        if(line.empty()) continue;
        bool adjustment=false;
        for(auto prefix:{u"{添加}",u"{删除}",u"{置顶}",u"{前移}"}) if(line.rfind(prefix,0)==0) adjustment=true;
        if(adjustment) continue;
        if(line.find(u'\t')==line.npos) {
            const auto parts=split(line,false);
            if(parts.size()>=2 && likelyCode(parts[0])) {
                auto count=parts.size();int frequency=0;
                if(count>=3 && integer(parts.back(),frequency)) --count;
                std::vector<std::u16string> candidates;
                for(std::size_t i=1;i<count;++i) {auto candidate=parseAddedWord(parts[i]);if(!candidate.empty()) candidates.push_back(std::move(candidate));}
                if(!candidates.empty()) {
                    const auto code=normalizeAddedCode(parts[0]);
                    for(auto& candidate:candidates) rows.coded.push_back({code,std::move(candidate),frequency});
                    continue;
                }
            }
        }
        const auto parts=split(line,true);if(parts.empty()) continue;
        auto candidate=parseAddedWord(parts[0]);if(candidate.empty()) continue;
        std::u16string_view code;int frequency=0,number=0;
        if(parts.size()>=3 && integer(parts[2],number)) {code=parts[1];frequency=number;}
        else if(parts.size()>=3 && integer(parts[1],number)) {code=parts[2];frequency=number;}
        else if(parts.size()>=2 && !integer(parts[1],number)) code=parts[1];
        else if(parts.size()>=2) frequency=number;
        code=trim(code);
        if(code.empty()) rows.uncoded.push_back({std::move(candidate),frequency});
        else rows.coded.push_back({std::u16string(code),std::move(candidate),frequency});
    }
    return rows;
}
}
