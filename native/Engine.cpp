#include "Engine.h"
#include "UppercaseText.h"
#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace tiger {
namespace {
constexpr int Back=8, Tab=9, Return=13, Shift=16, Control=17, Alt=18, Caps=20,
    Escape=27, Space=32, PageUp=33, PageDown=34, LWin=91, RWin=92,
    LShift=160, RShift=161, LControl=162, RControl=163, LAlt=164, RAlt=165,
    Semi=186, Plus=187, Comma=188, Minus=189, Period=190, Slash=191,
    Grave=192, Bracket=219, Backslash=220, CloseBracket=221, Quote=222;
bool shiftKey(int vk) { return vk == Shift || vk == LShift || vk == RShift; }
bool controlKey(int vk) { return vk == Control || vk == LControl || vk == RControl; }
bool modifier(int vk) { return shiftKey(vk) || controlKey(vk) || vk == Alt || vk == LAlt || vk == RAlt || vk == LWin || vk == RWin || vk == Caps; }
bool digitKey(int vk) { return (vk >= '0' && vk <= '9') || (vk >= 96 && vk <= 105); }
bool letter(int vk) { return vk >= 'A' && vk <= 'Z'; }
std::u16string_view display(std::u16string_view packed) {
    const auto split = packed.find(u'\x1e');
    return split == std::u16string_view::npos ? packed : packed.substr(0, split);
}
std::u16string_view output(std::u16string_view packed) {
    const auto split = packed.find(u'\x1e');
    return split == std::u16string_view::npos ? packed : packed.substr(split + 1);
}
std::u16string lower(std::u16string_view source) {
    std::u16string text(source);
    for (auto& ch : text) if (ch >= u'A' && ch <= u'Z') ch += u'a' - u'A';
    return text;
}
}

Config::Config() {
    for (int i = 1; i <= 9; ++i) selection['0' + i] = i;
    selection['0'] = 10;
}
bool Config::operator==(const Config& other) const {
    const auto fields=[](const Config& c) {
        return std::tie(c.reloadRequest,c.defaultChinese,c.shiftToggle,c.ctrlSpaceToggle,
            c.englishPunctuation,c.slashDunhao,c.enterClear,c.tabClear,c.clearOnNoCode,
            c.maxCodeAutoCommit,c.reverseLookup,c.semicolonSecond,c.quoteThird,
            c.showComment,c.showSplit,c.addWordEnabled,c.mixedInput,c.recentSchemaEnabled,
            c.recentSchemaShortcut.vk,c.recentSchemaShortcut.ctrl,c.recentSchemaShortcut.alt,c.recentSchemaShortcut.shift,
            c.addWordShortcut.vk,c.addWordShortcut.ctrl,c.addWordShortcut.alt,c.addWordShortcut.shift,
            c.maxCodeLength,c.pageSize,c.pageKeys,c.selection);
    };
    return fields(*this)==fields(other);
}
bool Engine::refreshConfiguration(Config config) {
    config.maxCodeLength=std::clamp(config.maxCodeLength,1,16);
    config.pageSize=std::clamp(config.pageSize,1,10);
    if(config_==config)return false;
    configure(std::move(config));return true;
}
Engine::Engine(std::shared_ptr<const Dictionary> dictionary, Config config)
    : Engine(std::make_shared<Lexicon>(std::move(dictionary)), std::move(config)) {}
