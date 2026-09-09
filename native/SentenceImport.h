#pragma once
#include "SentenceSupplement.h"
#include "SentenceLexicon.h"
#include <filesystem>
namespace tiger {
std::vector<SentenceSupplementEntry> parseSentenceSupplements(std::u16string_view text);
inline std::filesystem::path sentenceLexiconPath(const std::filesystem::path& ordinary){auto path=ordinary;path+=L".sentence.tcd";return path;}
inline std::filesystem::path sentenceSupplementPath(const std::filesystem::path& ordinary){auto path=ordinary;path+=L".supplement.tcd";return path;}
// Prepare from source encounter order, unpacking ordinary display/commit aliases.
// Publish companions before the ordinary file/active generation descriptor.
void publishSentenceSidecars(const ImportedLexicon& ordinary,std::u16string_view supplements,
    const std::filesystem::path& dictionaryPath);
void validateSentenceSidecars(const std::filesystem::path& dictionaryPath);
}
