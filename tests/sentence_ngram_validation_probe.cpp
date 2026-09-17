#include "SentenceNgram.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using Narrow=std::vector<std::pair<std::uint32_t,float>>;
using Wide=std::vector<std::pair<std::uint64_t,float>>;
template<class T>void put(std::vector<unsigned char>& out,T value) {
    const auto* bytes=reinterpret_cast<const unsigned char*>(&value);out.insert(out.end(),bytes,bytes+sizeof(value));
}
void narrow(std::vector<unsigned char>& out,const Narrow& values) {
    put(out,static_cast<std::uint32_t>(values.size()));for(const auto& item:values){put(out,item.first);put(out,item.second);}
}
void wide(std::vector<unsigned char>& out,const Wide& values) {
    put(out,static_cast<std::uint64_t>(values.size()));for(const auto& item:values){put(out,item.first);put(out,item.second);}
}
void model(const std::filesystem::path& path,const Narrow& unigram,
    const Wide& bigram={},const Narrow& bigramBackoff={},const Wide& trigram={},const Wide& trigramBackoff={}) {
    std::vector<unsigned char> bytes{'T','C','S','K','N','M','0','1'};put(bytes,std::uint32_t{1});
    narrow(bytes,unigram);wide(bytes,bigram);narrow(bytes,bigramBackoff);wide(bytes,trigram);wide(bytes,trigramBackoff);
    std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!output)throw std::runtime_error("write model fixture");
}
void invalid(const std::filesystem::path& path,const Narrow& unigram,
    const Wide& bigram={},const Narrow& bigramBackoff={},const Wide& trigram={},const Wide& trigramBackoff={}) {
    model(path,unigram,bigram,bigramBackoff,trigram,trigramBackoff);
    bool rejected=false;
    try{auto loaded=tiger::SentenceNgram::Open(path);(void)loaded;}catch(const std::runtime_error&){rejected=true;}
    if(!rejected)throw std::runtime_error("invalid model accepted");
}
}

int main() {
    using namespace tiger;
    const auto root=std::filesystem::current_path();
    const auto valid=root/"ngram-valid.bin";
    model(valid,{{0,.1f},{0x4e2d,.2f}});
    auto loaded=SentenceNgram::Open(valid);
    if(!std::isfinite(loaded->logProbability(u"",u"",u"中",true)))throw std::runtime_error("valid model failed");

    invalid(root/"ngram-duplicate.bin",{{0,.1f},{0,.2f}});
    invalid(root/"ngram-unsorted.bin",{{0,.1f},{2,.2f},{1,.3f}});
    invalid(root/"ngram-nan.bin",{{0,.1f},{1,std::numeric_limits<float>::quiet_NaN()}});
    invalid(root/"ngram-negative.bin",{{0,.1f},{1,-.1f}});
    invalid(root/"ngram-infinite-backoff.bin",{{0,.1f}}, {},{{0,std::numeric_limits<float>::infinity()}});
    std::cout<<"PASS: sentence n-gram rejects unsorted and non-finite/negative records.\n";
    return 0;
}
