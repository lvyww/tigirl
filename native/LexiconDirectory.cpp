#include "LexiconDirectory.h"
#include "LexiconFile.h"
#include "LexiconDecode.h"
#include "LexiconOrder.h"
#include "OrdinalCase.h"
#include <iterator>
namespace tiger {
namespace {
bool hasSuffix(const std::filesystem::path& file,std::u16string_view suffix) {
    const auto name=file.filename().u16string();
    return name.size()>=suffix.size() && ordinalCompareIgnoreCase(
        std::u16string_view(name).substr(name.size()-suffix.size()),suffix)==0;
}
}
ImportedLexicon importLexiconDirectory(const std::filesystem::path& directory,
    const std::filesystem::path& pinyinDirectory,const std::string& culture) {
    auto imported=importMainDirectory(directory,culture);
    std::vector<CodedLexiconRow> pinyinRows;
    if(!pinyinDirectory.empty() && std::filesystem::is_directory(pinyinDirectory)) {
        for(const auto& file:enumerateLexiconFiles(pinyinDirectory)) {
            if(!hasSuffix(file,u".txt")) continue;
            auto parsed=parseLexiconFile(file);
            pinyinRows.insert(pinyinRows.end(),std::make_move_iterator(parsed.coded.begin()),std::make_move_iterator(parsed.coded.end()));
        }
    }
    imported.pinyin=mergeCodedLexiconRows(pinyinRows);
    for(const auto& file:enumerateLexiconFiles(directory)) {
        if(hasSuffix(file,u".注释")) appendImportedAnnotations(imported.comments,readLexiconText(file),false);
        else if(hasSuffix(file,u".拆分")) appendImportedAnnotations(imported.splits,readLexiconText(file),true);
    }
    return imported;
}
ImportedLexicon importMainDirectory(const std::filesystem::path& directory,const std::string& culture) {
    const auto files=orderedLexiconFiles(directory,culture);
    const auto constructPath=directory/std::filesystem::path(u"构词.txt");
    const bool hasConstruct=std::filesystem::is_regular_file(constructPath);
    ParsedLexiconRows rows,construction;
    for(const auto& file:files) {
        const auto name=file.filename().u16string();
        if(ordinalCompareIgnoreCase(name,u"补充语料.txt")==0) continue;
        if(hasConstruct && ordinalCompareIgnoreCase(name,u"构词.txt")==0) continue;
        auto parsed=parseLexiconFile(file);
        rows.coded.insert(rows.coded.end(),std::make_move_iterator(parsed.coded.begin()),std::make_move_iterator(parsed.coded.end()));
        rows.uncoded.insert(rows.uncoded.end(),std::make_move_iterator(parsed.uncoded.begin()),std::make_move_iterator(parsed.uncoded.end()));
    }
    if(hasConstruct) construction=parseLexiconFile(constructPath);
    const auto adjustmentPath=directory/std::filesystem::path(u"用户调整.txt");
    const auto adjustments=std::filesystem::is_regular_file(adjustmentPath)?readLexiconText(adjustmentPath):std::u16string{};
    return buildImportedLexicon(std::move(rows),hasConstruct?&construction:nullptr,adjustments);
}
}
