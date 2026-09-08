#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <cstdint>
namespace tiger {
struct CodedLexiconRow { std::u16string code,text; int frequency=0; };
struct UncodedLexiconRow { std::u16string text; int frequency=0; };
struct ParsedLexiconRows {
    std::vector<CodedLexiconRow> coded;
    std::vector<UncodedLexiconRow> uncoded;
};
// Import-time parser. Input is decoded text; keep encounter order for the later
// stable frequency sort. YAML input starts only after an exact "..." header line.
ParsedLexiconRows parseLexiconRows(std::u16string_view text,bool yaml);
struct ImportedLexiconEntry { std::u16string code;std::vector<std::u16string> candidates; };
// Stable descending frequency, invariant code normalization and ordinal
// case-insensitive code identity; packed candidates deduplicate exactly. Entries
// retain first-insertion order for subsequent auxiliary-map construction.
std::vector<ImportedLexiconEntry> mergeCodedLexiconRows(const std::vector<CodedLexiconRow>& rows);
struct ImportedIndexEntry {
    std::u16string code;
    std::uint32_t flags=0;
    std::size_t sourceIndex=static_cast<std::size_t>(-1);
};
struct ImportedLexicon {
    std::vector<ImportedLexiconEntry> main;
    std::map<std::u16string,std::u16string> construct,fullCodes;
    std::vector<ImportedLexiconEntry> pinyin;
    std::map<std::u16string,std::u16string> comments,splits;
    std::vector<ImportedIndexEntry> indexedMain;
    std::uint32_t quickSymbols=0;
};
// The caller excludes the dedicated construction file from ordinary main rows.
// Its coded entries override inferred construction codes; its uncoded entries
// join ordinary uncoded rows before the second stable merge.
ImportedLexicon buildImportedLexicon(ParsedLexiconRows rows,const ParsedLexiconRows* construction=nullptr,
    std::u16string_view adjustments={});
void applyImportedAdjustments(std::vector<ImportedLexiconEntry>& main,std::u16string_view adjustments);
// Invoke in source-file encounter order: comments concatenate, splits overwrite.
void appendImportedAnnotations(std::map<std::u16string,std::u16string>& target,
    std::u16string_view text,bool splitMap);
}
