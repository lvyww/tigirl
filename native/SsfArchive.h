#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>
namespace tiger::skin {
using Bytes=std::vector<std::uint8_t>;
using Files=std::map<std::u16string,Bytes>;
inline constexpr std::size_t maxInput=64u*1024u*1024u;
inline constexpr std::size_t maxExpanded=128u*1024u*1024u;
inline constexpr std::size_t maxAsset=64u*1024u*1024u;
// Canonical, relative archive path. Never used to extract a file to disk.
std::u16string archivePath(std::u16string_view);
bool validSkinFile(std::u16string_view) noexcept;
std::u16string decodeText(const Bytes&);
std::uint32_t crc32(const std::uint8_t*,std::size_t);
// Bounded Skin-v3 (AES-256-CBC/zlib) and ordinary ZIP SSF reader.
// Throws std::runtime_error on malformed/unsupported data; executes nothing.
Files decodeArchive(const Bytes&);
}
