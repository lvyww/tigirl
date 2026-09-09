#define NOMINMAX
#include <windows.h>
#include "PrivateFonts.h"
#include "../FileCachePath.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <cwctype>

namespace tiger::tsf {
namespace { constexpr DWORD flags=FR_PRIVATE|FR_NOT_ENUM; }
std::shared_ptr<PrivateFonts> PrivateFonts::Open(const std::filesystem::path& directory) {
    static std::mutex mutex;
    static std::map<std::filesystem::path,std::weak_ptr<PrivateFonts>> cache;
    const auto path=fileCachePath(directory);
    std::lock_guard<std::mutex> lock(mutex);
    for(auto it=cache.begin();it!=cache.end();) { if(it->second.expired()) it=cache.erase(it); else ++it; }
    if(auto found=cache.find(path);found!=cache.end()) if(auto fonts=found->second.lock()) return fonts;
    auto fonts=std::shared_ptr<PrivateFonts>(new PrivateFonts);
    fonts->load(path);
    cache[path]=fonts;
    return fonts;
}
void PrivateFonts::load(const std::filesystem::path& directory) {
    if(!std::filesystem::exists(directory)) return;
    std::vector<std::filesystem::path> paths;
    for(const auto& item:std::filesystem::directory_iterator(directory)) {
        if(!item.is_regular_file()) continue;
        auto extension=item.path().extension().wstring();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](wchar_t c){ return static_cast<wchar_t>(std::towlower(c)); });
        if(extension==L".ttf" || extension==L".otf" || extension==L".ttc") paths.push_back(item.path());
    }
    std::sort(paths.begin(),paths.end());
    for(const auto& path:paths) {
        // Allocate the bookkeeping entry before acquiring the GDI resource.
        files_.push_back(path);
        const int faces=AddFontResourceExW(path.c_str(),flags,nullptr);
        if(faces>0) faces_+=static_cast<unsigned>(faces); else files_.pop_back();
    }
}
PrivateFonts::~PrivateFonts() {
    for(auto it=files_.rbegin();it!=files_.rend();++it) RemoveFontResourceExW(it->c_str(),flags,nullptr);
}
}
