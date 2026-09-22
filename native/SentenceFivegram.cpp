#include "SentenceFivegram.h"
#include "Text.h"
#include "FileCachePath.h"
#ifdef _MSC_VER
#pragma warning(push, 0) // Keep third-party header warnings out of Tigirl /WX.
#endif
#include "lm/model.hh"
#include "lm/binary_format.hh"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
namespace tiger {
struct SentenceFivegram::Data {
    std::unique_ptr<lm::base::Model> model;
    std::uint64_t bytes=0;
    explicit Data(const std::filesystem::path& path) {
        bytes=std::filesystem::file_size(path);
        lm::ngram::ModelType type;
        if(!lm::ngram::RecognizeBinary(path.u8string().c_str(),type))
            throw std::runtime_error("Fivegram requires a KenLM binary model");
        lm::ngram::Config config;config.load_method=util::LAZY;config.show_progress=false;config.messages=nullptr;
        model.reset(lm::ngram::LoadVirtual(path.u8string().c_str(),config));
        if(model->Order()!=5)throw std::runtime_error("Expected a fivegram model");
    }
};
struct SentenceFivegram::Cache {
    struct Hash {
        std::size_t operator()(const std::array<std::uint32_t,6>& key) const {
            std::size_t h=2166136261u;for(auto n:key){h^=n;h*=16777619u;}return h;
        }
    };
    std::mutex mutex;
    std::unordered_map<std::u16string,std::uint32_t> tokens;
    std::unordered_map<std::array<std::uint32_t,6>,double,Hash> scores;
    std::unordered_map<std::uint64_t,bool> observed;
    std::uint32_t token(const Data& data,std::u16string_view text) {
        const std::u16string key(text);auto it=tokens.find(key);if(it!=tokens.end())return it->second;
        std::uint32_t value;
        try{value=data.model->BaseVocabulary().Index(text==u"\x03"?"</s>":utf8(text));}
        catch(const std::invalid_argument&){value=data.model->BaseVocabulary().NotFound();}
        if(tokens.size()>=32768)tokens.clear();tokens.emplace(key,value);return value;
    }
};
SentenceFivegram::SentenceFivegram(std::shared_ptr<const Data> data):data_(std::move(data)),cache_(std::make_unique<Cache>()){}
SentenceFivegram::~SentenceFivegram()=default;
std::shared_ptr<const SentenceFivegram> SentenceFivegram::Open(const std::filesystem::path& path) {
    static std::mutex mutex;
    // Include file generation metadata so replacing a model never reuses stale token IDs.
    using Key=std::tuple<std::filesystem::path,std::uintmax_t,std::filesystem::file_time_type>;
    static std::map<Key,std::weak_ptr<const SentenceFivegram>> models;
    const Key key{fileCachePath(path),std::filesystem::file_size(path),std::filesystem::last_write_time(path)};
    std::lock_guard<std::mutex> lock(mutex);
    for(auto it=models.begin();it!=models.end();)if(it->second.expired())it=models.erase(it);else ++it;
    if(auto existing=models[key].lock())return existing;
    auto model=std::shared_ptr<SentenceFivegram>(new SentenceFivegram(std::make_shared<Data>(std::get<0>(key))));
    auto history=model->beginHistory();model->step(history,u"\x03");
    models[key]=model;return model;
}
std::shared_ptr<const SentenceLanguageModel> SentenceFivegram::querySession() const {
    return std::shared_ptr<SentenceFivegram>(new SentenceFivegram(data_));
}
SentenceLmHistory SentenceFivegram::beginHistory() const {
    return {{{data_->model->BaseVocabulary().BeginSentence(),0,0,0}},1};
}
double SentenceFivegram::step(SentenceLmHistory& history,std::u16string_view target) const {
    if(history.count>4)throw std::invalid_argument("Invalid fivegram history");
    std::lock_guard<std::mutex> lock(cache_->mutex);
    const auto token=cache_->token(*data_,target);
    const std::array<std::uint32_t,6> key{history.tokens[0],history.tokens[1],history.tokens[2],history.tokens[3],history.count,token};
    auto found=cache_->scores.find(key);double score;
    if(found!=cache_->scores.end())score=found->second;
    else {
        lm::ngram::State next;
        score=data_->model->BaseFullScoreForgotState(history.tokens.data(),history.tokens.data()+history.count,token,&next).prob*std::log(10.0);
        if(!std::isfinite(score))throw std::runtime_error("Non-finite fivegram score");
        if(cache_->scores.size()>=8192)cache_->scores.clear();cache_->scores.emplace(key,score);
    }
    for(std::size_t i=3;i>0;--i)history.tokens[i]=history.tokens[i-1];
    history.tokens[0]=token;history.count=std::min<std::uint32_t>(4,history.count+1);return score;
}
double SentenceFivegram::logProbability(std::u16string_view,std::u16string_view,std::u16string_view,bool) const {
    throw std::logic_error("Fivegram scoring requires full history");
}
bool SentenceFivegram::hasObservedBigram(std::u16string_view previous,std::u16string_view target) const {
    if(previous.empty() || target.empty())return false;
    std::lock_guard<std::mutex> lock(cache_->mutex);
    const auto a=cache_->token(*data_,previous),b=cache_->token(*data_,target);
    if(a==data_->model->BaseVocabulary().NotFound() || b==data_->model->BaseVocabulary().NotFound())return false;
    const auto key=(static_cast<std::uint64_t>(a)<<32)|b;
    auto it=cache_->observed.find(key);if(it!=cache_->observed.end())return it->second;
    lm::ngram::State next;
    const bool observed=data_->model->BaseFullScoreForgotState(&a,&a+1,b,&next).ngram_length==2;
    if(cache_->observed.size()>=8192)cache_->observed.clear();cache_->observed.emplace(key,observed);return observed;
}
std::uint64_t SentenceFivegram::mappedBytes() const{return data_->bytes;}
}