Engine::Engine(std::shared_ptr<const Lexicon> lexicon, Config config)
    : lexicon_(std::move(lexicon)) {
    if (!lexicon_) throw std::invalid_argument("Engine requires a lexicon");
    configure(std::move(config));
    mode_ = config_.defaultChinese ? Mode::Idle : Mode::English;
}
void Engine::setLexicon(std::shared_ptr<const Lexicon> lexicon) {
    if (!lexicon) throw std::invalid_argument("Engine requires a lexicon");
    lexicon_ = std::move(lexicon);
    ++lexiconRevision_;
    sentence_.invalidatePending(true);
    if(!mixedRaw_.empty()) rebuildMixed();
    refreshPage();
}
std::vector<UserChange> Engine::takeUserChanges() {
    auto changes = std::move(userChanges_);
    userChanges_.clear();
    return changes;
}
void Engine::switchSchema(std::shared_ptr<const Lexicon> lexicon,Config config) {
    if(!lexicon) throw std::invalid_argument("Schema switch requires a lexicon");
    const bool reload=requiresConfigurationReload(config);
    const auto previousMode=mode_;
    const auto code=mixedRaw_.empty()?raw_:mixedRaw_;
    cancel(); mixedDecoder_.clear();
    configure(std::move(config));
    lexicon_=std::move(lexicon); ++lexiconRevision_;
    if(reload)return;
    mode_=previousMode; raw_=code; pageRaw_.clear(); page_=0;
    if(mode_==Mode::Sentence) {sentence_.start(code,sentenceResources_);syncSentenceRaw();}
    // Original schema refresh preserves all raw keys. Long ordinary input
    // becomes a temporary mixed session even if the target disables mixed mode.
    if(mode_==Mode::Composing && !code.empty() &&
       (config_.mixedInput || code.size()>static_cast<std::size_t>(config_.maxCodeLength))) {
        mixedRaw_=code; rebuildMixed();
    }
    refreshPage();
}
void Engine::configure(Config config) {
    const bool reload=requiresConfigurationReload(config);
    config.maxCodeLength = std::clamp(config.maxCodeLength, 1, 16);
    config.pageSize = std::clamp(config.pageSize, 1, 10);
    config_ = std::move(config);
    if(reload) {
        // Match ResetCompositionForConfigChange followed by SetChinese:
        // discard raw input, preserve history/held keys, reset digit punctuation.
        reloadRequest_=config_.reloadRequest;
        cancel();digit_=false;
        mode_=config_.defaultChinese?Mode::Idle:Mode::English;
    }
    if(!mixedRaw_.empty()) rebuildMixed();
    pageRaw_.clear(); page_ = 0;
}
void Engine::cancel() {
    sentence_.clear();
    mixedRaw_.clear(); mixedPreferred_.clear(); mixedResult_={};
    raw_.clear(); pageRaw_.clear(); page_ = 0;
    mode_ = chinese() ? Mode::Idle : Mode::English;
}
void Engine::focusChanged() {
    sentence_.invalidatePending();
    oneShotActionKey_ = 0;
    schemaAwaitingModifierRelease_ = false;
    leftShift_ = rightShift_ = shiftChord_ = false;
    consumedModifiers_.fill(false);
    resetControlSpace();
    digit_ = false; pageRaw_.clear(); page_ = 0;
}
KeyResult Engine::finish(std::u16string text) {
    cancel();
    return {true, false, std::move(text)};
}
KeyResult Engine::finishMixed(std::u16string text) {
    return finish(mixedResult_.prefix+text);
}
void Engine::rebuildMixed() {
    const auto completed=mixedRaw_.empty()?0:((mixedRaw_.size()-1)/config_.maxCodeLength)*config_.maxCodeLength;
    for(auto i=mixedPreferred_.lower_bound(completed);i!=mixedPreferred_.end();) i=mixedPreferred_.erase(i);
    mixedResult_=mixedDecoder_.decode(mixedRaw_,config_.maxCodeLength,lexiconRevision_,[&](std::u16string_view code) {
        auto found=lexicon_->find(Section::Main,lower(code));
        return found.count?convert(output(lexicon_->value(found,0))):std::u16string{};
    },mixedPreferred_);
    raw_=mixedResult_.active; pageRaw_.clear(); page_=0;
}
KeyResult Engine::setChinese(bool enabled) {
    if (enabled == chinese()) return {};
    std::u16string text = enabled ? std::u16string{} : mode_==Mode::Sentence?sentence_.liveRaw():mixedRaw_.empty()?raw_:mixedRaw_;
    cancel();
    mode_ = enabled ? Mode::Idle : Mode::English;
    return {!text.empty(), false, std::move(text)};
}
KeyResult Engine::toggle() { return setChinese(!chinese()); }
Lexicon::Match Engine::entry() const {
    if (mode_ != Mode::Composing && mode_ != Mode::Pinyin) return {};
    return lexicon_->find(mode_ == Mode::Pinyin ? Section::Pinyin : Section::Main,
        lower(mode_ == Mode::Pinyin ? std::u16string_view(raw_).substr(1) : raw_));
}
void Engine::refreshPage() {
    if (pageRaw_ != raw_ || pageMode_ != mode_) {
        pageRaw_ = raw_; pageMode_ = mode_; page_ = 0;
    }
    const auto candidates = entry();
    const auto last = candidates.count ? (candidates.count - 1) / static_cast<unsigned>(config_.pageSize) : 0;
    page_ = std::clamp(page_, 0, static_cast<int>(last));
}
std::u16string Engine::selected(int index) const {
    if (index < 0 || index >= config_.pageSize) return {};
    const auto candidates = entry();
    const auto n = static_cast<std::uint32_t>(page_ * config_.pageSize + index);
    return convert(output(lexicon_->value(candidates, n)));
}
std::u16string Engine::resolve(std::u16string_view code) const {
    auto found = lexicon_->find(Section::Main, lower(code));
    return found.count ? convert(output(lexicon_->value(found, 0))) : std::u16string(code);
}
KeyResult Engine::selectRaw(int index) {
    if(mode_==Mode::Sentence) {
        auto text=sentence_.commitCandidate(index);auto learning=sentence_.takeLearning();
        auto result=text?finish(std::move(*text)):KeyResult{true,false,{}};
        result.learning=std::move(learning);return result;
    }
    refreshPage();
    return finishMixed(selected(index));
}
KeyResult Engine::select(int index) {
    auto result=selectRaw(index);
    // Mouse/UI-less selection has the same output normalization and history
    // effects as Space, although no physical key event reaches process().
    KeyEvent key; key.vk=Space;
    postprocess(key,result);
    return result;
}

