#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace tiger {
// Unsorted top-level files, preserving Windows enumeration order.
std::vector<std::filesystem::path> enumerateLexiconFiles(const std::filesystem::path& directory);
// Explicit culture is the importer's current culture (e.g. zh-CN); empty is
// invariant. Return top-level .txt and .dict.yaml files in original load order.
std::vector<std::filesystem::path> orderedLexiconFiles(
    const std::filesystem::path& directory,const std::string& culture);
}
