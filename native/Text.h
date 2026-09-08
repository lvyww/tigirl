#pragma once
#include <string>
#include <string_view>
#include <filesystem>

namespace tiger {
std::u16string readUnicodeFile(const std::filesystem::path& path);
// Same bounded file read as readUnicodeFile, before Unicode decoding.
std::string readTextFileBytes(const std::filesystem::path& path);
std::u16string decodeUnicodeText(std::string_view bytes);
std::u16string normalizeCode(std::u16string_view text);
std::u16string unpackText(std::u16string_view text);
std::u16string packText(std::u16string_view text);
std::u16string parseEntry(std::u16string_view text);
std::u16string_view commitText(std::u16string_view entry);
std::u16string_view displayText(std::u16string_view entry);
std::string utf8(std::u16string_view text);
std::u16string utf16(std::string_view text);
}