std::optional<KeyResult> Engine::selectCandidate(std::uint32_t index) {
    if (mode_ == Mode::Sentence) {
        const int candidate = sentenceCandidateIndex(index);
        if (candidate < 0) return {};
        return select(candidate);
    }
    if (index >= entry().count) return {};
    const auto size = static_cast<std::uint32_t>(config_.pageSize);
    setPage(static_cast<int>(index / size));
    return select(static_cast<int>(index % size));
}

int Engine::selection(int vk) const {
    if (vk < 0 || vk >= 256) return 0;
    if (config_.selection[vk]) return config_.selection[vk];
    if (shiftKey(vk)) return config_.selection[Shift];
    if (controlKey(vk)) return config_.selection[Control];
    if (vk == LAlt || vk == RAlt) return config_.selection[Alt];
    if (vk == RWin) return config_.selection[LWin];
    return 0;
}
int Engine::pageDelta(const KeyEvent& key) const {
    switch (config_.pageKeys) {
    case 1: return key.shift ? 0 : key.vk == Bracket ? -1 : key.vk == CloseBracket ? 1 : 0;
    case 2: return key.vk == Tab ? (key.shift ? -1 : 1) : 0;
    case 3: return key.shift ? 0 : key.vk == PageUp ? -1 : key.vk == PageDown ? 1 : 0;
    default: return key.shift ? 0 : key.vk == Minus ? -1 : key.vk == Plus ? 1 : 0;
    }
}
void Engine::resetControlSpace() {
    controlDown_ = spaceDown_ = controlArmed_ = controlSwitched_ = false;
    controlReleased_ = {};
}
bool Engine::controlSpace(const KeyEvent& key, KeyResult& result) {
    const auto now = std::chrono::steady_clock::now();
    auto cleanup = [&] {
        if (!controlDown_ && !spaceDown_ && (controlSwitched_ ||
            (controlArmed_ && controlReleased_ != std::chrono::steady_clock::time_point{} &&
             now - controlReleased_ > std::chrono::milliseconds(250)))) resetControlSpace();
    };
    if (!controlKey(key.vk) && key.vk != Space) {
        if (key.down && controlDown_) resetControlSpace(); else cleanup();
        return false;
    }
    if (!config_.ctrlSpaceToggle || key.shift || key.alt || key.win) {
        resetControlSpace(); return false;
    }
    bool trigger = false;
    if (key.down) {
        if (controlKey(key.vk)) {
            controlDown_ = true; controlArmed_ = !spaceDown_; controlSwitched_ = false; controlReleased_ = {};
        } else {
            spaceDown_ = true; trigger = controlDown_ && key.repeat <= 1;
        }
    } else {
        if (controlKey(key.vk)) {
            controlDown_ = false; controlReleased_ = now; trigger = spaceDown_;
        } else {
            spaceDown_ = false;
            trigger = controlDown_ || (controlReleased_ != std::chrono::steady_clock::time_point{} &&
                now - controlReleased_ <= std::chrono::milliseconds(250));
        }
    }
    const bool handled = trigger && controlArmed_ && !controlSwitched_;
    if (handled) { result = toggle(); result.handled = true; controlSwitched_ = true; }
    cleanup();
    return handled;
}
KeyResult Engine::process(const KeyEvent& key,const SentencePathQueries& queries) {
    auto result = dispatch(key,queries);
    postprocess(key, result);
    return result;
}
KeyResult Engine::dispatch(KeyEvent key,const SentencePathQueries& queries) {
    if(!key.down && modifier(key.vk) && !key.shift && !key.ctrl && !key.alt && !key.win)
        schemaAwaitingModifierRelease_ = false;
    if (!key.down && key.vk == oneShotActionKey_) oneShotActionKey_ = 0;
    if (key.vk == Shift) {
        if (key.scan == 0x2a) key.vk = LShift;
        if (key.scan == 0x36) key.vk = RShift;
    } else if (key.vk == Control) key.vk = key.extended ? RControl : LControl;
    else if (key.vk == Alt) key.vk = key.extended ? RAlt : LAlt;
    if (key.down && key.vk != Quote && key.vk != Back && !modifier(key.vk)) deletedSingle_ = deletedDouble_ = false;
    if (key.vk == Quote) {
        if (key.down) quoteDown_ = true;
        else {
            const bool seen = quoteDown_; quoteDown_ = false;
            if (!seen && !key.ctrl && !key.alt && !key.win && !key.caps) {
                if (mode_ == Mode::Idle) return {true, false, quote(key.shift)};
                if (mode_ == Mode::Composing || mode_ == Mode::Pinyin) {
                    refreshPage();
                    auto first = selected(0);
                    if (!first.empty() || !mixedRaw_.empty()) return finishMixed(first + quote(key.shift));
                }
            }
        }
    }
    KeyResult chord;
    if (controlSpace(key, chord)) return chord;
    if (key.down && modifier(key.vk) && composing() &&
        (mode_ == Mode::Composing || mode_ == Mode::Pinyin) && selection(key.vk)) {
        consumedModifiers_[key.vk] = true;
        return selectRaw(selection(key.vk) - 1);
    }
    if (!key.down && key.vk >= 0 && key.vk < 256 && consumedModifiers_[key.vk]) {
        consumedModifiers_[key.vk] = false; return {true, false, {}};
    }
    if (shiftKey(key.vk)) {
        bool& down = key.vk == RShift ? rightShift_ : leftShift_;
        if (key.down) {
            if (!leftShift_ && !rightShift_) shiftChord_ = false;
            down = true; return {};
        }
        const bool matched = down; down = false;
        KeyResult result;
        if (matched && config_.shiftToggle && !shiftChord_ && !key.ctrl && !key.alt && !key.win) result = toggle();
        if (!leftShift_ && !rightShift_) shiftChord_ = false;
        return result;
    }
    if (key.down && (leftShift_ || rightShift_)) shiftChord_ = true;
    if (!key.down) return {};
    const bool addWord=config_.addWordEnabled && config_.addWordShortcut.matches(key.vk,key.shift,key.ctrl,key.alt,key.win);
    const bool recent=config_.recentSchemaEnabled && config_.recentSchemaShortcut.matches(key.vk,key.shift,key.ctrl,key.alt,key.win);
    if(schemaAwaitingModifierRelease_ && (addWord || recent)) return {true,false,{}};
    if(addWord) {
        const bool first=oneShotActionKey_!=key.vk;
        oneShotActionKey_=key.vk;
        return {true,false,{},first};
    }
    if(recent && key.recentSchemaAvailable) {
        if(oneShotActionKey_==key.vk) return {true,false,{}};
        oneShotActionKey_=key.vk; schemaAwaitingModifierRelease_=true;
        KeyResult result;result.handled=true;result.switchRecentSchema=true;return result;
    }
    if (mode_ == Mode::Composing && key.vk >= '1' && key.vk <= '9' && !key.win &&
        ((key.ctrl && !key.alt) || (key.alt && !key.ctrl && !key.shift))) {
        if (oneShotActionKey_ == key.vk) return {true, false, {}};
        refreshPage();
        const auto all = entry();
        const auto index = static_cast<unsigned>(page_ * config_.pageSize + key.vk - '1');
        if (key.vk - '1' < config_.pageSize && index < all.count) {
            UserChange change{key.alt ? ChangeKind::Advance : (key.shift ? ChangeKind::Delete : ChangeKind::Top),
                raw_, std::u16string(lexicon_->value(all, index))};
            bool changed = false;
            auto updated = lexicon_->changed(change, &changed);
            if (changed) { setLexicon(std::move(updated)); userChanges_.push_back(std::move(change)); }
            if (changed || key.alt) {
                oneShotActionKey_ = key.vk;
                return {true, false, {}};
            }
        }
    }
    if (key.ctrl || key.alt || key.win) {
        const bool bare=key.alt?(key.vk==Alt || key.vk==LAlt || key.vk==RAlt):key.ctrl?controlKey(key.vk):(key.vk==LWin || key.vk==RWin);
        const bool shouldCancel = !bare && composing();
        if (shouldCancel) cancel();
        return {false, shouldCancel, {}};
    }
    if (key.vk == Caps && composing()) {
        std::u16string commit;
        if (mode_ == Mode::Pinyin) { refreshPage(); auto all = entry(); commit = all.count ? convert(output(lexicon_->value(all, 0))) : raw_; }
        else if(mode_==Mode::Sentence)commit=sentence_.liveRaw();
        else commit = mixedResult_.prefix+resolve(raw_);
        cancel(); mode_ = Mode::Idle;
        return {false, false, std::move(commit)};
    }
    if (key.caps || key.vk == Caps || mode_ == Mode::English) return {};
    refreshPage();
    if (mode_ == Mode::Idle) return idle(key);
    if (mode_ == Mode::Uppercase) return uppercase(key);
    if (mode_ == Mode::Sentence) return sentence(key,queries);
    return composition(key);
}

