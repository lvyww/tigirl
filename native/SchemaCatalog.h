#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace tiger {
bool validSchemaGeneration(std::u16string_view generation);
std::filesystem::path schemaGenerationPath(const std::filesystem::path& directory,std::u16string_view generation);
std::vector<std::u16string> schemaGenerations(const std::filesystem::path& directory);
std::filesystem::path schemaDictionaryPath(const std::filesystem::path& schemaDirectory);
// Built-in name may use a user-prepared override; keep its existing journal.
std::filesystem::path activeSchemaDictionaryPath(const std::filesystem::path& userRoot,
    const std::filesystem::path& bundled,std::u16string_view name);
std::filesystem::path schemaJournalPath(const std::filesystem::path& userRoot,std::u16string_view name);
// Enumerate source directories; fall back to the bundled schema if none exist.
std::vector<std::u16string> schemaNames(const std::filesystem::path& userRoot);
std::u16string recentSchemaName(std::u16string_view configuration,const std::vector<std::u16string>& schemas);
}
