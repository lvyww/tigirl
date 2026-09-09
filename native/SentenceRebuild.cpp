#include "SentenceRebuild.h"
#include "OrdinalCase.h"
#include "Text.h"
#include <stdexcept>
namespace tiger {
ImportedLexicon rebuildSentenceLexicon(const SentenceLexicon& inventory,const Lexicon& snapshot) {
    std::vector<ImportedLexiconEntry> entries;
    std::set<std::u16string> exact,identities;
    auto append=[&](std::u16string_view code) {
        if(code.empty() || !exact.emplace(code).second || !identities.insert(ordinalCaseKey(code)).second)
            throw std::runtime_error("Duplicate or empty sentence source code");
        const auto match=snapshot.find(Section::Main,code);
        ImportedLexiconEntry entry{std::u16string(code),{}};
        for(std::uint32_t i=0;i<match.count;++i)entry.candidates.emplace_back(commitText(snapshot.value(match,i)));
        entries.push_back(std::move(entry));
    };
    const auto& base=*snapshot.dictionary();
    for(std::uint32_t i=0;i<inventory.sourceCodeCount();++i) {
        const auto code=inventory.sourceCode(i);const auto original=base.find(Section::Main,code);
        if(!(original.flags&8))throw std::runtime_error("Sentence source code absent from base generation");
        append(code);
    }
    for(std::uint32_t i=0;i<base.count(Section::Main);++i) {
        const auto entry=base.at(Section::Main,i);
        if((entry.flags&8) && !exact.count(std::u16string(entry.key)))
            throw std::runtime_error("Sentence inventory omits a base code");
    }
    for(const auto& code:snapshot.addedCodes())append(code);
    return prepareSentenceLexicon(entries);
}
}
