#pragma once
#include "Engine.h"
#include "CandidatePresentation.h"
#include <filesystem>
namespace tiger {
CandidateStyle parseCandidateStyle(std::u16string_view text);
Config parseEngineSettings(std::u16string_view text);
Config loadEngineSettings(const std::filesystem::path& path);
std::u16string currentSchemaSetting(std::u16string_view text);
bool validSchemaName(std::u16string_view name);
std::u16string configurationValue(std::u16string_view text,std::u16string_view key);
std::u16string withConfigurationValue(std::u16string_view text,std::u16string_view key,std::u16string_view value);
std::u16string withHiddenCandidateSetting(std::u16string_view text,bool hidden);
}
