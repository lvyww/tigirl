# Native TigerClaw: ordinary word input

Goal: deliver the ordinary TigerClaw word-input experience inside the TSF DLL,
with no resident Core or per-key IPC. Reference checkout is read-only:
`C:\Users\yc\Desktop\bime_codex_src_20260513`.

## Acceptance scope

- Native C++ key engine; independent composition per TSF context.
- Original table formats, file precedence, stable frequency ordering, duplicate
  handling, inferred phrase codes, display/commit aliases and escaped text.
- Shared, immutable, indexed binary tables: main lexicon, pinyin, annotations,
  splits, full/construct codes, prefix/unique/quick-symbol metadata. Do not
  deserialize complete tables into each host's private heap.
- Ordinary letters, extended codes, max-code auto commit, overflow/top commit,
  empty-code policy, Space/Enter/Tab/Escape/Backspace, candidate selection and
  paging, configurable selection bindings, short symbols, uppercase mode.
- Shift and Ctrl+Space switching, modifier/chord/repeat handling, CapsLock,
  Chinese/English punctuation, smart quotes, decimal points, pinyin reverse lookup.
- Candidate presentation: paging, horizontal/vertical layout, font/size, code,
  annotations/splits/full codes, caret anchoring and mouse selection.
- Configuration, table switching, recent-table shortcut, user add/delete/top/
  advance operations and persistence across processes without a resident service.
- ARM64 build/install/uninstall, practical application checks, documented
  architecture coverage. Preserve the installed daily TigerClaw runtime.
- Original Core is the oracle for deterministic dictionary and key trace checks.
- Multi-process mapped-file/private-memory measurements and input latency checks.

Sentence decoding and neural reranking are outside this first version. Ordinary
mixed-input behavior is tracked separately from sentence decoding; do not silently
claim unsupported options work. A short-lived import/settings utility is permitted;
normal typing must remain entirely native and in-process.

## Checkpoints

1. [x] Snapshot reference semantics and establish oracle/export tooling.
2. [x] Versioned mapped dictionary format, validated reader, real-table checks.
3. [ ] Native input engine with differential key traces.
4. [ ] TSF composition/candidate integration and native UI.
5. [ ] User data/configuration/update lifecycle.
6. [ ] Packaging, memory/latency/application validation, completion audit.

## Progress

- Native ARM64 TSF builds and is registered as 原生虎码 independently of daily
  TigerClaw. The original SampleIME DLL remains available as a rollback target.
- Source inspection confirms original Core loads main and auxiliary tables into
  private dictionaries; SampleIME maps text read-only but performs linear scans.
- Implementation and component parity checks are underway; full ordinary-input
  runtime/configuration parity is not yet established.
- Shared reader passes exhaustive 757,399-record parity and ARM64 multi-process
  physical-page sharing checks. Initial key engine passes 33,424 reference events;
  full ordinary-operation coverage and TSF/user-data lifecycle checks remain pending.
- See `docs/PROGRESS.md` for exact evidence, limitations and oracle registry isolation.
- See `docs/ACCEPTANCE.md` for requirement-by-requirement gaps and current
  architecture findings; `docs/INSTALL.md` documents the actual script workflow.
- User-word snapshot overlay and journal now pass concurrent-writer/recovery
  tests on ARM64 and Linux; adjustment shortcuts pass 35,554 key events against
  original Core. v2 preserves empty exact-code metadata. These components still
  have TSF/native-candidate integration. Configuration and full UI/lifecycle
  coverage remain pending; component tests do not complete these checkpoints.
