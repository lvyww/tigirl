// Absolute candidate selection regression tests. Only synthetic dictionaries.
#include "Engine.h"
#include "LexiconSerialize.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace tiger;
int checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
KeyResult press(Engine& engine, int vk) {
    KeyEvent key;
    key.vk = vk;
    return engine.process(key);
}
const std::vector<std::u16string> words = {
    u"甲", u"乙", u"丙", u"丁", u"戊", u"己", u"庚", u"辛", u"壬", u"癸", u"子", u"丑"
};
std::shared_ptr<const Dictionary> dictionary(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) throw std::runtime_error("Fixture already exists");
    ImportedLexicon imported;
    imported.main = {{u"a", words}, {u"b", {u"加词\x1e{添加}", u"隐藏\x1e{隐藏候选}", u"重复\x1e{重复上屏}"}}};
    imported.indexedMain = {{u"a", 8, 0}, {u"b", 8, 1}};
    imported.pinyin = {{u"a", words}};
    const auto bytes = serializeImportedLexicon(imported);
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        file.close();
        if (!file) throw std::runtime_error("Cannot write fixture");
    }
    return Dictionary::Open(path);
}
void checkInvalid(Engine source, std::uint32_t count) {
    const auto before = source.snapshot();
    const auto history = source.recentText();
    for (auto index : {count, std::numeric_limits<std::uint32_t>::max()}) {
        check(!source.selectCandidate(index), "Out-of-range candidate accepted");
        const auto after = source.snapshot();
        check(after.raw == before.raw && after.mode == before.mode &&
              after.page == before.page && after.total == before.total &&
              source.recentText() == history, "Invalid selection changed input state");
    }
}
void checkAll(const Engine& source, const std::vector<std::u16string>& expected,
              std::u16string_view prefix = {}) {
    checkInvalid(source, static_cast<std::uint32_t>(expected.size()));
    for (std::uint32_t i = 0; i < expected.size(); ++i) {
        auto next = source;
        check(next.candidateAt(i).commit == expected[i], "Published candidate mismatch");
        const auto result = next.selectCandidate(i);
        const auto text = std::u16string(prefix) + expected[i];
        check(result && result->handled && result->commit == text, "Wrong absolute candidate committed");
        check(!next.composing(), "Selection did not finish composition");
        check(next.recentText() == text, "Selection did not update history exactly once");
        check(source.composing() && source.recentText().empty(), "Selection mutated source preview");
    }
}
Engine sentence(std::shared_ptr<const Dictionary> dict, int pageSize, bool emptyRows) {
    Config config;
    config.pageSize = pageSize;
    Engine engine(std::move(dict), config);
    engine.enableSentenceInput(true, 1);
    press(engine, 'A');
    SentenceDecodeResult decoded;
    for (std::size_t i = 0; i < 7; ++i) {
        if (emptyRows && i % 2 == 0) decoded.candidates.push_back({});
        SentenceCandidate candidate;
        candidate.text = words[i];
        candidate.segmentedCode = u"a";
        decoded.candidates.push_back(std::move(candidate));
    }
    if (emptyRows) decoded.candidates.push_back({});
    const auto ticket = engine.sentenceRequest();
    check(ticket && engine.applySentenceResult(*ticket, std::move(decoded)), "Sentence fixture rejected");
    check(engine.snapshot().total == 7, "Empty rows leaked into published total");
    return engine;
}
int run(const std::filesystem::path& path) {
    const auto dict = dictionary(path);
    const std::vector<std::u16string> sentenceWords(words.begin(), words.begin() + 7);
    for (int pageSize = 1; pageSize <= 10; ++pageSize) {
        Config config;
        config.pageSize = pageSize;
        Engine ordinary(dict, config);
        press(ordinary, 'A');
        checkAll(ordinary, words);
        // An existing nonzero page must not offset an absolute selection twice.
        ordinary.setPage(1);
        checkAll(ordinary, words);
        // Existing page-relative keyboard/adapter selection retains its contract.
        auto relative = ordinary;
        auto absolute = ordinary;
        const auto first = static_cast<std::uint32_t>(relative.snapshot().page * pageSize);
        const auto oldResult = relative.select(0);
        const auto newResult = absolute.selectCandidate(first);
        check(newResult && newResult->commit == oldResult.commit &&
              absolute.recentText() == relative.recentText(), "Page-relative selection changed");
        auto keyboard = ordinary;
        auto mouse = ordinary;
        const auto keyResult = press(keyboard, '2');
        const auto mouseResult = mouse.selectCandidate(first + 1);
        if (pageSize >= 2) {
            check(mouseResult && keyResult.commit == mouseResult->commit &&
                  keyboard.recentText() == mouse.recentText(), "Keyboard and UI selection diverged");
        }
        Engine pinyin(dict, config);
        press(pinyin, 192);
        press(pinyin, 'A');
        check(pinyin.snapshot().mode == Mode::Pinyin, "Pinyin fixture did not enter reverse lookup");
        pinyin.setPage(1);
        checkAll(pinyin, words);
        config.mixedInput = true;
        config.maxCodeLength = 1;
        Engine mixed(dict, config);
        press(mixed, 'A');
        press(mixed, 'A');
        checkAll(mixed, words, words[0]);
        checkAll(sentence(dict, pageSize, false), sentenceWords);
        checkAll(sentence(dict, pageSize, true), sentenceWords);
    }
    auto pending = sentence(dict, 5, false);
    press(pending, 'B');
    const auto before = pending.sentenceRequest();
    const auto stale = pending.selectCandidate(5);
    check(stale && stale->handled && stale->commit.empty() && pending.composing(),
          "Pending decode committed a stale candidate");
    check(pending.sentenceRequest()->generation == before->generation && pending.recentText().empty(),
          "Pending selection changed generation or history");
    Engine actions(dict);
    press(actions, 'B');
    auto add = actions;
    const auto addResult = add.selectCandidate(0);
    check(addResult && addResult->openAddWord && addResult->commit.empty() && add.recentText().empty(),
          "Add-word action bypassed postprocessing");
    const auto hideResult = actions.selectCandidate(1);
    check(hideResult && hideResult->toggleHiddenCandidates && hideResult->commit.empty(),
          "Hide-candidates action bypassed postprocessing");
    press(actions, 'A');
    check(actions.selectCandidate(0)->commit == words[0], "Repeat fixture commit failed");
    press(actions, 'B');
    const auto repeated = actions.selectCandidate(2);
    check(repeated && repeated->commit == words[0] && actions.recentText() == words[0] + words[0],
          "Dynamic repeat/history normalization changed");
    checkInvalid(Engine(dict), 0);
    std::cout << "{\"status\":\"passed\",\"checks\":" << checks
              << ",\"page_sizes\":10,\"physical_input_tested\":false}\n";
    return 0;
}
} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    try {
        if (argc != 2) throw std::runtime_error("Usage: candidate_selection_probe <new-fixture-path>");
        return run(std::filesystem::path(argv[1]));
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