std::u16string Engine::symbol(int vk, bool shifted, bool english) const {
    if (shifted && vk >= '0' && vk <= '9') {
        static constexpr std::u16string_view en[] = {u")",u"!",u"@",u"#",u"$",u"%",u"^",u"&",u"*",u"("};
        static constexpr std::u16string_view cn[] = {u"\uff09",u"\uff01",u"@",u"#",u"\uffe5",u"%",u"\u2026\u2026",u"&",u"*",u"\uff08"};
        return std::u16string(english ? en[vk-'0'] : cn[vk-'0']);
    }
    struct Symbol { int vk; std::u16string_view cn, en, shiftCn, shiftEn; };
    static constexpr Symbol symbols[] = {
        {Plus,u"=",u"=",u"+",u"+"}, {Minus,u"-",u"-",u"\u2014\u2014",u"_"},
        {Comma,u"\uff0c",u",",u"\u300a",u"<"}, {Period,u"\u3002",u".",u"\u300b",u">"},
        {Semi,u"\uff1b",u";",u"\uff1a",u":"}, {Slash,u"\u3001",u"/",u"\uff1f",u"?"},
        {Grave,u"\u00b7",u"`",u"~",u"~"}, {Bracket,u"\u3010",u"[",u"{",u"{"},
        {CloseBracket,u"\u3011",u"]",u"}",u"}"}, {Backslash,u"\u3001",u"\\",u"|",u"|"}
    };
    if (vk == Quote && english) return shifted ? u"\"" : u"'";
    for (const auto& item : symbols) if (vk == item.vk) {
        if (shifted) return std::u16string(english ? item.shiftEn : item.shiftCn);
        if (vk == Slash && !config_.slashDunhao) return u"/";
        if (vk == Period && digit_) return u".";
        return std::u16string(english ? item.en : item.cn);
    }
    return {};
}
std::u16string Engine::quote(bool doubleQuote) {
    bool& left = doubleQuote ? leftDouble_ : leftSingle_;
    bool& deleted = doubleQuote ? deletedDouble_ : deletedSingle_;
    const bool colon = history_.last()==u":" || history_.last()==u"\uff1a";
    const bool emitLeft = colon && !deleted ? true : deleted ? !left : left;
    left = !emitLeft; deleted = false;
    return doubleQuote ? (emitLeft ? u"\u201c" : u"\u201d") : (emitLeft ? u"\u2018" : u"\u2019");
}
KeyResult Engine::idle(const KeyEvent& key) {
    const int vk = key.vk;
    const auto quick = lexicon_->quickSymbols();
    char16_t special = 0;
    if (!key.shift) {
        if (vk == Semi && (quick & 1)) special = u';';
        if (vk == Slash && (quick & 2)) special = u'/';
        if (vk == Bracket && (quick & 4)) special = u'[';
        if (vk == 'Z' && (quick & 8)) special = u'z';
    }
    if (special) {
        raw_ = special; mode_ = Mode::Composing;
        if (lexicon_->find(Section::Main, raw_).autoSymbol()) return finish(resolve(raw_));
        return {true, false, {}};
    }
    if (key.shift) {
        auto text = symbol(vk, true, config_.englishPunctuation);
        if (!text.empty() && vk != Quote) return {true, false, std::move(text)};
    }
    if (vk == Grave && !key.shift && config_.reverseLookup && lexicon_->dictionary()->count(Section::Pinyin)) {
        raw_ = u"\u00b7"; mode_ = Mode::Pinyin; return {true, false, {}};
    }
    if (vk == Quote) return {true, false, quote(key.shift)};
    auto text = symbol(vk, false, config_.englishPunctuation);
    if (!text.empty()) return {true, false, std::move(text)};
    if (letter(vk)) {
        raw_ = static_cast<char16_t>(key.shift ? vk : vk + 32);
        if(sentenceEnabled_ && !key.shift) {sentence_.start(raw_,sentenceResources_);mode_=Mode::Sentence;return {true,false,{}};}
        mode_ = key.shift ? Mode::Uppercase : Mode::Composing;
        if(!key.shift && config_.mixedInput) { mixedRaw_=raw_; rebuildMixed(); return {true,false,{}}; }
        if (!key.shift && config_.maxCodeLength == 1 && config_.maxCodeAutoCommit && entry().unique()) return finish(resolve(raw_));
        return {true, false, {}};
    }
    if (vk == RControl) { mode_ = Mode::English; return {true, false, {}}; }
    // This is a pass-through result carrying history text. The TSF adapter must
    // avoid inserting the newline twice when allowing the physical Enter through.
    if (vk == Return) return {false, false, u"\n"};
    return {};
}
void Engine::enableSentenceInput(bool enabled,std::uint64_t revision,bool automatic,int retainedRaw) {
    if(sentenceAutomatic_!=automatic || sentenceRetainedRaw_!=retainedRaw)sentence_.resetAutomaticState();
    sentenceAutomatic_=automatic;sentenceRetainedRaw_=retainedRaw;
    sentenceEnabled_=enabled;sentenceResources_=revision;
    if(mode_==Mode::Sentence) {
        if(!enabled)cancel();
        else {sentence_.changeResources(revision);syncSentenceRaw();}
    }
}
KeyResult Engine::autoCommitSentence(const SentencePathQueries& queries) {
    if(mode_!=Mode::Sentence)return {};
    auto commit=sentence_.tryAutoCommit(sentenceAutomatic_,sentenceRetainedRaw_,queries);
    if(!commit)return {};
    syncSentenceRaw();KeyResult result{true,false,std::move(*commit)};result.learning=sentence_.takeLearning();return result;
}
std::optional<SentenceDecodeTicket> Engine::sentenceRequest() const {
    return mode_==Mode::Sentence?sentence_.request():std::optional<SentenceDecodeTicket>{};
}
bool Engine::applySentenceResult(const SentenceDecodeTicket& ticket,SentenceDecodeResult result) {
    if(mode_!=Mode::Sentence)return false;
    bool applied=sentence_.apply(ticket,std::move(result));if(applied)syncSentenceRaw();return applied;
}
void Engine::syncSentenceRaw(){raw_=sentence_.liveRaw();if(!sentence_.active())mode_=Mode::Idle;}
KeyResult Engine::sentence(const KeyEvent& key,const SentencePathQueries& queries) {
    const int vk=key.vk;
    if(vk==Back){sentence_.backspace();syncSentenceRaw();return {true,false,{}};}
    if(vk==Escape)return finish();
    if(vk==Return)return finish(sentence_.commitRaw(config_.enterClear));
    const bool selector=!key.shift && (digitKey(vk) || (vk==Semi && config_.semicolonSecond) || (vk==Quote && config_.quoteThird));
    if(letter(vk) || selector) {
        if(letter(vk) && sentence_.tabSelectionPending() && !sentence_.current()) {
            KeyResult pending;pending.handled=true;pending.awaitSentenceDecode=true;return pending;
        }
        char16_t c=letter(vk)?static_cast<char16_t>(vk+32):vk==Semi?u';':vk==Quote?u'\'':static_cast<char16_t>(vk>=96?'0'+vk-96:vk);
        auto commit=sentence_.appendAutomatic(c,sentenceAutomatic_,sentenceRetainedRaw_,queries);
        syncSentenceRaw();KeyResult result{true,false,commit?std::move(*commit):std::u16string{}};
        result.learning=sentence_.takeLearning();return result;
    }
    auto suffix=vk==Quote?std::u16string{}:symbol(vk,key.shift,config_.englishPunctuation);
    const bool needsCurrent=vk==Tab || vk==Space || vk==38 || vk==40 || vk==Quote || !suffix.empty();
    if(vk==Tab || vk==Space || vk==38 || vk==40)sentence_.resetEmptyCodePending();
    if(needsCurrent && !sentence_.current()) {
        KeyResult pending;pending.handled=true;pending.awaitSentenceDecode=true;return pending;
    }
    if(vk==Tab || vk==38 || vk==40) {
        if(vk==Tab && sentence_.result().candidates.empty() && config_.tabClear)return finish();
        sentence_.moveSelection(vk==38 || (vk==Tab && key.shift)?-1:1,config_.pageSize,vk==Tab);return {true,false,{}};
    }
    if(vk==Space) {
        return selectRaw(sentence_.selectedIndex());
    }
    if(vk==Quote)suffix=quote(key.shift);
    if(!suffix.empty()) {
        auto text=sentence_.commitWithSuffix(suffix);auto learning=sentence_.takeLearning();
        auto result=finish(std::move(text));result.learning=std::move(learning);return result;
    }
    return {};
}

