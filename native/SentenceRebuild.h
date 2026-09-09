#pragma once
#include "SentenceLexicon.h"
#include "Lexicon.h"
namespace tiger {
// Import/helper-process operation only: constructs transient full metadata.
// sourceInventory must belong to snapshot.dictionary()'s base generation.
// Publish the returned value as a new immutable sidecar; never replace a mapped
// generation or run this full-table rebuild in each TSF host.
ImportedLexicon rebuildSentenceLexicon(const SentenceLexicon& sourceInventory,
    const Lexicon& snapshot);
}
