#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

namespace tiger {
// Preserve even non-VK integers accepted by the original format for round trips.
// Only physical Windows virtual keys (0..255) enter the engine dispatch map.
struct SelectionKeys {
    std::array<std::vector<int>,10> bindings;
    SelectionKeys();
    static bool parse(std::u16string_view text,SelectionKeys& result,std::u16string& error);
    static SelectionKeys load(const std::filesystem::path& path);
    std::array<int,256> dispatch() const;
    std::u16string serialize() const;
};
}
