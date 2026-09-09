#include "Engine.h"
#include "SelectionKeys.h"
#include "Settings.h"
#include "SentenceResources.h"
#include "SentenceSettings.h"
#include "Text.h"
#include <iostream>
#include <sstream>
#include <fstream>

static void json(std::u16string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    std::cout << '"';
    for (const auto ch : text) std::cout << "\\u" << hex[(ch>>12)&15] << hex[(ch>>8)&15] << hex[(ch>>4)&15] << hex[ch&15];
    std::cout << '"';
}
int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    try {
        if (argc < 6 || argc > 8) throw std::runtime_error("sentence_trace_probe <dictionary> <trace.tsv> <selection.txt> <config.txt> <model>");
        std::ifstream trace;
        if (argc >= 3) {
            trace.open(std::filesystem::u8path(argv[2]));
            if (!trace) throw std::runtime_error("Cannot open trace");
        }
        auto& source = argc >= 3 ? static_cast<std::istream&>(trace) : std::cin;
        auto dictionary = tiger::Dictionary::Open(std::filesystem::u8path(argv[1]));
        tiger::Config config;
        if(argc>=5) config=tiger::loadEngineSettings(std::filesystem::u8path(argv[4]));
        if(argc>=4) config.selection=tiger::SelectionKeys::load(std::filesystem::u8path(argv[3])).dispatch();
        const bool automatic=argc>=7;
        const int retained=argc==8?std::stoi(argv[7]):3;
        tiger::Engine engine(dictionary,config);
        const auto sentence=tiger::parseSentenceSettings(tiger::readUnicodeFile(std::filesystem::u8path(argv[4])));
        tiger::SentenceDecoderOptions options;options.emittedCharacterReward=2;options.wholeInputSingleCharacterReward=5;options.allowDuplicateSingleCharacters=sentence.allowDuplicateSingleCharacters;
        auto resources=tiger::SentenceResources::Open(std::filesystem::u8path(argv[1]),std::filesystem::u8path(argv[5]),sentence.commonCharacterLimit,sentence.whitelist(),options);
        auto decoder=resources->createDecoder();engine.enableSentenceInput(true,1,automatic,retained);
        auto complete=[&] {if(auto ticket=engine.sentenceRequest())engine.applySentenceResult(*ticket,decoder->decode(ticket->raw,20,automatic,ticket->requiredPrefix,ticket->lockedPrefix));};
        for (std::string line; std::getline(source, line);) {
            if (line.empty()) continue;
            std::istringstream input(line);
            tiger::KeyEvent key;
            bool reset = false;
            if (!(input >> reset >> key.vk >> key.scan >> key.down >> key.shift >> key.ctrl >> key.alt >> key.win >> key.caps >> key.num >> key.repeat >> key.extended))
                throw std::runtime_error("Invalid key trace");
            bool settle=true;input>>settle;
            if (reset) {engine = tiger::Engine(engine.lexicon(),config);engine.enableSentenceInput(true,1,automatic,retained);decoder=resources->createDecoder();}
            tiger::SentencePathQueries queries;
            queries.complete=[&](std::u16string_view raw,std::u16string_view prefix,std::optional<std::u16string_view> excluded,bool grouped,const SentenceLockedPrefix* locked){return decoder->hasCompleteCandidate(raw,prefix,excluded,grouped,locked);};
            queries.properPrefix=[&](std::u16string_view raw){return decoder->isProperCodePrefix(raw);};
            auto result = engine.process(key,queries);if(result.awaitSentenceDecode){complete();result=engine.process(key,queries);}if(settle)complete();
            if(automatic && std::string(argv[6])=="--legacy-auto")result.commit+=engine.autoCommitSentence().commit;
            const auto snapshot = engine.snapshot();
            engine.takeUserChanges();
            std::cout << "{\"handled\":" << (result.handled ? "true" : "false")
                      << ",\"cancel\":" << (result.cancelComposition ? "true" : "false")
                      << ",\"chinese\":" << (snapshot.chinese ? "true" : "false")
                      << ",\"mode\":" << static_cast<int>(snapshot.mode)
                      << ",\"page\":" << snapshot.page << ",\"raw\":";
            auto request=engine.sentenceRequest();json(request?request->raw:snapshot.raw); std::cout << ",\"commit\":"; json(result.commit);
            std::cout << ",\"candidates\":[";
            for (std::size_t i=0; i<snapshot.candidates.size(); ++i) { if(i) std::cout << ','; json(snapshot.candidates[i].display); }
            std::cout << "],\"annotations\":[";
            for (std::size_t i=0; i<snapshot.candidates.size(); ++i) { if(i) std::cout << ','; json(snapshot.candidates[i].annotation); }
            std::cout << "]";
            if(result.openAddWord) std::cout<<",\"openAddWord\":true";
            std::cout<<",\"selected\":"<<snapshot.selectedCandidate<<",\"surface\":";json(snapshot.compositionText());
            std::cout<<",\"committed\":";json(request?request->requiredPrefix:std::u16string{});
            std::cout<<",\"committedRaw\":"<<(request?request->raw.size()-snapshot.raw.size():0)<<"}\n";
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
