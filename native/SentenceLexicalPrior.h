#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace tiger {
// Immutable reader for the compact TCSLEX01 word-existence Bloom filter.
// It is loaded on the sentence resource worker and queried only for Top-5
// candidates after Beam search has completed.
class SentenceLexicalPrior {
public:
    static std::shared_ptr<const SentenceLexicalPrior> Open(const std::filesystem::path& path) {
        std::ifstream input(path,std::ios::binary);
        if(!input)return {};
        input.seekg(0,std::ios::end);const auto end=input.tellg();
        if(end<0 || end>static_cast<std::streamoff>(64*1024*1024))throw std::runtime_error("Invalid TCSLEX01 size");
        input.seekg(0,std::ios::beg);std::vector<unsigned char> bytes(static_cast<std::size_t>(end));
        if(!bytes.empty())input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        if(!input && !bytes.empty())throw std::runtime_error("Cannot read TCSLEX01 lexical prior");
        return Parse(std::move(bytes));
    }

    bool contains(std::u16string_view text,std::map<std::u16string,bool,std::less<>>* cache=nullptr) const {
        if(text.empty())return false;
        if(cache)if(auto found=cache->find(text);found!=cache->end())return found->second;
        const auto bytes=utf8(text);std::uint64_t first=2166136261ull,second=16777619ull;
        for(unsigned char value:bytes){first=(first*131+value+17)%modulus;second=(second*137+value+53)%modulus;}
        if(!second)second=1;bool found=true;
        for(std::uint32_t index=0;index<hashCount_;++index) {
            const auto bit=(first+index*second+static_cast<std::uint64_t>(index)*index*97)%bitCount_;
            if(!(bits_[static_cast<std::size_t>(bit/8)]&(1u<<(bit%8)))){found=false;break;}
        }
        if(cache)(*cache)[std::u16string(text)]=found;return found;
    }

    double score(std::u16string_view text,std::map<std::u16string,bool,std::less<>>* cache=nullptr) const {
        const auto elements=codepoints(text);if(elements.empty())return 0;
        std::vector<double> best(elements.size()+1);std::u16string word;
        for(std::size_t finish=1;finish<=elements.size();++finish) {
            best[finish]=best[finish-1];
            for(std::uint32_t length=minimumLength_;length<=maximumLength_;++length) {
                if(length>finish)break;const auto start=finish-length;word.clear();
                for(std::size_t i=start;i<finish;++i)word.append(elements[i]);
                if(!contains(word,cache))continue;
                best[finish]=std::max(best[finish],best[start]+1.0+0.2*(length-2));
            }
        }
        return best.back();
    }

    std::uint32_t entryCount() const{return entryCount_;}
    std::uint32_t bitCount() const{return bitCount_;}
    std::uint32_t hashCount() const{return hashCount_;}
    std::uint32_t minimumLength() const{return minimumLength_;}
    std::uint32_t maximumLength() const{return maximumLength_;}
    std::size_t byteCount() const{return headerSize+bits_.size();}

private:
    static constexpr std::size_t headerSize=32;
    static constexpr std::uint64_t modulus=4294967291ull;
    std::vector<unsigned char> bits_;
    std::uint32_t entryCount_=0,bitCount_=0,hashCount_=0,minimumLength_=0,maximumLength_=0;

    static std::uint32_t integer(const std::vector<unsigned char>& bytes,std::size_t offset) {
        return static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset+1])<<8) |
            (static_cast<std::uint32_t>(bytes[offset+2])<<16) |
            (static_cast<std::uint32_t>(bytes[offset+3])<<24);
    }
    static std::shared_ptr<const SentenceLexicalPrior> Parse(std::vector<unsigned char> bytes) {
        if(bytes.size()<headerSize || std::memcmp(bytes.data(),"TCSLEX01",8))throw std::runtime_error("Not a TCSLEX01 lexical prior");
        const auto version=integer(bytes,8),entries=integer(bytes,12),bits=integer(bytes,16),hashes=integer(bytes,20);
        const auto minimum=integer(bytes,24),maximum=integer(bytes,28);
        if(version!=1 || !entries || bits<8 || bits%8 || !hashes || hashes>32 || minimum<2 || maximum<minimum || maximum>16 ||
           bytes.size()!=headerSize+static_cast<std::size_t>(bits/8))throw std::runtime_error("Invalid TCSLEX01 lexical prior header");
        auto result=std::shared_ptr<SentenceLexicalPrior>(new SentenceLexicalPrior);
        result->entryCount_=entries;result->bitCount_=bits;result->hashCount_=hashes;
        result->minimumLength_=minimum;result->maximumLength_=maximum;
        result->bits_.assign(bytes.begin()+headerSize,bytes.end());return result;
    }
    static std::vector<std::u16string_view> codepoints(std::u16string_view text) {
        std::vector<std::u16string_view> result;result.reserve(text.size());
        for(std::size_t i=0;i<text.size();) {
            std::size_t count=1;const auto first=text[i];
            if(first>=0xd800 && first<=0xdbff && i+1<text.size() && text[i+1]>=0xdc00 && text[i+1]<=0xdfff)count=2;
            result.push_back(text.substr(i,count));i+=count;
        }
        return result;
    }
    static std::string utf8(std::u16string_view text) {
        std::string result;result.reserve(text.size()*3);
        for(std::size_t i=0;i<text.size();++i) {
            std::uint32_t value=text[i];
            if(value>=0xd800 && value<=0xdbff) {
                if(i+1>=text.size() || text[i+1]<0xdc00 || text[i+1]>0xdfff)throw std::runtime_error("Invalid UTF-16 lexical query");
                value=0x10000+((value-0xd800)<<10)+(text[++i]-0xdc00);
            }else if(value>=0xdc00 && value<=0xdfff)throw std::runtime_error("Invalid UTF-16 lexical query");
            if(value<0x80)result.push_back(static_cast<char>(value));
            else if(value<0x800){result.push_back(static_cast<char>(0xc0|(value>>6)));result.push_back(static_cast<char>(0x80|(value&63)));}
            else if(value<0x10000){result.push_back(static_cast<char>(0xe0|(value>>12)));result.push_back(static_cast<char>(0x80|((value>>6)&63)));result.push_back(static_cast<char>(0x80|(value&63)));}
            else {result.push_back(static_cast<char>(0xf0|(value>>18)));result.push_back(static_cast<char>(0x80|((value>>12)&63)));result.push_back(static_cast<char>(0x80|((value>>6)&63)));result.push_back(static_cast<char>(0x80|(value&63)));}
        }
        return result;
    }
};
}
