#pragma once
#include "../SsfSkin.h"
#include "../SsfAnimation.h"
#include <windows.h>
#include <wincodec.h>
#include <filesystem>
#include <memory>
namespace tiger::skin {
struct Image {
    unsigned width=0,height=0;
    std::uint32_t plays=0;
    std::vector<std::vector<std::uint32_t>> frames; // premultiplied BGRA
    std::vector<std::uint32_t> delays;
};
struct Resources {
    Definition definition;
    std::map<std::u16string,Image> images;
    std::u16string error;
};
// Per-user root beside code tables; honors NATIVE_TIGER_USER_ROOT for isolation.
std::filesystem::path skinDirectory();
std::vector<std::u16string> skinFiles(const std::filesystem::path&);
// Shared immutable CPU pixels only: no COM objects outlive an apartment.
std::shared_ptr<const Resources> loadResources(const std::filesystem::path&,std::u16string_view,
    IWICImagingFactory*,std::u16string_view revision={});
}
