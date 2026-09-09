#pragma once
#include <filesystem>

namespace tiger {
// Cache identity only: opening the file remains the authority for access and
// existence. Windows canonical() can require access beyond the file itself
// and fails in AppContainer even for readable Program Files dictionaries.
// Different aliases may occupy separate weak cache entries; their mapped
// file pages are still shared by Windows.
inline std::filesystem::path fileCachePath(const std::filesystem::path& path) {
#ifdef _WIN32
    return std::filesystem::absolute(path).lexically_normal();
#else
    return std::filesystem::canonical(path);
#endif
}
}
