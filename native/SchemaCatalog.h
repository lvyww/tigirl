#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace tiger {
bool validSchemaGeneration(std::u16string_view generation);
std::filesystem::path schemaGenerationPath(const std::filesystem::path& directory,std::u16string_view generation);
std::vector<std::u16string> schemaGenerations(const std::filesystem::path& directory);
std::filesystem::path schemaDictionaryPath(const std::filesystem::path& schemaDirectory);
// Only prepared native schemes are selectable; the bundled schema is implicit.
std::vector<std::u16string> schemaNames(const std::filesystem::path& userRoot);
std::u16string recentSchemaName(std::u16string_view configuration,const std::vector<std::u16string>& schemas);
}
