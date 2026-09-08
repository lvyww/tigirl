#include "LexiconFile.h"
#include "LexiconDecode.h"
namespace tiger {
ParsedLexiconRows parseLexiconFile(const std::filesystem::path& path) {
    auto name=path.filename().u16string();
    // Only the ASCII suffix is compared, using the original case-insensitive rule.
    for(auto& c:name) if(c>=u'A' && c<=u'Z') c=static_cast<char16_t>(c+32);
    constexpr std::u16string_view suffix=u".dict.yaml";
    const bool yaml=name.size()>=suffix.size() &&
        std::u16string_view(name).substr(name.size()-suffix.size())==suffix;
    return parseLexiconRows(readLexiconText(path),yaml);
}
}
