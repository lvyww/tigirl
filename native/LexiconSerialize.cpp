#include "LexiconSerialize.h"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace tiger {
namespace {
struct Record {
    std::u16string_view key;
    std::uint32_t flags;
    std::vector<std::u16string_view> values;
};
std::uint32_t narrow(std::size_t n) {
    if(n>std::numeric_limits<std::uint32_t>::max()) throw std::length_error("Dictionary count exceeds v2 limit");
    return static_cast<std::uint32_t>(n);
}
std::size_t add(std::size_t base,std::size_t count,std::size_t width) {
    if(count>(std::numeric_limits<std::size_t>::max()-base)/width)
        throw std::length_error("Dictionary size overflow");
    return base+count*width;
}
void put(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint64_t value,unsigned width) {
    for(unsigned i=0;i<width;++i) bytes.at(at+i)=static_cast<std::uint8_t>(value>>(8*i));
}
}
std::vector<std::uint8_t> serializeImportedLexicon(const ImportedLexicon& lexicon) {
    if(lexicon.main.empty() || lexicon.indexedMain.empty()) throw std::invalid_argument("Empty main lexicon");
    if(lexicon.quickSymbols & ~15u) throw std::invalid_argument("Invalid quick-symbol bits");
    std::array<std::vector<Record>,6> sections;
    for(const auto& index:lexicon.indexedMain) {
        if(index.flags & ~15u) throw std::invalid_argument("Invalid main flags");
        Record record{index.code,index.flags,{}};
        if(index.sourceIndex!=static_cast<std::size_t>(-1)) {
            if(index.sourceIndex>=lexicon.main.size()) throw std::invalid_argument("Invalid main source index");
            for(const auto& value:lexicon.main[index.sourceIndex].candidates) record.values.push_back(value);
        }
        if(!record.values.empty() && !(record.flags&8)) throw std::invalid_argument("Missing exact-key flag");
        if((record.flags&1) && (record.values.size()!=1 || (record.flags&2)))
            throw std::invalid_argument("Invalid unique flag");
        sections[0].push_back(std::move(record));
    }
    for(const auto& entry:lexicon.pinyin) {
        Record record{entry.code,0,{}};
        for(const auto& value:entry.candidates) record.values.push_back(value);
        sections[1].push_back(std::move(record));
    }
    const std::array<const std::map<std::u16string,std::u16string>*,4> maps{
        &lexicon.comments,&lexicon.splits,&lexicon.fullCodes,&lexicon.construct};
    for(std::size_t i=0;i<maps.size();++i)
        for(const auto& entry:*maps[i]) sections[i+2].push_back({entry.first,0,{entry.second}});
    std::size_t valuesAt=128;
    for(auto& section:sections) {
        narrow(section.size());
        std::sort(section.begin(),section.end(),[](const Record& a,const Record& b){return a.key<b.key;});
        for(std::size_t i=1;i<section.size();++i)
            if(section[i-1].key==section[i].key) throw std::invalid_argument("Duplicate dictionary key");
        valuesAt=add(valuesAt,section.size(),32);
    }
    std::size_t stringsAt=valuesAt;
    for(const auto& section:sections) for(const auto& record:section) {
        if(record.key.empty()) throw std::invalid_argument("Empty dictionary key");
        narrow(record.key.size()); narrow(record.values.size());
        stringsAt=add(stringsAt,record.values.size(),16);
        for(auto value:record.values) narrow(value.size());
    }
    std::vector<std::uint8_t> bytes(stringsAt,0);
    const char magic[]="TIGERD02";
    std::copy_n(magic,8,bytes.begin());
    put(bytes,8,2,4); put(bytes,12,6,4); put(bytes,24,lexicon.quickSymbols,4);
    std::unordered_map<std::u16string_view,std::size_t> pool;
    auto intern=[&](std::u16string_view text) {
        const auto found=pool.find(text);
        if(found!=pool.end()) return found->second;
        const auto offset=bytes.size();
        bytes.resize(add(offset,text.size(),2));
        for(std::size_t i=0;i<text.size();++i) put(bytes,offset+2*i,text[i],2);
        pool.emplace(text,offset);
        return offset;
    };
    std::size_t recordAt=128,valueAt=valuesAt;
    for(std::size_t i=0;i<sections.size();++i) {
        put(bytes,32+i*16,i+1,4); put(bytes,36+i*16,narrow(sections[i].size()),4);
        put(bytes,40+i*16,recordAt,8);
        for(const auto& record:sections[i]) {
            put(bytes,recordAt,intern(record.key),8); put(bytes,recordAt+8,narrow(record.key.size()),4);
            put(bytes,recordAt+12,record.flags,4); put(bytes,recordAt+16,valueAt,8);
            put(bytes,recordAt+24,narrow(record.values.size()),4); recordAt+=32;
            for(auto value:record.values) {
                put(bytes,valueAt,intern(value),8); put(bytes,valueAt+8,narrow(value.size()),4); valueAt+=16;
            }
        }
    }
    put(bytes,16,bytes.size(),8);
    return bytes;
}
}