KeyResult Engine::composition(const KeyEvent& key) {
    const int vk = key.vk;
    const bool pinyin = mode_ == Mode::Pinyin;
    const auto all = entry();
    const auto available = static_cast<int>(std::min<std::uint32_t>(
        static_cast<unsigned>(config_.pageSize), all.count - static_cast<unsigned>(page_ * config_.pageSize)));
    auto first = selected(0);
    if (key.shift && vk != Quote) {
        auto suffix = symbol(vk, true, config_.englishPunctuation);
        if (!suffix.empty()) return finishMixed((available || !mixedRaw_.empty()) ? first + suffix : std::u16string{});
    }
    auto selectKey = [&]() -> KeyResult {
        const int choice = selection(vk);
        if (!available) return finishMixed();
        if (choice <= available) return selectRaw(choice - 1);
        return finishMixed(digitKey(vk) ? first + static_cast<char16_t>('0' + (choice == 10 ? 0 : choice)) : std::u16string{});
    };
    if (!pinyin && !key.shift && selection(vk)) return selectKey();
    if (!pinyin && raw_ == u"[" && (lexicon_->quickSymbols() & 4) &&
        (vk == Bracket || (vk == Space && !available))) return finish(u"\u3010");
    if (const int delta = pageDelta(key)) { page_ += delta; refreshPage(); return {true, false, {}}; }
    if (pinyin && raw_ == u"\u00b7" && vk == Grave) return finish(u"\u00b7");
    if (!pinyin) {
        if (raw_ == u";" && (vk == Semi || (vk == Space && !available))) return finish(u"\uff1b");
        if (raw_ == u"/" && (vk == Slash || (vk == Space && !available))) return finish(config_.slashDunhao ? u"\u3001" : u"/");
        if (raw_ == u"[" && (vk == Bracket || (vk == Space && !available))) return finish(u"\u3010");
    }
    if (vk == Space) return finishMixed(first);
    if (!key.shift && vk == Semi && config_.semicolonSecond && available >= 2) return selectRaw(1);
    if (!key.shift && vk == Quote && config_.quoteThird && available >= 3) return selectRaw(2);
    if (pinyin && !key.shift && selection(vk)) return selectKey();
    // In reverse lookup, configured bindings precede editing keys, while
    // Space, paging and built-in second/third selection retain their priority.
    if (vk == Back) {
        if(!mixedRaw_.empty()) {
            mixedRaw_.pop_back(); rebuildMixed();
            return mixedRaw_.empty()?finish():KeyResult{true,false,{}};
        }
        if (!raw_.empty()) raw_.pop_back();
        if (raw_.size() <= (pinyin ? 1u : 0u)) return finish();
        return {true, false, {}};
    }
    if (vk == Escape) return finish();
    if (vk == Return) return finish(config_.enterClear ? std::u16string{} : mixedRaw_.empty()?raw_:mixedRaw_);
    if (vk == Tab) return config_.tabClear ? finish() : KeyResult{};
    if (vk == Quote) return finishMixed((available || !mixedRaw_.empty()) ? first + quote(key.shift) : std::u16string{});
    if (!symbol(vk, false, config_.englishPunctuation).empty()) {
        const bool mixed=!mixedRaw_.empty();
        first.insert(0,mixedResult_.prefix);
        cancel();
        if (!available && !mixed) return {true, false, {}};
        auto result = idle(key);
        if (result.commit == u"\u3002" && !first.empty() && first.back() >= u'0' && first.back() <= u'9') result.commit = u".";
        result.commit.insert(0, first); result.handled = true; return result;
    }
    if (letter(vk)) {
        const auto ch = static_cast<char16_t>(vk + 32);
        if (pinyin) { raw_ += ch; return {true, false, {}}; }
        if(!mixedRaw_.empty()) {
            if(raw_.size()==static_cast<std::size_t>(config_.maxCodeLength) && available)
                mixedPreferred_[mixedRaw_.size()-raw_.size()]=first;
            mixedRaw_+=static_cast<char16_t>(key.shift?vk:vk+32); rebuildMixed();
            return {true,false,{}};
        }
        const auto extended = raw_ + ch;
        const auto found = lexicon_->find(Section::Main, lower(extended));
        if (lexicon_->quickSymbols() && found.autoSymbol()) return finish(resolve(extended));
        if (raw_.size() < static_cast<std::size_t>(config_.maxCodeLength - 1)) { raw_ += ch; return {true, false, {}}; }
        if (config_.maxCodeAutoCommit && found.unique()) return finish(resolve(extended));
        if (raw_.size() == static_cast<std::size_t>(config_.maxCodeLength - 1) || found.count || found.prefix()) {
            raw_ += ch; return {true, false, {}};
        }
        if (config_.clearOnNoCode || available) { raw_ = ch; return {true, false, first}; }
        raw_ += ch; return {true, false, {}};
    }
    if (!key.shift && vk >= '0' && vk <= '9') return finishMixed(first + static_cast<char16_t>(vk));
    return {};
}
KeyResult Engine::finishUppercase(std::u16string suffix) {
    const auto timer=parseManualTimer(raw_);
    if(timer.matched) {
        auto result=finish(std::move(suffix)); result.manualTimerMs=timer.milliseconds; return result;
    }
    return finish(uppercaseCurrency(raw_)+suffix);
}
KeyResult Engine::uppercase(const KeyEvent& key) {
    if (key.vk == Space || key.vk == Return) return finishUppercase();
    if (key.vk == Tab) return config_.tabClear ? finish() : KeyResult{};
    auto suffix = symbol(key.vk, key.shift, true);
    if (!suffix.empty()) {
        if(!key.shift && (key.vk==Period || key.vk==Comma) && numericUppercasePrefix(raw_)) {
            raw_+=suffix; return {true,false,{}};
        }
        return finishUppercase(std::move(suffix));
    }
    if (letter(key.vk)) { raw_ += static_cast<char16_t>(key.shift ? key.vk : key.vk + 32); return {true, false, {}}; }
    if (key.vk >= '0' && key.vk <= '9') { raw_ += static_cast<char16_t>(key.vk); return {true, false, {}}; }
    if (key.vk == Escape) return finish();
    if (key.vk == Back) {
        if (!raw_.empty()) raw_.pop_back();
        return raw_.empty() ? finish() : KeyResult{true, false, {}};
    }
    return {};
}

