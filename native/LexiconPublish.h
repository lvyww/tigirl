#pragma once
#include "LexiconImport.h"
#include <filesystem>
namespace tiger {
// Validate and publish a new immutable generation. Existing destinations always
// fail, including concurrent publication. Never replace a mapped generation.
void publishImportedLexicon(const ImportedLexicon& lexicon,const std::filesystem::path& destination);
}
