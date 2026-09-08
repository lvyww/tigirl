# Native input history

The engine now segments each committed or guessed pass-through string into
grapheme elements separately. Backspace on a passed key removes one stored
element, including an entire supplementary character, combining sequence,
regional-indicator pair or emoji ZWJ sequence. Separately committed base letters
and combining marks remain separate entries, matching original Core's
`AppendSendHistory`/`PostProcessKey` behavior. Quote recovery compares the whole
deleted entry rather than a trailing UTF-16 code unit.

Guessed pass-through history now follows the original virtual-key mapping:
letters respect Shift XOR CapsLock, digits include their shifted symbols,
numeric-keypad digits and English OEM punctuation are recorded, and Ctrl/Alt/Win
chords are excluded. An English pass-through Enter has no guessed history entry;
the Chinese idle Enter's explicit result text is recorded once. Guessed text is
appended before any result text, matching original postprocessing order.

`History.h` uses a persistent stack. Engine preview copies share existing nodes;
appending or popping in a preview cannot change live history. Uniquely owned
chains are released iteratively to avoid stack overflow. Like the original
engine, retained history grows with a typing session; no full session is copied
for every preview. The add-word adapter requests only the most recent 20 entries
and passes their boundaries intact to the dialog. History is still per TSF
context: process-wide/cross-application ordering and sharing remain unfinished.

`Grapheme.h` shares the former add-word segmentation algorithm with the engine.
Its read-only `GraphemeData.inc` is generated from the current Windows ICU's
Unicode 15.1.0 grapheme-break and extended-pictographic properties over all
Unicode code points. It does not use ICU's additional Indic-conjunct tailoring.
The generated table gives Linux and ARM64 the same properties without a Linux
ICU development dependency. Windows ICU remains used for add-word lowercasing.
Future Unicode upgrades require regeneration and original-runtime parity checks.

Regenerate by building `tests/ExportGrapheme.vcxproj` in Release/ARM64, then:

```
build/tests/ARM64/export_grapheme.exe > native/GraphemeData.inc
```

Validation:

- Original add-word/StringInfo corpus: 633 cases, zero differences.
- `tests/history_parity.py`: 633 commits and 4,919 append/pop actions per
  architecture, comparing the original StringInfo elements after each action.
  Every operation also checks copied-preview isolation. Each probe additionally
  creates/copies/releases a 100,000-entry history.
- Dynamic engine probe: separate base/combining-mark commits, copied Backspace,
  and whole deletion of emoji, accented text, emoji ZWJ and flag entries.
- Default key regression: 35,554 events per architecture, zero differences.
- `tests/history_key_parity.py`: 17,220 events per architecture compare actual
  original `PostProcessKey`/`GetLastCi(20)` history with the native engine in
  Chinese and English modes. All 256 virtual keys, Shift/Caps combinations,
  Ctrl/Alt/Win exclusion, keypad keys and a continuous history/pop sequence
  are covered. Reports: `build/history-key-parity-*.json`.
- Native dialog probe: separate commit boundaries survive the 20-entry limit
  and the more/less controls. Real TSF add-word and mixed fixtures pass.

Reports: `build/history-parity-*.json`,
`build/history-default-regression-*.json`, `build/add-word-parity-arm64.json`,
`build/add-word-ui-arm64.json`, and the private TSF reports.
