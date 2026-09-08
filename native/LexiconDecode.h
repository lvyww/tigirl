#pragma once
#include <string>
#include <string_view>
#include <filesystem>

namespace tiger {
// Original .NET 10 DetectTextEncoding + StreamReader behavior, including BOM
// consumption and replacement fallback. No locale-codepage guessing.
std::u16string decodeLexiconBytes(std::string_view bytes);
// Reads one file without modifying it. I/O failures throw; an unreadable table
// must not silently become an empty successful import.
std::u16string readLexiconText(const std::filesystem::path& path);
}
