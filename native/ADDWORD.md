# Native manual add-word workflow

`Ctrl+=` opens the native add-word dialog by default. `Ctrl+等号手动加词` enables
it, and `手动加词快捷键` accepts the original shortcut notation, such as
`Ctrl+VK_E` or `Alt+Shift+VK_F12`. Invalid bindings fall back to Ctrl+=.
Reserved Ctrl+Space, word-reordering and native-hook bindings are excluded;
conflicting enabled add-word/recent-schema bindings disable the add action.
This does not implement the recent-schema or native-hook actions themselves.

Dictionary action entries `{添加}` and `{加词}` also request the dialog instead
of committing their literal token. Engine previews return the action flag but
cannot display UI or write a journal. Only successful real TSF dispatch queues
an open request. A message-only dispatcher opens the dialog after the edit
session unwinds; word segmentation and dialog creation run at that point.
Secure activation does not open the add-word UI.

The dialog initially selects the last two available text elements. The arrows
expand/shrink that suffix, editing the word recomputes its code, and the code
can be edited manually. The multiline word field accepts newline and Tab.
The add button saves and closes; continue-add saves, clears both fields and
retains the dialog; cancel writes nothing. Save errors remain visible in the
status area. This is a native Win32 dialog with system controls, not WPF.

## Data semantics

`AddWord.cpp` removes the original punctuation tokens before code inference,
looks up shared construct-code metadata, uses doubled lowercase ASCII letters
as a fallback, ignores unknown elements, and applies the original 1/2/3/4+
word-construction rules. It does not deserialize or duplicate the whole table.

Entry preparation trims the original Unicode whitespace set, decodes backslash
escapes in the original order (including its literal marker behavior), and
packs `display=>commit` aliases. Equal display/commit text stays unpacked.
Code normalization follows invariant lowercase, including supplementary letters
and the original preservation of capital I-with-dot.

Built-in Unicode 15.1 tables supply Unicode property/casing data. Segmentation implements the
reference StringInfo rules explicitly: default ICU character segmentation has
additional Indic tailoring and failed the original oracle. The native routine
now matches the tested original boundaries for those cases, combining marks,
emoji ZWJ sequences, supplementary characters and regional indicators. Unicode
property-version differences outside this corpus remain possible.

Saving calls `UserStore::commit` with the prepared add operation, so existing
cross-process journal locking/rebase/durability and candidate identity rules
apply. The owning service installs the returned immutable lexicon into its
contexts. Other hosts refresh on focus as before; live notifications and
journal compaction are still pending. The dialog holds only the mapped base
and edited-code overlay. Service deactivation closes the dialog/dispatcher;
a DLL lease protects window callbacks until the modal loop has unwound.

## Evidence and limits

- `tests/add_word_parity.py`: 633 original-oracle cases for construction, entry
  decoding, casing and segmentation; ARM64 passes. The model uses Windows ICU
  and is not part of the Linux engine build.
- `tests/add_word_key_parity.py`: 11 configuration profiles, 20,460 events;
  Linux and ARM64 match original results/snapshots and the open-dialog flag.
  Original oracle traces report actions without invoking a real dialog.
- `tests/add_word_ui.py`: actual native controls, default/suffix selection,
  automatic code, validation, two durable saves, continue-add, cancel and
  service-style close; three dialogs exit with zero retained DLL leases.
  It uses a disposable journal and no physical input events.
- `tests/dynamic_engine_probe.cpp`: six action-token/preview cases and seven
  selection-history cases. Keyboard selection now uses an internal raw-selection
  operation so the public mouse-selection postprocessor is not run twice.
- Default 35,554 events, 17,696 custom-selection events and 41,100 nondefault
  configuration events remain regression gates on both platforms.

Reports: `build/add-word-parity-arm64.json`, `build/add-word-key-parity-*.json`,
`build/add-word-ui-arm64.json`, `build/add-word-default-regression-*.json`.
The screenshot `build/add-word-dialog.png` is a PrintWindow capture of the test
process's client area, not a screenshot of typing into an application.

TSF-to-dialog opening, Chinese typing in the word field, IME bypass in the code
field and focus restoration after saving now pass in the controlled host.
Multi-monitor/DPI changes and broader application shutdown/focus behavior still
need interactive application verification. Current recent-text
storage remains per context. It now preserves per-commit grapheme boundaries
and removes complete elements on Backspace; see `HISTORY.md`. It does not yet
provide history shared across applications. The add-word UI is connected to the
current fixed Tiger schema; schema switching must close/rebind it later.

The window now limits its recent-text selection to the last 20 grapheme elements,
matching original `GetSendHistoryCount`/`GetLastCi`. The UI probe supplies more
than 20 elements including a supplementary-plane emoji, verifies the initial
two-element suffix, repeatedly expands to exactly 20 and shrinks to empty.
The adapter now supplies the engine's existing element boundaries directly, so
separately committed combining marks are not merged with earlier commits.
Cross-application history sharing remains unfinished.

`tests/run_add_word_tsf.ps1` exercises the current private DLL with a disposable
user directory in both UI-less and native-popup modes. Each run passes 74 key
events across two real TSF contexts: Ctrl+= opens the dialog, its controls save
a new word with the host's edit lock released, and both contexts can commit the
new word after the dialog automatically restores host and TSF document focus.
The maximum code length is
16 so the six-letter test code does not trigger ordinary auto-commit midway.
The fixture sends 12 keyboard events through Windows SendInput: `ab` plus Space
commits `交` in the word field and remains `ab ` in the code field. It verifies
foreground/field focus before each sequence. It then fills the final saved test
word/code with control messages. This is automated keyboard input in the native
dialog; it does not establish focus behavior across arbitrary applications or
when the user switches away during the dialog.
`build/tsf-private-add-word-validation.json` records matching DLL/host hashes
and unchanged registration; the temporary journal/configuration is removed.

The fixture also denies write-sharing on its isolated journal and dispatches
Ctrl+2. The failed adjustment must leave persisted candidate order visible,
and subsequent Space must commit the unchanged first candidate.

Empty-code and empty-raw-word validation now match the original dialog's
messages, validation order and focus target. Successful keep-adding returns
focus to the word field. The native dialog probe verifies all four focus cases
alongside its existing persistence and lifetime checks; it drives controls in
its own process, so this does not establish Chinese typing behavior in a host.

## Reproduce

Build ReferenceOracle, `tests/AddWordProbe.vcxproj`, `tests/AddWordUIProbe.vcxproj`
and `tests/EngineProbe.vcxproj` in Release/ARM64; build the Linux engine probe
using the command in `docs/PROGRESS.md`, then run:

```
python3 tests/add_word_parity.py
python3 tests/add_word_key_parity.py
python3 tests/add_word_key_parity.py --replay --windows
python3 tests/add_word_ui.py
```

The model/UI use built-in Unicode tables; the UI also uses `imm32.lib`,
`comctl32.lib`, `user32.lib` and GDI. Deployment keeps typing entirely in the
native TSF process with no resident Core or per-key IPC.
