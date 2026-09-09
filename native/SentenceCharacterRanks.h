#pragma once
#include "SentenceLexicon.h"
namespace tiger {
int sentenceCharacterRank(std::u16string_view text);
SentenceLexicon::Characters sentenceTopCharacters(int count);
}
