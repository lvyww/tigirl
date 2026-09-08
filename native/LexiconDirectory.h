#pragma once
#include "LexiconImport.h"
#include <filesystem>
namespace tiger {
// Main, construction, inferred phrases, adjustments and derived main metadata.
// Auxiliary pinyin/comments/splits are filled by the complete importer later.
ImportedLexicon importMainDirectory(const std::filesystem::path& directory,const std::string& culture);
ImportedLexicon importLexiconDirectory(const std::filesystem::path& directory,
    const std::filesystem::path& pinyinDirectory,const std::string& culture);
}
