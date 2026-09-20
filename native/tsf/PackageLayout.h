#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <stdexcept>

namespace tiger::tsf {
inline std::filesystem::path packageResourceDirectory(HINSTANCE module) {
    wchar_t path[32768]{};
    const auto length=GetModuleFileNameW(module,path,32768);
    if(!length || length>=32768) throw std::runtime_error("Cannot locate Tigirl module");
    auto directory=std::filesystem::path(path).parent_path();
    const auto leaf=directory.filename().wstring();
    if(_wcsicmp(leaf.c_str(),L"x64")==0 || _wcsicmp(leaf.c_str(),L"x86")==0) {
        const auto shared=directory.parent_path()/L"shared";
        // A stray directory must not change an older side-by-side installation.
        // The bundled dictionary is the stable marker of the new shared layout.
        if(std::filesystem::is_regular_file(shared/L"tiger-v2.tcd")) return shared;
    }
    return directory;
}
}
