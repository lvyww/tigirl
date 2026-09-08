#pragma once
#include "LexiconImport.h"

namespace tiger {
// Import utility only: serialize all six sections into the immutable v2 format.
// The publisher must create a new generation, never overwrite a mapped file.
// Temporary serialization storage is not retained by the TSF reader.
std::vector<std::uint8_t> serializeImportedLexicon(const ImportedLexicon& lexicon);
}