std::u16string Engine::convert(std::u16string_view text) const {
    if (text == u"\u3002" && digit_) return u".";
    if (text == u"{\u91cd\u590d\u4e0a\u5c4f}") return repeat_;
    return expandDynamicText(text,random_);
}
std::u16string Engine::annotation(std::u16string_view packed) const {
    if (packed.find(u'\x1e') != std::u16string_view::npos) return {};
    const bool reverse = mode_ == Mode::Pinyin;
    auto metadata = [&](Section section, std::u16string_view key) {
        return lexicon_->value(lexicon_->find(section, key), 0);
    };
    auto collect = [&](Section section) {
        std::u16string result;
        for (std::size_t i = 0; i < packed.size();) {
            const std::size_t size = packed[i] >= 0xd800 && packed[i] <= 0xdbff && i+1 < packed.size() && packed[i+1] >= 0xdc00 && packed[i+1] <= 0xdfff ? 2 : 1;
            auto item = metadata(section, packed.substr(i, size));
            if (item.empty()) return std::u16string{};
            if (!result.empty()) result += u'\u00b7';
            result += item;
            i += size;
        }
        return result;
    };
    std::u16string result;
    auto append = [&](std::u16string_view part) { if (!part.empty()) { if (!result.empty()) result += u" | "; result += part; } };
    if (reverse || config_.showSplit) append(collect(Section::Split));
    if (reverse) append(collect(Section::FullCode));
    if (reverse) append(metadata(Section::Comment, packed));
    else if (config_.showComment) {
        auto comment = metadata(Section::Comment, packed);
        if (!comment.empty()) { if (!result.empty()) result += u' '; result += comment; }
    }
    return result;
}
int Engine::sentenceCandidateIndex(std::uint32_t index) const {
    // A pending result may still contain a candidate already committed in
    // full. Display and selection must skip the same empty remaining text.
    for (int i = 0; i < static_cast<int>(sentence_.result().candidates.size()); ++i) {
        if (sentence_.candidateText(i).empty()) continue;
        if (index == 0) return i;
        --index;
    }
    return -1;
}
Candidate Engine::candidateAt(std::uint32_t index) const {
    if(mode_==Mode::Sentence) {
        const int candidate = sentenceCandidateIndex(index);
        if (candidate < 0) return {};
        const auto text = sentence_.candidateText(candidate);
        return {convert(text),convert(text),annotation(text)};
    }
    const auto all = entry();
    if (index >= all.count) return {};
    const auto packed = lexicon_->value(all, index);
    return {packed.find(u'\x1e') == std::u16string_view::npos ? convert(packed) : std::u16string(display(packed)),
        convert(output(packed)), annotation(packed)};
}
void Engine::setPage(int page) { refreshPage(); page_ = page; refreshPage(); }
Snapshot Engine::snapshot() {
    refreshPage();
    Snapshot result;
    result.chinese = chinese(); result.mode = mode_; result.raw = raw_; result.page = page_;
    if(!mixedRaw_.empty()) { result.raw=mixedRaw_; result.surface=mixedResult_.surface; result.displayPrefixLength=mixedResult_.prefix.size(); }
    if(mode_==Mode::Sentence) {
        result.raw=sentence_.liveRaw();result.surface=sentence_.displayCode();
        for(int i=0;i<static_cast<int>(sentence_.result().candidates.size());++i)
            if(!sentence_.candidateText(i).empty())++result.total;
        for(std::uint32_t i=0;i<std::min(result.total,static_cast<std::uint32_t>(config_.pageSize));++i)result.candidates.push_back(candidateAt(i));
        result.selectedCandidate=sentence_.selectedIndex()<static_cast<int>(result.candidates.size())?sentence_.selectedIndex():-1;
        return result;
    }
    const auto all = entry(); result.total = all.count;
    for (int i = 0; i < config_.pageSize; ++i) {
        const auto n = static_cast<std::uint32_t>(page_ * config_.pageSize + i);
        if (n >= all.count) break;
        result.candidates.push_back(candidateAt(n));
    }
    return result;
}
void Engine::appendHistory(std::u16string_view text) {
    history_.append(text);
}
void Engine::postprocess(const KeyEvent& key, KeyResult& result) {
    if(result.commit==u"{添加}" || result.commit==u"{加词}") {
        result.commit.clear(); result.openAddWord=true;
    }
    if(result.commit==u"{隐藏候选}") {
        result.commit.clear(); result.toggleHiddenCandidates=true;
    }
    if(!result.commit.empty()) {
        auto original=result.commit;result.commit=convert(result.commit);
        if(original!=result.commit)result.learning.clear();
    }else result.learning.clear();
    if (key.down && key.vk == Back && !result.handled && !history_.empty()) {
        const auto last=history_.pop();
        if (last == u"\u201c" || last == u"\u201d") { leftDouble_ = !leftDouble_; deletedDouble_ = true; }
        else if (last == u"\u2018" || last == u"\u2019") { leftSingle_ = !leftSingle_; deletedSingle_ = true; }
        else deletedSingle_ = deletedDouble_ = false;
    }
    if (key.down && !result.handled && !key.ctrl && !key.alt && !key.win) {
        if (letter(key.vk)) appendHistory(std::u16string(1,static_cast<char16_t>((key.shift != key.caps) ? key.vk : key.vk + 32)));
        else if (key.vk >= '0' && key.vk <= '9' && !key.shift) appendHistory(std::u16string(1,static_cast<char16_t>(key.vk)));
        else if (key.vk >= 96 && key.vk <= 105) appendHistory(std::u16string(1,static_cast<char16_t>('0'+key.vk-96)));
        else if (key.vk == Space) appendHistory(u" ");
        else appendHistory(symbol(key.vk,key.shift,true));
    }
    if (!result.commit.empty()) {
        appendHistory(result.commit);
        if (result.handled) repeat_ = result.commit;
    }
    if (key.down) {
        const bool committedDigit = !result.commit.empty() && result.commit.back() >= u'0' && result.commit.back() <= u'9';
        const bool passedDigit = result.commit.empty() && !result.handled && !key.shift && !key.ctrl && !key.alt && !key.win && digitKey(key.vk);
        if (committedDigit || passedDigit) digit_ = true;
        else if (!modifier(key.vk)) digit_ = false;
    }
}
}
