#include "SentenceLexicon.h"
#include "AddWord.h"
#include "Grapheme.h"
#include "OrdinalCase.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
namespace tiger {
namespace {
std::u16string ordinalKey(std::uint32_t value) {
    std::u16string result(8,u'0');constexpr char16_t digits[]=u"0123456789abcdef";
    for(int i=7;i>=0;--i){result[i]=digits[value&15];value>>=4;}
    return result;
}
}
ImportedLexicon prepareSentenceLexicon(const std::vector<ImportedLexiconEntry>& source) {
    ImportedLexicon out;std::map<std::u16string,std::size_t> exact;
    std::set<std::u16string> inventory;
    for(const auto& entry:source) {
        auto code=normalizeAddedCode(entry.code);if(code.empty())continue;
        auto inventoryCode=ordinalCaseKey(code);
        if(inventory.insert(inventoryCode).second) {
            if(out.comments.size()>=std::numeric_limits<std::uint32_t>::max())throw std::length_error("Sentence inventory too large");
            out.comments.emplace(ordinalKey(static_cast<std::uint32_t>(out.comments.size())),code);
        }
        std::vector<std::u16string> values;std::set<std::u16string> seen;
        for(const auto& text:entry.candidates)if(!text.empty() && seen.insert(text).second)values.push_back(text);
        if(values.empty())continue;
        auto folded=ordinalCaseKey(code);auto added=exact.emplace(folded,out.main.size());
        if(added.second)out.main.push_back({std::move(code),std::move(values)});
        else out.main[added.first->second].candidates=std::move(values);
    }
    struct Choice {std::u16string optimal,first,any;};std::map<std::u16string,Choice> choices;
    const auto shorter=[](std::u16string& current,const std::u16string& code){if(current.empty() || code.size()<current.size())current=code;};
    for(const auto& entry:out.main)for(std::size_t rank=0;rank<entry.candidates.size();++rank) {
        const auto& text=entry.candidates[rank];if(wordTextElements(text).size()!=1)continue;
        auto& choice=choices[text];shorter(choice.optimal,entry.code);
        if(entry.code.size()>=2){shorter(choice.any,entry.code);if(rank==0)shorter(choice.first,entry.code);}
    }
    for(const auto& item:choices) {
        out.construct[item.first]=ordinalCaseKey(item.second.optimal);
        const auto& primary=item.second.first.empty()?item.second.any:item.second.first;
        if(!primary.empty())out.fullCodes[item.first]=ordinalCaseKey(primary);
    }
    if(out.main.empty())out.main.push_back({u"_sentence_empty",{}});
    // Prefixes are evaluated after common/whitelist filtering in the reader.
    for(std::size_t i=0;i<out.main.size();++i){out.main[i].code=ordinalCaseKey(out.main[i].code);out.indexedMain.push_back({out.main[i].code,8,i});}
    out.splits[u"_sentence_format"]=u"3";
    return out;
}
SentenceLexicon::SentenceLexicon(std::shared_ptr<const Dictionary> dictionary,Characters common,Characters whitelist):
    dictionary_(std::move(dictionary)),common_(std::move(common)),whitelist_(std::move(whitelist)) {
    if(!dictionary_ || dictionary_->value(dictionary_->find(Section::Split,u"_sentence_format"),0)!=u"3")
        throw std::runtime_error("Not a prepared sentence lexicon");
    for(std::uint32_t i=0;i<sourceCodeCount();++i) {
        const auto entry=dictionary_->at(Section::Comment,i);
        if(entry.key!=ordinalKey(i) || entry.count!=1 || dictionary_->value(entry,0).empty())
            throw std::runtime_error("Invalid sentence source inventory");
    }
    std::set<int> lengths;
    for(std::uint32_t i=0;i<dictionary_->count(Section::Main);++i){const auto entry=dictionary_->at(Section::Main,i);
        if(entry.key.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()))throw std::length_error("Sentence code too long");
        if(hasAllowed(entry))lengths.insert(static_cast<int>(entry.key.size()));}
    codeLengths_.assign(lengths.begin(),lengths.end());
}
std::u16string_view SentenceLexicon::sourceCode(std::uint32_t index) const {
    if(index>=sourceCodeCount())return {};
    return dictionary_->value(dictionary_->at(Section::Comment,index),0);
}
bool SentenceLexicon::allowed(std::u16string_view code,std::u16string_view text) const {
    if(code.size()==1 || !common_.count(text) || whitelist_.count(text))return true;
    const auto optimal=dictionary_->find(Section::ConstructCode,text);
    if(!optimal.count)return true; // Multi-element text has no single-character metadata.
    return dictionary_->value(dictionary_->find(Section::FullCode,text),0)==code;
}
bool SentenceLexicon::hasAllowed(const Dictionary::Entry& entry) const {
    for(std::uint32_t i=0;i<entry.count;++i)if(allowed(entry.key,dictionary_->value(entry,i)))return true;
    return false;
}
std::vector<SentenceLexiconCandidate> SentenceLexicon::candidates(std::u16string_view code) const {
    return *candidateView(code);
}
std::shared_ptr<const std::vector<SentenceLexiconCandidate>> SentenceLexicon::candidateView(std::u16string_view code) const {
    const auto key=ordinalCaseKey(code);
    {std::lock_guard<std::mutex> lock(metadataMutex_);auto found=metadata_.find(key);if(found!=metadata_.end())return found->second.values;}
    const auto entry=dictionary_->find(Section::Main,key);
    struct Storage {std::shared_ptr<const Dictionary> owner;std::vector<SentenceLexiconCandidate> values;};
    auto storage=std::make_shared<Storage>();storage->owner=dictionary_;
    auto result=std::shared_ptr<std::vector<SentenceLexiconCandidate>>(storage,&storage->values);
    result->reserve(std::min<std::uint32_t>(entry.count,128));
    std::size_t bytes=sizeof(CachedCandidates)+128+key.size()*sizeof(char16_t)*2;
    for(std::uint32_t i=0;i<entry.count;++i){const auto text=dictionary_->value(entry,i);if(!allowed(key,text))continue;
        result->push_back({text,i+1,std::log(i+1.0),wordTextElements(text),
            dictionary_->value(dictionary_->find(Section::ConstructCode,text),0)==key});
        const auto& elements=result->back().textElements;
        bytes+=elements.capacity()*sizeof(std::u16string);
        for(const auto& element:elements)bytes+=(element.capacity()+1)*sizeof(char16_t);
    }
    bytes+=result->capacity()*sizeof(SentenceLexiconCandidate);
    constexpr std::size_t budget=1024*1024;
    if(bytes<=budget) {
        std::lock_guard<std::mutex> lock(metadataMutex_);
        auto found=metadata_.find(key);if(found!=metadata_.end())return found->second.values;
        while(!metadataOrder_.empty() && (metadata_.size()>=256 || metadataBytes_+bytes>budget)) {
            auto old=metadata_.find(metadataOrder_.front());metadataBytes_-=old->second.bytes;
            metadata_.erase(old);metadataOrder_.pop_front();
        }
        metadata_.emplace(key,CachedCandidates{result,bytes});metadataOrder_.push_back(key);metadataBytes_+=bytes;
    }
    return result;
}
bool SentenceLexicon::isProperCodePrefix(std::u16string_view code) const {
    if(code.empty())return false;const auto key=ordinalCaseKey(code);
    {std::lock_guard<std::mutex> lock(metadataMutex_);auto found=prefixes_.find(key);if(found!=prefixes_.end())return found->second;}
    const bool value=properPrefixUncached(code);
    // Long custom codes do not create an unbounded side cache.
    if(key.size()<=128) {
        std::lock_guard<std::mutex> lock(metadataMutex_);
        if(!prefixes_.count(key)) {
            if(prefixes_.size()>=256){prefixes_.erase(prefixOrder_.front());prefixOrder_.pop_front();}
            prefixes_.emplace(key,value);prefixOrder_.push_back(key);
        }
    }
    return value;
}
bool SentenceLexicon::properPrefixUncached(std::u16string_view code) const {
    if(code.empty())return false;const auto key=ordinalCaseKey(code);
    std::uint32_t low=0,high=dictionary_->count(Section::Main);
    while(low<high){const auto mid=low+(high-low)/2;if(dictionary_->at(Section::Main,mid).key<=key)low=mid+1;else high=mid;}
    for(auto i=low;i<dictionary_->count(Section::Main);++i){const auto entry=dictionary_->at(Section::Main,i);
        if(entry.key.size()<key.size() || entry.key.substr(0,key.size())!=key)break;
        if(entry.key.size()>key.size() && hasAllowed(entry))return true;
    }
    return false;
}
}
