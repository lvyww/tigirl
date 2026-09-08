#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace tiger {
struct MixedSegment { std::u16string code, candidate; };
struct MixedResult {
    std::u16string raw, prefix, active, surface;
    std::vector<MixedSegment> segments;
    std::u16string compose(std::u16string_view output, std::u16string_view suffix={}) const {
        return prefix+std::u16string(output)+std::u16string(suffix);
    }
};
// Codes entering ordinary mixed input are ASCII letters/short-symbol starters.
// The callback returns the first candidate's output (empty means no candidate).
// Pass it per decode, rather than retaining a callback into an Engine: copying
// an engine for TestKeyDown must never resolve through the original instance.
class MixedDecoder {
public:
    using Preferred = std::map<std::size_t,std::u16string>;
    using Resolver = std::function<std::u16string(std::u16string_view)>;
    void clear() { cache_.clear(); haveVersion_=false; }
    MixedResult decode(std::u16string_view raw, int maximum, std::uint64_t version,
                       const Resolver& resolve, const Preferred& preferred={}) {
        if(!haveVersion_ || version_!=version) {
            cache_.clear(); version_=version; haveVersion_=true;
        }
        MixedResult result; result.raw=raw;
        if(raw.empty()) return result;
        const auto length=static_cast<std::size_t>(std::max(1,maximum));
        const auto completed=(raw.size()-1)/length;
        result.segments.reserve(completed);
        for(std::size_t i=0;i<completed;++i) {
            const auto start=i*length;
            MixedSegment segment{std::u16string(raw.substr(start,length)),{}};
            if(auto found=preferred.find(start);found!=preferred.end()) segment.candidate=found->second;
            if(segment.candidate.empty()) {
                auto key=segment.code;
                for(auto& c:key) if(c>=u'A' && c<=u'Z') c+=u'a'-u'A';
                auto found=cache_.find(key);
                if(found==cache_.end()) found=cache_.emplace(std::move(key),resolve(segment.code)).first;
                segment.candidate=found->second;
            }
            result.prefix+=segment.candidate.empty()?segment.code:segment.candidate;
            result.segments.push_back(std::move(segment));
        }
        result.active=raw.substr(completed*length);
        result.surface=result.prefix+result.active;
        return result;
    }
private:
    std::map<std::u16string,std::u16string> cache_;
    std::uint64_t version_=0;
    bool haveVersion_=false;
};
}
