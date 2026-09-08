#pragma once
#include "Lexicon.h"
#include "DynamicText.h"
#include "MixedInput.h"
#include "Grapheme.h"
#include "History.h"
#include <chrono>
#include <string>
#include <vector>

namespace tiger {
struct ActionShortcut {
    int vk=187;
    bool ctrl=true,alt=false,shift=false;
    bool matches(int key,bool s,bool c,bool a,bool w) const { return key==vk && shift==s && ctrl==c && alt==a && !w; }
};
struct Config {
    std::u16string reloadRequest;
    bool defaultChinese = true, shiftToggle = true, ctrlSpaceToggle = true;
    bool englishPunctuation = false, slashDunhao = true;
    bool enterClear = false, tabClear = true, clearOnNoCode = true;
    bool maxCodeAutoCommit = true, reverseLookup = true;
    bool semicolonSecond = true, quoteThird = true;
    bool showComment = true, showSplit = false;
    bool addWordEnabled = true;
    bool mixedInput = false;
    bool recentSchemaEnabled = false;
    ActionShortcut recentSchemaShortcut{77,true,false,false};
    ActionShortcut addWordShortcut;
    int maxCodeLength = 4, pageSize = 5;
    // 0: - =, 1: [ ], 2: Shift+Tab/Tab, 3: PageUp/PageDown
    int pageKeys = 0;
    std::array<int, 256> selection{};
    Config();
    bool operator==(const Config& other) const;
};
struct KeyEvent {
    int vk = 0, scan = 0, repeat = 1;
    bool down = true, shift = false, ctrl = false, alt = false, win = false;
    bool caps = false, num = true, extended = false;
    // The adapter supplies availability; ordinary engine dispatch never opens files.
    bool recentSchemaAvailable = false;
};
enum class Mode { English = 0, Idle, Composing, Uppercase, Pinyin };
struct KeyResult {
    bool handled = false;
    bool cancelComposition = false;
    std::u16string commit;
    bool openAddWord = false;
    bool toggleHiddenCandidates = false;
    int manualTimerMs = -1;
    bool switchRecentSchema = false;
};
struct Candidate {
    std::u16string display, commit, annotation;
};
struct Snapshot {
    bool chinese = true;
    Mode mode = Mode::Idle;
    std::u16string raw;
    int page = 0;
    std::uint32_t total = 0;
    std::vector<Candidate> candidates;
    std::u16string surface;
    const std::u16string& compositionText() const { return surface.empty()?raw:surface; }
};

// Pure input state machine. It has no Windows/COM/UI/IPC dependencies and cannot
// write user files. The adapter must apply returned commits using a TSF edit session.
class Engine {
public:
    explicit Engine(std::shared_ptr<const Dictionary> dictionary, Config config = {});
    explicit Engine(std::shared_ptr<const Lexicon> lexicon, Config config = {});
    void setLexicon(std::shared_ptr<const Lexicon> lexicon);
    // File invalidations may repeat. Preserve paging and held-key state when
    // the effective configuration did not change.
    bool refreshConfiguration(Config config);
    bool requiresConfigurationReload(const Config& config) const {
        return !config.reloadRequest.empty() && config.reloadRequest!=reloadRequest_;
    }
    void switchSchema(std::shared_ptr<const Lexicon> lexicon,Config config);
    const std::shared_ptr<const Lexicon>& lexicon() const { return lexicon_; }
    // Only the real dispatch adapter drains and persists these. A copied engine
    // may preview keys without writing files or changing another context.
    std::vector<UserChange> takeUserChanges();
    KeyResult process(const KeyEvent& event);
    Snapshot snapshot();
    Candidate candidateAt(std::uint32_t index) const;
    int pageSize() const { return config_.pageSize; }
    void setPage(int page);
    KeyResult select(int index);
    KeyResult setChinese(bool chinese);
    void cancel();
    void focusChanged();
    void configure(Config config);
    bool chinese() const noexcept { return mode_ != Mode::English; }
    bool composing() const noexcept { return !raw_.empty(); }
    std::u16string recentText() const { return history_.text(); }
    std::vector<std::u16string> recentElements() const { return history_.recent(); }

private:
    KeyResult dispatch(KeyEvent key);
    KeyResult idle(const KeyEvent& key);
    KeyResult composition(const KeyEvent& key);
    KeyResult uppercase(const KeyEvent& key);
    KeyResult finishUppercase(std::u16string suffix={});
    KeyResult finish(std::u16string text = {});
    KeyResult finishMixed(std::u16string text = {});
    void rebuildMixed();
    KeyResult selectRaw(int index);
    KeyResult toggle();
    bool controlSpace(const KeyEvent& key, KeyResult& result);
    void resetControlSpace();
    void refreshPage();
    Lexicon::Match entry() const;
    std::u16string selected(int index) const;
    std::u16string resolve(std::u16string_view code) const;
    std::u16string annotation(std::u16string_view packed) const;
    std::u16string convert(std::u16string_view text) const;
    std::u16string symbol(int vk, bool shifted, bool english) const;
    std::u16string quote(bool doubleQuote);
    int selection(int vk) const;
    int pageDelta(const KeyEvent& key) const;
    void postprocess(const KeyEvent& key, KeyResult& result);
    void appendHistory(std::u16string_view text);

    std::shared_ptr<const Lexicon> lexicon_;
    std::vector<UserChange> userChanges_;
    int oneShotActionKey_ = 0;
    bool schemaAwaitingModifierRelease_ = false;
    Config config_;
    std::u16string reloadRequest_;
    mutable std::minstd_rand random_{dynamicSeed()};
    Mode mode_ = Mode::Idle;
    std::u16string raw_, pageRaw_;
    std::u16string mixedRaw_;
    MixedDecoder mixedDecoder_;
    MixedDecoder::Preferred mixedPreferred_;
    MixedResult mixedResult_;
    std::uint64_t lexiconRevision_=0;
    Mode pageMode_ = Mode::Idle;
    int page_ = 0;
    bool leftShift_ = false, rightShift_ = false, shiftChord_ = false;
    std::array<bool, 256> consumedModifiers_{};
    bool controlDown_ = false, spaceDown_ = false, controlArmed_ = false, controlSwitched_ = false;
    std::chrono::steady_clock::time_point controlReleased_{};
    bool leftSingle_ = true, leftDouble_ = true, deletedSingle_ = false, deletedDouble_ = false;
    bool quoteDown_ = false, digit_ = false;
    History history_;
    std::u16string repeat_ = u"\u91cd\u590d\u4e0a\u5c4f";
};
}
