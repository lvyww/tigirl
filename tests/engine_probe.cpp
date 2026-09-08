#include "Engine.h"
#include "SelectionKeys.h"
#include "Settings.h"
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
        if (argc < 2 || argc > 6 || (argc==6 && std::string(argv[5])!="--history")) throw std::runtime_error("engine_probe <dictionary> [trace.tsv] [selection.txt] [config.txt] [--history]");
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
        tiger::Engine engine(dictionary,config);
        for (std::string line; std::getline(source, line);) {
            if (line.empty()) continue;
            std::istringstream input(line);
            tiger::KeyEvent key;
            bool reset = false;
            if (!(input >> reset >> key.vk >> key.scan >> key.down >> key.shift >> key.ctrl >> key.alt >> key.win >> key.caps >> key.num >> key.repeat >> key.extended))
                throw std::runtime_error("Invalid key trace");
            if (reset) engine = tiger::Engine(engine.lexicon(),config);
            const auto result = engine.process(key);
            const auto snapshot = engine.snapshot();
            engine.takeUserChanges();
            std::cout << "{\"handled\":" << (result.handled ? "true" : "false")
                      << ",\"cancel\":" << (result.cancelComposition ? "true" : "false")
                      << ",\"chinese\":" << (snapshot.chinese ? "true" : "false")
                      << ",\"mode\":" << static_cast<int>(snapshot.mode)
                      << ",\"page\":" << snapshot.page << ",\"raw\":";
            json(snapshot.raw); std::cout << ",\"commit\":"; json(result.commit);
            std::cout << ",\"candidates\":[";
            for (std::size_t i=0; i<snapshot.candidates.size(); ++i) { if(i) std::cout << ','; json(snapshot.candidates[i].display); }
            std::cout << "],\"annotations\":[";
            for (std::size_t i=0; i<snapshot.candidates.size(); ++i) { if(i) std::cout << ','; json(snapshot.candidates[i].annotation); }
            std::cout << "]";
            if(config.mixedInput) { std::cout<<",\"surface\":"; json(snapshot.compositionText()); }
            if(argc==6) { std::cout<<",\"history\":"; json(engine.recentText()); }
            if(result.openAddWord) std::cout<<",\"openAddWord\":true";
            std::cout<<"}\n";
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
