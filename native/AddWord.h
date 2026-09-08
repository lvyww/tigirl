#pragma once
#include "Dictionary.h"
#include "Grapheme.h"
#include "Lexicon.h"
#include <string>
#include <vector>
#include <functional>
namespace tiger {
std::u16string constructWordCode(const Dictionary& dictionary,std::u16string_view word);
std::u16string constructWordCode(std::u16string_view word,
    const std::function<std::u16string_view(std::u16string_view)>& lookup);
// Same entry decoding as the original manual-add API (trim, escapes, alias).
std::u16string parseAddedWord(std::u16string_view text);
std::u16string decodeLexiconEscapes(std::u16string_view text);
std::u16string normalizeAddedCode(std::u16string_view code);
UserChange prepareAddedWord(std::u16string_view code,std::u16string_view text);
}
