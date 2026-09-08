#include "Lexicon.h"
#include "Text.h"
#include <fstream>
#include <iostream>
#include <sstream>

static std::u16string unhex(const std::string& hex) {
    if (hex.size()%4) throw std::runtime_error("Invalid UTF-16 hex");
    std::u16string result;
    for (std::size_t i=0;i<hex.size();i+=4) result+=static_cast<char16_t>(std::stoul(hex.substr(i,4),nullptr,16));
    return result;
}
static void json(std::u16string_view text) {
    constexpr char hex[]="0123456789abcdef";
    std::cout << '"';
    for (auto ch:text) std::cout << "\\u" << hex[(ch>>12)&15] << hex[(ch>>8)&15] << hex[(ch>>4)&15] << hex[ch&15];
    std::cout << '"';
}
int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("lexicon_probe <dictionary> <operations.tsv>");
        auto dictionary=tiger::Dictionary::Open(std::filesystem::u8path(argv[1]));
        std::shared_ptr<const tiger::Lexicon> lexicon=std::make_shared<tiger::Lexicon>(dictionary);
        const auto original=lexicon;
        std::ifstream file(std::filesystem::u8path(argv[2]));
        if(!file) throw std::runtime_error("Cannot read operations");
        for(std::string line;std::getline(file,line);) {
            std::istringstream input(line);
            int kind; std::string code,text;
            if(!(input>>kind>>code>>text)) throw std::runtime_error("Invalid operation");
            bool changed=false;
            lexicon=lexicon->changed({static_cast<tiger::ChangeKind>(kind),unhex(code),kind==0?tiger::parseEntry(unhex(text)):unhex(text)},&changed);
            if(original->editedCodes()!=0 || lexicon->dictionary()!=dictionary) throw std::runtime_error("Snapshot/base isolation failed");
            std::cout << "{\"changed\":" << (changed?"true":"false") << ",\"quick\":" << lexicon->quickSymbols() << ",\"entries\":[";
            bool first=true;
            for(std::string query;input>>query;) {
                auto key=unhex(query); auto match=lexicon->find(tiger::Section::Main,key);
                if(!first) std::cout << ',';
                first=false;
                std::cout << "{\"key\":"; json(key);
                std::cout << ",\"flags\":" << (match.flags&7) << ",\"values\":[";
                for(unsigned i=0;i<match.count;++i) { if(i) std::cout << ','; json(lexicon->value(match,i)); }
                std::cout << "]}";
            }
            std::cout << "]}\n";
        }
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
