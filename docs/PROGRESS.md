## Donation settings tab

- Add a fifth native tab displaying the user-provided donation PNG, embedded
  unchanged as RCDATA and decoded with Windows WIC. Preserve its aspect ratio.
- Resize tab headers and keyboard cycling for five pages; keep font-dropdown
  test capture separate from the new page index.
- ARM64/x64 builds and 96/144/192-DPI settings regression pass. Source image
  SHA256 matches the supplied original; phone scanning is not verified.
- Source/build only; not installed or included in the distribution installer yet.

## Tiger-colored settings

- Add cream window and warm-white page backgrounds, brown labels and muted
  descriptions. Preserve classic input controls and the compact layout.
- Owner-draw native tab headers with a light-orange selected fill and orange
  indicator, retaining tab labels, keyboard navigation and focus rectangles.
- Use system palette in high contrast and repaint on system color/theme changes.
- ARM64/x64 builds and isolated 96/144/192-DPI settings regression pass.
  Screenshots reviewed; live high-contrast switching was not exercised.
- Source/build only; local deployment and distribution installer remain unchanged.

## Compact native settings styling

- Keep the existing dense four-page layout; replace page buttons with a native
  tab control, including Ctrl+Tab / Ctrl+Shift+Tab navigation.
- Use DengXian for interface text and classic native control styles; unify
  static/checkbox backgrounds with the page while preserving font previews.
- Put all page controls in a child container inside the full native tab frame,
  using its adjusted display rectangle. Keep white editable fields on a system
  gray page; rely on classic control borders without additional group boxes.
- Distinguish descriptions with 13-DIP muted text while option labels stay
  16 DIP; preserve high-contrast text and prominent save errors.
  Description styling is built/tested but not installed: local UAC was canceled.
- Build ARM64 and x64 settings tools. Isolated real-control tests pass at
  96/144/192 DPI, including save/cancel, validation and concurrent configuration.
- Restore settings regression execution through a dedicated probe executable;
  the removed scheme-manager CLI is no longer used to enter these tests.
- Installed locally as ARM64X generation f41e6fce1d53448e with x86 support.
  Manager hash, registration, URI and shortcut targets verified; evidence:
  build/settings-style-installed.json. Distribution installer remains unchanged.

## Locked DLL uninstall cleanup (2026.9.10.2)

- Fix PowerShell string marshaling at MoveFileEx: pass native NULL with
  NullString.Value so occupied DLLs can be deleted after restart.
- Retain the active cleanup scripts and manifest until Inno completes uninstall;
  retry cleanup accepts already removed files and skips redundant unregistration.
- Report scheduling errors with their Win32 code, log unregistration and cleanup
  separately, and avoid labeling every uninstall error as a registration failure.
- Add locked-file regression coverage using a real exclusive file handle and an
  intercepted native scheduling boundary; no reboot registry changes in tests.
- Real Intel/AMD x64 uninstall and reboot acceptance remains pending.

## Empty-code ambiguity includes duplicate singles (upstream 1784df1)

- Empty-code capture now includes decoder-approved duplicate-single paths in
  both uniqueness and strong-confidence comparisons. Eligibility travels with
  the candidate snapshot, so it uses the producing decoder's settings.
- Exact complete-path queries also count legal non-first single characters,
  even when Beam or visible-candidate limits hide those alternatives. Non-first
  multi-character words still need explicit selection; the disabled switch
  retains first-rank grouping.
- Regression fixture: ot = 是/题/多字词, qm = 目. With duplicate singles enabled,
  otq must not implicitly submit 是; otqm can rank 题目 first.
- Tests: tests/sentence_empty_code_test.ps1 builds the new Engine/decoder probe
  plus Tab-lock, rumination and session regressions for x64, Win32 and ARM64.
- Rebuilt the x64/x86 graphical installer as 2026.9.10.1 with this fix.
  Package hashes, binary versions, architecture loaders, initialization and runtime
  fixtures pass. ARM64 rejection preserves live registration and configuration.
  Evidence: build/setup-2026.9.10.1-validation.json. The installed ARM64X DLL
  remains unchanged; real Intel/AMD x64 installation acceptance is still pending.

## Single-file x64 graphical installer (2026.9.9.6)

- Added the Chinese Inno Setup wizard, branded artwork, offline x64/x86 payload,
  original-user initialization, and optional settings launch.
- Added persistent machine/user recovery, immutable version repair, old-package
  adoption, downgrade rejection, and owned-version cleanup.
- Added a nonresident graphical uninstall controller with default data retention,
  confirmed current-user-only deletion, and deferred cleanup of locked binaries.
- Local validation covers data merge, interrupted transactions, mixed registration
  recovery models, package runtime, and UI previews. Real Intel/AMD x64 machine
  installation, multi-user, UWP, and restart/uninstall acceptance remains required.
- Build and operational details: docs/GRAPHICAL-INSTALLER.md. This development
  does not replace the ARM64X version currently installed on this ARM64 host.

## Font selection and previews aligned with TigerClaw

- Replaced the three-entry editable font field with a real font picker: bundled
  TTF families first with # labels, then all DirectWrite system font families.
  Prefer Chinese names, retain English-family aliases, deduplicate case-insensitively
  and select the first entry if a configured font cannot be found.
- Owner-drawn dropdown rows and the selected field use their own font through
  DirectWrite/Direct2D. Preview uses the opening font size, as in the reference;
  saves retain the existing atomic edited-field behavior and cancellation is inert.
- Actual settings tests pass with 250 font choices, alias and fallback checks,
  existing save/reopen/cancel and concurrent-field preservation tests. Native
  control captures at 96/144/192 DPI were inspected, including the dropdown rows.
- ARM64X package generation 201bcce54c555a00 contains the new shared ARM64 manager.
  Installed-file verification is recorded in build/font-settings-installed-audit.json.
  UI behavior/reference mapping: docs/FONT_SETTINGS.md.

## First candidate has no highlight

- At the user's request, the native renderer no longer paints selection fill for
  the first visible candidate. Text, hit rectangles, TSF selection and commit
  behavior are unchanged; a selected later candidate still has its highlight.
  The supplied TigerClaw sentence formatter similarly hides selection index 0.
- Existing renderer probes now compare first-selection pixels to no-selection
  pixels and confirm selecting the second candidate changes them. All 270 cases
  pass across ARM64/x64/Win32, nine themes, horizontal/vertical layouts and five
  DPI samples. The cyberpunk 150% image was visually inspected.
- Updated mouse-test assumptions to select the second candidate before locating
  a painted selection; those foreground tests have not been rerun for this change.
- Evidence: build/directwrite-render-validation.json. Installation validation is
  recorded separately in build/first-candidate-highlight-install.json.

## Tiger-style taskbar menu installed

- Added folder, export, reload, add-word, checked scheme/theme submenus, settings,
  manager and original-project GitHub entries to the native input-mode button.
  Both TSF InitMenu and the Win32 popup use the same menu snapshot. Pure TSF has
  no resident Core to exit; the final action switches to English.
- Slow imports/exports run in short-lived schema_manager processes. Export merges
  the current base with the user overlay and writes a unique UTF-8 file under the
  user export directory. Source tables and user journals remain intact.
- Companion action tests pass for theme, schema switch, reload and export with
  added words. ARM64/x64/Win32 menu contract/launch checks pass; ARM64 popup
  selection and focus-return checks pass. ARM64 and Win32 fixtures additionally
  open/cancel add-word and verify the English action's system compartment.
- Installed ARM64X generation dc8185042a242e42 and matching x86 component. All 13
  primary artifact hashes, x86 hash, both registry views and shared hard links
  verified. Ctrl+Space remains delegated to the system by the user's configuration.
- Evidence: build/tray-menu-installed-audit.json, build/tray-menu-actions.json,
  management-actions-{arm64x-ARM64,arm64x-x64,Win32}.json and
  management-menu-arm64x-ARM64.json. User taskbar clicking is not claimed.
  Usage and scope: docs/TRAY_MENU.md.

## Use the system Ctrl+Space shortcut

- At the user's request, disabled the native engine's Ctrl+Space handler through
  the existing `Ctrl+空格切换中英文` setting (`否`) in the actual user configuration.
  Published a new settings reload token, retained the selected 虎整句 scheme and
  all unrelated settings, and backed up the previous configuration in build/.
- Windows IME hotkey registration and installed binaries are unchanged. Existing
  external compartment handling remains responsible for synchronizing the native
  mode and language-bar state after the system switches. Application confirmation
  is pending; this configuration change is not yet proof of a successful fix.
- Evidence: build/system-ctrlspace-config.json.

## Ctrl+Space actual-application regression reported after installation

- User reports Chinese changes to English on Space down but returns to Chinese
  on Space up. Prior direct TSF callback coverage is insufficient to establish
  this shortcut works through the operating-system input path.
- Added a foreground Rich Edit probe that sends individual Ctrl/Space edges and
  checks mode after Space down/up and Ctrl up, in both toggle directions. The
  x64 probe builds. Execution has not started: the desktop input-idle prerequisite
  failed twice. Production DLLs are unchanged; no fix is claimed.
- Read-only inspection found the system IME hotkeys also assign Ctrl+Space.
  Duplicate system handling is a hypothesis requiring a reproduced trace.

## Installed sentence trial: 2026-09-09

- Installed ARM64X generation `56502c3a10a47997` and matching Win32 DLL.
  Both registry views, all 13 primary artifact hashes, x86 DLL hash, and all
  shared artifact hard-link identities were verified after installation.
- Foreground standard Rich Edit probes passed on ARM64, x64 and Win32 using
  system registration and the installed DLLs: 27 injected taps per architecture,
  covering commit, Escape, context return, selection, Backspace and replacement.
  These use isolated ordinary-schema settings; they do not establish sentence
  acceptance in Word, WeChat or Pain.
- The separate hidden registered-activation fixture could not complete because
  no English keyboard layout was already loaded. This is recorded as an unmet
  test precondition, not a pass. No user language configuration was changed.
- Evidence: `build/sentence-installed-audit.json` and
  `build/rich-edit-tsf-{ARM64,x64,Win32}.json`. Actual sentence application
  feedback remains pending; the documented long-burst x86 limitation remains.

## Sentence settings visual review on an isolated desktop

- Added native-control capture to the existing settings test. Initial hidden
  window captures omitted edit contents, so tests now launch on a unique,
  inactive desktop and only show the dialog when its desktop name is distinct
  from the input desktop. No SwitchDesktop or physical input is used.
- WM_PRINT captures at 96/144/192 DPI were visually inspected: labels and
  buttons do not overlap, numeric values render, and the long whitelist occupies
  its horizontal edit field. Pixel checks reject blank input-field captures.
  Existing save/reopen/cancel/validation and selection tests still pass.
- Evidence: build/input-settings-arm64.json and sentence-settings-96/144/192.png.
  This reviews actual control rendering, not active-desktop composition or
  cross-monitor dragging. The updated manager is being included in the trial
  package; installed application acceptance remains required.

## Real-table timing through actual TSF edit sessions

- Added a separate --sentence-measure host mode and reproducible runner. The
  isolated fixture uses real source tables and the packaged model; the frozen
  original full engine supplies expected commit text. All 54 compositions match
  across ARM64X ARM64/x64 and x86: three repeats of 32/64/128 codes, paced at
  30 ms or sent as an unpaced burst, automatic commits off.
- Paced encoding-key p95 medians range about 0.75–2.04 ms. Paced 128-code Space
  takes about 18.9/26.8/30.3 ms in ARM64/x64/x86; immediate Space after a burst
  takes 132/167/310 ms. The long-burst commit path remains noticeably slower,
  particularly in x86. This is actual TSF callback/edit-session timing, not a
  standalone decoder timing or an installed application latency guarantee.
- Samples record active and post-commit private bytes. After long commits, the
  allocator can retain memory, which later short inputs can release again;
  three rounds are insufficient to claim bounded memory or absence of leaks.
- Evidence: build/sentence-tsf-measure-all.json and preserved raw/original
  outputs. Reproduce with tests/sentence_tsf_measure_test.py; measurement details
  are in docs/SENTENCE_PERFORMANCE.md. Production code and installation unchanged.
  The next acceptance work is settings visual review and installed application
  input, while retaining the measured long-burst limitation explicitly.

## Compact candidate deduplication index

- Bucket deduplication now stores text hashes and vector positions instead of
  another owned copy of every candidate string. Complete text equality resolves
  collisions; moved vectors do not invalidate numeric positions, and sorted
  buckets discard/rebuild the index. Rank representative selection and merged
  probability arithmetic are unchanged.
- Same real-table benchmark, three-run full-decode medians: 32 codes 3.64 →
  3.54 ms, 64 codes 58.00 → 53.83 ms, 128 codes 200.58 → 174.78 ms. The 128-code
  process still uses about 101 MiB private memory. This is a latency improvement,
  not a solution to long-input retained state or proof of installed latency.
- All three architectures pass neutral/real-model full/incremental decoder
  differentials. All eight real-table full-engine configurations pass (10,850
  events each). Current standalone ARM64, ARM64X ARM64/x64 and hard-linked x86
  pass bundled-model TSF journal/resource/automatic/deferred-result fixtures;
  ordinary staged TSF regression passes as well. ARM64X preflight passes.
- Measurement details: docs/SENTENCE_PERFORMANCE.md and
  build/real-sentence-measure/index-comparison.json. Current package is rebuilt;
  installed files remain unchanged. The earlier 18 synthetic configurations are
  historical evidence; this optimization was checked with the full decoder
  differential and eight real-table configurations described above.

## Computed results delayed at the TSF edit-session boundary

- Extended the real hidden TSF fixture to defer the text-store edit lock after
  input and wait for an actual pending request. A positive control grants the
  lock and verifies the decoded 中国 candidate, establishing that this path
  delivers a computed result rather than merely deferring worker capture.
- Separate cases switch contexts or lose thread focus before granting that lock.
  Neither document text nor candidate UI begin/update counters may change when
  the delayed callback is released. Returning to the original context and
  canceling also preserves the previously committed document text.
- ARM64X ARM64/x64 and hard-linked x86 package layouts pass with the bundled
  model, together with all existing journal/base/schema reload and automatic
  commit tests. Hosts report explicit deferred-positive/context/thread-focus
  flags; the runner rejects older hosts without these checks. No production
  code or installation changed. This closes the computed-result transit gap for
  focus transitions; it is not an original-engine schedule differential or an
  exhaustive permutation of resource changes and teardown.
- Evidence: sentence-tsf-arm64x-journal-bundled-validation.json and
  sentence-tsf-x86-journal-bundled-validation.json under build. Long-input
  latency/private state, settings visual review and installed input acceptance
  remain before completion.

## Packaged non-neural model and default TSF resource lookup

- ARM64 and ARM64X build scripts now take SentenceModelPath, validate the fixed
  reference model's length/SHA256 and copy it into Models with provenance.
  Install preflight includes both files in the package identity, rejects missing
  or mismatched models before elevation, and verifies deployed artifact hashes.
  The x86 installer already hard-links every primary companion artifact, so the
  new model follows the same shared-file path without a second backing copy.
- Both complete build scripts and package preflights pass. ARM64X negative
  fixtures reject missing/corrupt models before elevation as well as wrong/missing
  architecture loaders. Records: native-package[-arm64x]-preflight.json in build.
- Real hidden ARM64X ARM64/x64 TSF tests pass with no model-path override, using
  the bundled model and importer for input, active journal/base/schema reload,
  automatic prefix and empty-code commits and pause timing. The equivalent x86
  layout passes too; every shared artifact's hard-link identity is checked in an
  isolated directory. Records: sentence-tsf-arm64x-journal-bundled-validation.json
  and sentence-x86-package-validation.json. No installed files or registration
  changed. The x86 fixture does not run the actual privileged installer.
- Remaining release gates include computed-result transit delays, long-input
  latency/private state, settings visual review and installed application
  acceptance. Model packaging is now implemented, not installed acceptance.

## Native sentence settings page

- Added an independent sentence page to the development input settings dialog:
  schema-name activation, automatic commits, duplicate single characters,
  retained raw 0–32, common-character limit and explicit-empty full-code whitelist.
  It uses the production SentenceSettings parser and defaults, with no Qwen UI.
- Saves only changed fields through the existing atomic configuration writer and
  settings reload marker. Existing model-path overrides and unrelated fields are
  preserved. Eight invalid numeric cases are rejected before writing; retained
  32/common zero/empty whitelist and all three flags save and reopen correctly.
- Updated the hidden native settings test to run through the Windows runner.
  Existing appearance, shortcuts, selection repair and concurrent-field tests
  pass with the sentence controls, including cancel and 96/144/192 DPI bounds.
  Evidence: build/input-settings-arm64.json; usage: docs/SENTENCE_SETTINGS.md.
  The ARM64 companion builds successfully. Installed tools are unchanged.

## Real-table full-engine acceptance traces

- Added an opt-in real-table mode to sentence_engine_parity.py, copying the
  supplied 虎整句 main table, quick symbols and supplementary corpus into the
  isolated original/native fixture. Original source files and installed files
  are untouched. Input codes are derived from source text independently of the
  native importer. Reverse lookup retains the small synthetic ni fixture.
- Twelve phrases exercise both shortest/full codes, Space/punctuation, explicit
  suffix selection, Tab/arrows, repeated deletion and cancellation: 10,850 key
  events per configuration. The native adapter now parses sentence settings as
  production does, including the default 1,500-character rule and whitelist;
  previous synthetic fixtures retain their explicit zero-character setting.
- Eight configurations pass with zero differences: automatic-off on all three
  architectures, controlled automatic bursts on all three, plus ARM64 paced
  automatic and burst retained-raw-zero. Paced automatic produces 471 commits;
  retained-three bursts produce 142, retained-zero bursts 176. Candidates,
  selection, composition, output and cumulative committed raw boundaries agree.
- Reproduce with `python3 tests/sentence_real_engine_test.py`; use
  `--summarize-only` to audit existing reports against source/table/binary hashes.
  The matrix is build/sentence-real-engine-matrix.json. The same 10,850 events
  are replayed eight times; this is not 86,800 distinct input fixtures or a
  comprehensive accuracy benchmark. Computed-result transit delays, actual TSF
  latency, settings UI, packaging and installed application acceptance remain.
- Refreshed sentence-tsf-integration.json only after checking the current DLL
  hashes against all six sentence reports and ordinary staged activation.
  SENTENCE_PORT.md now distinguishes current development status from historical
  checkpoints. No production source changes or installation in this checkpoint.

## Bounded scoring reuse and smaller decoder context states

- Added an exact-key, direct-mapped 4,096-slot n-gram query cache to each lattice
  (128 KiB per allocated cache). It covers tokens up to two UTF-16 units, checks
  all key units on hits, bypasses longer tokens/custom models, and is destroyed
  with the lattice. Model/options remain immutable; no shared mutable cache.
- Avoided repeated grapheme/vector/string allocation in isolation scoring for
  candidates composed entirely of basic Han or ASCII letters. All other Unicode
  text uses the unchanged full grapheme path. Context tokens in beam states now
  point into the retained immutable lexicon instead of owning two strings.
- Original decoder parity passes all three architectures in neutral/real-model,
  full/incremental variants, including scores, order and early-commit evidence.
  All 18 full-engine configurations also pass. Current development DLLs pass
  all four sentence TSF host/build combinations with and without user journals.
- Same real-scheme workload, three-run medians: 32 codes 5.03 → 3.64 ms;
  64 codes 83.41 → 58.00 ms; 128 codes 281.63 → 200.58 ms. At 128 codes,
  process private memory falls from 117.10 to 101.32 MiB. Incremental p95 varies
  from 26.8–36.0 ms and later-phase retained heap use remains variable, so this
  does not establish an overall latency/memory acceptance threshold.
- Measurement comparison: build/real-sentence-measure/score-comparison.json.
  Source/binary hashes and limitations are recorded in the current validation
  report and docs/SENTENCE_PERFORMANCE.md. Long-input tuning, real-table accuracy,
  packaging and installed application acceptance remain. Installed files unchanged.

## Real-scheme memory sharing and decoder cost baseline

- Added optimized standalone measurement probes and a repeatable runner using
  the reference 虎整句 scheme (14,429 records), copied to an isolated build root.
  The original source, installed DLLs and production decoder were left unchanged.
- Concurrent ARM64/x64/x86 readers demonstrate actual multiply shared physical
  pages for all 58,299 model pages and all 682 sentence-index pages. Both mappings
  are read-only MEM_MAPPED views. Private resource-loading increments are about
  0.54–0.62 MiB, rather than a private 228 MiB model copy per process.
- Four fresh-process 32-code decoder contexts add about 5.48 MiB total private
  memory. Long decoding remains expensive: three-run full-decode medians are
  5.03 ms at 32 codes, 83.41 ms at 64, and 281.63 ms at 128; the 128-code process
  has about 117 MiB private memory. Incremental 64-code p95 is about 32–34 ms.
- An experimental post-pruning capacity trim saved memory but increased measured
  decoding time, so it was reverted. No unverified optimization was installed.
  Details, measurement limits and reproduction are in docs/SENTENCE_PERFORMANCE.md;
  hashes/results are in build/real-sentence-measure/validation.json.
- This establishes real cross-process/architecture sharing, not installed TSF
  latency acceptance. Long-input state memory and commit-key latency need work,
  followed by real-table accuracy, packaging and installed application acceptance.

## Controlled burst traces and empty published-candidate correction

- Added a seeded per-key settle schedule to the full original-engine adapter.
  It holds the original reentrant engine lock across deferred events, preventing
  its unchanged worker from capturing work, then releases and settles at selected
  boundaries. Native decoding is deferred equivalently; manual commit/selection
  keys still complete decoding. Original method bodies remain unchanged.
- Retained-raw-zero bursts exposed 176 differing events: after empty-code commit,
  a pending old candidate became an empty suffix and native UI still published
  it as an empty selected row. Original GetPublishedSentenceCandidates omits
  these rows. Engine snapshot totals, candidateAt and selected-row validity now
  follow that projection without deleting internal decoder/policy candidates.
  Before-fix evidence: `build/sentence-empty-row-before.json`.
- All 18 architecture/configuration/schedule combinations pass: ARM64/x64/x86,
  automatic off/on with retained 3/on with retained 0, settled and burst modes.
  Each replays 4,042 events; burst modes defer 2,968 events. Current source and
  executable hashes are checked by `build/sentence-engine-matrix.json`.
- This verifies queued work before capture, including delete/cancel/selection
  while results are pending. It does not inject an already-computed result held
  in transit; that stale-publication schedule remains separate. Larger real
  schemes, memory/latency, packaging and installed acceptance are still required.

## Full-engine parity across ARM64, x64 and x86

- SentenceTraceProbe now builds independently for ARM64/x64/Win32 with isolated
  intermediate/output directories, matching source lists and warnings as errors.
  The runner selects a platform via SENTENCE_TRACE_PLATFORM and keeps each
  architecture/configuration report and complete trace separate.
- All nine combinations pass against the full frozen original engine: automatic
  disabled, paced asynchronous automatic input with retained raw 3, and retained
  raw 0, on each of the three native architectures. Each replays the same 4,042
  events; this is 36,378 replayed events, not 36,378 distinct fixtures.
- Each architecture agrees on candidates, selection, raw/display, mode, output
  and cumulative prefix boundaries, including 14 and 175 automatic outputs in
  the two automatic configurations. All source and binary hashes were checked
  before writing `build/sentence-engine-matrix.json`.
- This extends the previous ARM64-only full-engine evidence. Production DLLs
  and installed files were not changed in this checkpoint. Rapid bursts with
  controlled delayed/stale publication, larger real schemes, memory/latency,
  packaging and installed application acceptance remain required.

## Match asynchronous automatic-commit timing to the original

- Removed automatic-prefix evaluation from TSF decoder completion. Completion
  now only publishes candidates; the next appended key consumes already-decoded
  evidence, matching original AppendSentenceInput and its UI-only completion
  callback. This avoids spontaneous document commits while the user pauses.
- Extended the full original-engine driver to run the actual asynchronous worker
  and settle it after each key. Comparisons now also include cumulative committed
  text and raw boundary. On ARM64, all three 4,042-event suites pass: automatic
  off, automatic with retained raw 3 (14 early outputs), and retained raw 0
  (175 early outputs). This covers both prefix and empty-code policy interaction.
- The legacy completion-time simulation fails 61 events on the same paced
  automatic trace. More decisively, the new real TSF pause fixture fails on the
  previous DLL: it commits after six c keys. Evidence is retained in
  `build/sentence-timing-before.json`. Current standalone ARM64, ARM64X ARM64/x64
  and x86 pass: six c keys plus a pause leave raw composed, the seventh commits
  明, and eighth/Space finishes 明明明明 without duplication or loss.
- Reports: `sentence-engine-parity.json`,
  `sentence-engine-async-auto-parity.json`,
  `sentence-engine-async-auto-retain0-parity.json` under build, alongside current
  TSF journal reports. Original trace modes have separate saved JSONL outputs.
- This closes the previously identified completion-time commit discrepancy.
  The full-engine asynchronous trace uses a settled-per-key schedule; rapid
  bursts and deliberately delayed/stale publications, larger real schemes,
  cross-architecture full-engine traces, memory/latency, packaging and installed
  application acceptance remain required. Installed files unchanged.

## Full original-engine baseline and empty selection correction

- Added a separate SentenceEngineOracle compiling the frozen original full
  InputMethodEngine and CoreRuntimeState with their model, decoder and source
  dependencies. Dependency hashes are recorded and checked. A staging marker
  and process-local HKCU redirection isolate initialization; no rerank client,
  pipe server or Overlay is started. Original method bodies are unchanged.
- Added an ARM64 native trace adapter and 3,852 physical-key events covering
  settled sentence decoding, Space/Tab/up/down selection, selectors, deletion,
  cancellation, punctuation, short-symbol and reverse-lookup cases, plus seeded
  random traces. Both use the real model and matching duplicate-character rules.
- Full original/native comparisons pass for handled/cancel, language/mode, page,
  raw, output, candidate order/annotations, selected row and displayed code.
  The expanded selected-row check found 1,684 mismatching events caused by one
  state bug: no sentence candidates still reported selected row zero. Engine
  now reports -1, matching the original; all 3,852 events pass after the fix.
- Evidence: `build/sentence-engine-parity.json` and both complete JSONL traces.
  This baseline deliberately uses synchronous settled decoding and automatic
  commits disabled. Automatic/empty-code full-engine traces, asynchronous result
  schedules, larger user schemes and other native architectures remain required.
  It does not replace installed application acceptance or performance checks.
- Follow-up found by reading the original asynchronous path: decoder completion
  only publishes candidates (ProtocolHandler registers PublishUiState); runtime
  probabilistic prefix commits are evaluated on the next appended key. The native
  pollSentence currently also evaluates automatic commit on completion. This
  timing difference is not covered by the settled/auto-off baseline and must be
  corrected and tested with an explicit asynchronous schedule before acceptance.

## Empty-code commit policy integrated into Session/Engine/TSF

- Ported the original capture-before-append and resolve-after-append decisions:
  matching current results, implicit first-rank eligibility, strong visible-top
  confidence, unpruned complete paths, last-segment proper-prefix deferral,
  configured retained raw and excluded-text/group-eligible uniqueness checks.
  Pending candidates survive multiple letters and reset on selectors, deletion,
  selection, invalidation, disabled settings or contradictory complete paths.
- Empty-code commits retain the full raw sentence and committed prefix; subsequent
  decode results apply the original first-rank continuation restriction unless
  explicit selection markers occur. The final document commit excludes previously
  committed text. Probabilistic prefix commits remain a distinct policy.
- Engine receives read-only path-query callbacks per dispatch. TSF supplies the
  accepted revision's decoder for both copied previews and actual key handling;
  Engine stores no decoder callbacks. These existence queries use unpruned path
  traversal, not Beam decoding or IPC. Their key-path latency remains to measure.
- Fixed backspace to reset probabilistic evidence as well as empty-code pending
  state, matching the original input handler.
- Session tests pass 224 checks per ARM64/x64/x86. Hidden real TSF tests pass
  standalone ARM64, ARM64X ARM64/x64 and x86: cc produces 明, appending a commits
  明 while preserving an active tail, and a/Space finishes 明中 without loss or
  duplication. Existing automatic-prefix and resource-transition tests also pass.
  Ordinary key parity passes 35,554 events with zero differences.
- These fixtures are native state assertions and actual TSF checks, not a complete
  original-engine differential. Extracted original empty-code/randomized traces,
  broader short-symbol/reverse transitions, performance/memory, packaging and
  installed application acceptance remain required. Installed files unchanged.

## Automatic-prefix commits integrated and verified in TSF

- Connected SentenceAutoCommit to the value-copy SentenceSession, Engine key
  append path and TSF decoder completion edit session. Approved prefixes update
  committed text/raw together, retain full decoding context, invalidate older
  tickets and keep the uncommitted tail composed. Probabilistic prefix commits
  preserve non-first-rank continuations; empty-code continuation is separate.
- Session probes pass 177 checks on ARM64/x64/x86, covering copied previews,
  current and immediately previous evidence, merged-tail boundaries absent from
  visible candidates, selected-candidate suspension, retained raw, stale tickets
  and final commits without repeated prefixes. The standalone original-policy
  differential passes 10,400 steps / 489 commits per architecture after rebuild.
- Current standalone ARM64, ARM64X ARM64/x64 and x86 DLLs pass hidden real TSF
  journal/resource-transition tests including automatic prefix commit before
  Space, remaining live composition, and final exact complete text. No-journal
  tests also pass all four combinations. Ordinary staged activation passes its
  three ARM64/ARM64X host combinations with registration unchanged; ordinary
  key parity reports 35,554 events and zero differences.
- Current DLL hashes and report links are recorded in
  `build/sentence-tsf-integration.json`. Component reports have narrower scopes
  than that integration report. Installed files remain unchanged.
- Next: original empty-code policy (capture before append, unpruned complete-path
  checks, proper-prefix deferral, retained-tail limit and hidden alternative
  uniqueness), full original-engine traces, short-symbol/reverse transitions,
  performance/memory, packaging and installed application acceptance.

## Native automatic-prefix commit policy

- Added copyable `SentenceAutoCommit` state: current/immediately-previous decoded
  generation checks, consecutive evidence and strong-evidence counts, neutral-gap
  retention, exact raw-boundary support, contradictory stems, supplementary-top
  restriction, visible-path membership and merged-tail behavior. Mature selection
  respects retained raw length, grapheme length, share, raw-boundary tie order and
  distance from the last automatic commit. Qwen acceptance is excluded.
- Generated a separate oracle from unchanged frozen original policy method
  bodies. UI/rerank and post-commit candidate filtering are stubbed only in this
  policy adapter; each following fixture supplies fresh decode evidence. This
  does not replace the still-required full original-engine trace comparison.
- All three architectures match 10,400 stateful steps / 489 commit decisions,
  including cumulative committed text and raw boundaries. Evidence:
  `build/sentence-auto-commit-validation.json`.
- The same sequence also evaluates a copied policy before each real evaluation;
  all three architectures preserve the original counters/decisions, checking the
  value-copy behavior required by Engine key previews.
- Not yet connected to SentenceSession/Engine/TSF document commits. Session
  integration must support immediately-previous evidence and merged-tail prefix
  boundaries, apply prefix bookkeeping atomically, and retain the remaining raw.
  Empty-code policy is still pending. Production/installed DLLs are unchanged.

## Preserve active sentences across base generations and sentence schemas

- Extended accepted-snapshot retention to replacement base generations and
  transitions between two enabled sentence schemes. The previous complete pair
  remains usable during preparation; adoption switches both ordinary lexicon
  and sentence resources, preserving raw keys through Engine::switchSchema.
  Explicit disabling or switching to a non-sentence scheme still clears sentence
  resources immediately under the existing mode rules.
- Added a real TSF regression that first failed on the previous ARM64 DLL:
  publishing a new base generation canceled the active sentence. The failure
  report is retained in `build/sentence-base-reload-before.json`.
- Updated tests pass standalone ARM64, ARM64X ARM64/x64 and x86. With the same
  active aabb input, a new base produces 中州, then another sentence scheme
  produces 中文. Each transition checks retained composition/raw and final text,
  alongside the existing delayed-helper and live-user-edit checks.
- This verifies these resource transitions in isolated hosts, not installed
  application acceptance or complete sentence engine parity. Automatic/empty-code
  commit policies, short-symbol/reverse transitions, performance and packaging
  remain pending. The installed version is unchanged.

## Preserve active sentences through same-base user reloads

- Separate requested resource revisions from accepted resource revisions.
  While a same-base replacement builds, Engine/decoder continue using the last
  complete immutable lexicon/resource pair. Candidate commits during that window
  intentionally use that accepted version, never a mixture of new user entries
  and an old index. Failed preparation retains the last usable pair for retry.
- Adoption changes the accepted pair together and invalidates obsolete decoder
  results while retaining the current raw sentence. Ordinary no-sentence modes
  and explicit sentence disabling still use their existing behavior.
- Hidden tests now hold the future cache's publication lock to delay the helper.
  With a live sentence, Back/letter/Space succeeds on the accepted version; another
  sentence remains composed while the lock is released and then commits using
  the new user data. Pending aabb legitimately becomes segmented aa bb, so the
  test verifies codes, live composition and final committed text separately.
- These tests pass standalone ARM64, ARM64X ARM64/x64 and x86. This closes the
  previous same-base journal reload cancellation gap. New base generations/schema
  switches, selection behavior across reload, additional failure races and full
  original engine traces remain to verify. No installed files were changed.

## TSF user-journal sentence resource integration

- Removed the unconditional edited-code sentence gate. The native resource
  worker captures an immutable Lexicon snapshot, hashes its revision, reuses a
  validated shared cache or launches the short-lived importer to prepare it.
  No full sentence table is rebuilt in a TSF host and no per-key IPC was added.
- Resource completions carry their source snapshot. Before adoption TSF polls
  data changes and checks both service generation and snapshot equivalence;
  Engine enabling also requires resources matching the current Lexicon.
  Journal refresh now requests resources after publishing the refreshed snapshot.
- Cache helper launch has correctly quoted arguments, no inherited handles or
  console, and a 30-second worker wait. Service teardown stays nonblocking; a
  timed-out helper can finish its immutable cache publication independently.
- Hidden real TSF tests pass standalone ARM64, ARM64X ARM64/x64 and x86. The
  base fixture deliberately lacks 中; only the user journal supplies it. All
  sentence input/selection/cancel/context tests pass, and an independent journal
  deletion while TSF stays active changes subsequent 中国 input to 中华.
  Reports: `build/sentence-tsf*-journal-validation.json`.
- No-journal sentence tests pass all four host/build combinations as well.
  Ordinary activation/mode/edit-lock regression passes all three staged variants
  with registration unchanged. Current hashes and both sentence report sets are
  recorded in `build/sentence-tsf-integration.json`.
- This covers initial loading and journal changes while no composition is active.
  Reloading during an active sentence currently disables Engine sentence mode
  while new resources load and can cancel preedit. Preserving live raw/selection,
  broader stale-task races, helper failures/performance and installed acceptance
  remain required. Automatic and empty-code commit behavior also remains pending.
  Development builds only; the installed version is unchanged.

## Shared sentence cache publication and checked loading

- Added `--ensure-sentence <base.tcd> <user.tcu> <cache-root>
  <expected-revision>`. Cache entries are partitioned by the effective revision.
  A stable per-revision lock serializes rebuilds; concurrent callers validate and
  reuse the selected immutable file instead of each rebuilding full metadata.
- New cache files have unique generation names. After validation the helper
  atomically updates the revision directory's descriptor. Missing/damaged entries
  rebuild into another generation; old paths are retained for existing mappings.
- SentenceResources accepts an optional sentence file only together with its
  expected revision and rejects mismatched or missing revision metadata before
  adopting it. Supplement/model loading continues to use the base generation.
- Four actual ARM64 helpers requesting one version produce one rebuild and three
  reuses. Repeated requests retain the descriptor; a damaged cache creates a new
  generation, and stale requests cannot change the selected cache.
- ARM64/x64/x86 readers all load the modified candidates from the shared cache
  and reject both stale and absent expected revisions. The ordinary resource
  import/decode/cache regression also passes with the updated loader.
  Evidence: `build/sentence-publish-validation.json` and
  `build/sentence-resources-validation.json`.
- TSF helper launch, current-snapshot revalidation and engine resource switching
  still need integration. Production DLLs and installed files remain unchanged.

## Sentence snapshot revision binding

- Added SHA-256 sentence revisions over mapped base bytes, mapped source inventory,
  effective user edits and new-code insertion order. Hashing streams mapped data
  and visits only the private overlay; it does not expand the full base dictionary.
  This is a helper/worker operation, not a per-key operation. Base-hash caching
  and production cost measurement are still pending.
- `--sentence-revision <base.tcd> <user.tcu>` returns the snapshot revision.
  `--rebuild-sentence` accepts an optional expected revision, rejects a mismatch
  before publication, and embeds the actual revision in `_sentence_revision`.
  Results also report the revision so the consumer can validate provenance.
- All three architecture probes agree on the revision for identical snapshots;
  changes to effective edits/new-key order change it, redundant history does not.
  Actual helper tests compare the digest with independent Python SHA-256 and
  pass stale-request rejection, embedded revision checks and publication races.
- This binds a result to the snapshot captured at helper start. It does not
  assert that the journal stayed unchanged during rebuilding. TSF must compare
  the result against its current snapshot before adopting it; shared cache and
  runtime adoption remain pending, and the edited-code gate is still present.

## Explicit journal-aware sentence publication

- Added `lexicon_import --rebuild-sentence <base.tcd> <user.tcu>
  <new-sentence.tcd>`. It opens the base's source inventory, takes one consistent
  UserStore snapshot under the existing journal locks, rebuilds sentence metadata
  in the short-lived helper, and atomically publishes a new immutable file.
- The command requires absolute paths and never replaces an existing destination.
  It does not select a generation or change configuration. Binding the resulting
  file to the exact base/user revision is still required before TSF can consume it.
- Actual Windows ARM64 helper tests pass journal edits, alias collapse, source
  order and primary-code updates, retaining old generations, existing-output/base
  refusal, competing helper processes (one winner), and corrupt-journal rejection.
  Base/supplement files and the readable journal remain unchanged. Evidence:
  `build/sentence-publish-validation.json`.
- Shared cache identity, stale-snapshot rejection, helper invocation and TSF
  resource switching remain pending. The edited-code gate is still present;
  no installed files or live application input were changed.

## Sentence metadata rebuild from user snapshots

- Added `SentenceRebuild`: combines mapped original code order with the immutable
  Lexicon snapshot, appends user-created codes in insertion order, unpacks commit
  aliases and regenerates ranks/primary/optimal metadata through the existing
  original-validated builder. This allocates transient full metadata and is
  explicitly an importer/helper operation, not a per-TSF-host runtime operation.
- Rebuild checks base-code inventory coverage and rejects duplicate identities
  or missing base codes. The caller must still bind inventory and snapshot to
  the same immutable base generation; this is not a content-hash identity check.
- Found and corrected an inventory representation issue: storing uppercase
  identity loses source spelling (for example final sigma). Format 3 retains
  normalized original spelling while deduplicating ordinal identities; folder
  cache fingerprint v4 invalidates older inventories.
- ARM64/x64/x86 rebuild probes pass deletion, formerly empty base keys, Top,
  new-key ties, delete/re-add, commit aliases, Unicode spelling, immutable old
  generations and incomplete-inventory rejection. Evidence is in
  `build/sentence-rebuild-validation.json`.
- Updated-format lexicon oracle comparisons still pass 252 fixtures / 7,420
  queries per architecture. Rebuilt ARM64 importer and three resource probes
  pass actual import/model decode and cache invalidation regression.
- Command-line publication, shared generation identity/coordination and TSF
  refresh are still pending. The edited-code gate remains and nothing is installed.

## Mapped original-code inventory

- Sentence sidecar format 2 stores normalized source codes in Comment, keyed by
  fixed-width ordinal indices. The inventory retains empty exact keys and first
  occurrence order separately from the filtered candidate table. Runtime access
  returns mapped string views and does not construct a per-host full order index.
- The reader validates contiguous ordinal keys and nonempty single-value entries.
  Folder cache fingerprint is now v3 so generations with older metadata rebuild.
- ARM64/x64/x86 serialized lexicon probes each pass 252 fixtures / 7,420 queries
  against the original. Every fixture also checks mapped source inventory order,
  empty keys, normalized duplicate keys and out-of-range access.
- Actual ARM64 importer output also loads/decodes on all three architectures
  with the original model. Resource tests pass cache reuse, corrupt/missing
  companion rebuilding, same-timestamp corpus edits and failed-import selection
  preservation (`build/sentence-resources-validation.json`).
- Shared generation rebuilding from this inventory plus UserStore snapshots is
  still pending. Production DLLs and installed applications have not been updated
  for the format change; use matching rebuilt tools/probes for development tests.

## User-code insertion order for sentence resource rebuilding

- Lexicon snapshots now retain only user-introduced exact codes in insertion
  order; base dictionary storage remains mapped. Delete/re-add keeps the same
  position, Top can introduce a new key, and no-op Delete does not add a key.
  Snapshot equivalence includes this order because sentence code ties depend on it.
- UserStore checkpoint generation emits newly introduced codes in that order.
  Legacy journal records remain unchanged in format; inherited duplicate commit
  identities continue to preserve their original history independently.
- Linux and actual Windows ARM64 replay/checkpoint probes pass 4,000 new codes
  and histories with 100,000 adjustments. Tests assert insertion order, retained
  snapshots, empty-key inventory, aliases, checkpoint equivalence and subsequent
  edits. Windows additionally verifies explicit compaction and backup recovery.
  Duplicate-identity fallback with independent compaction also passes on Linux.
  Four-process journal regression passes with 48 retained entries, recovery at
  71 interrupted-tail positions, coordination locking and corruption rejection.
- This is a prerequisite, not complete user-word synchronization. Original base
  code order still needs persisted metadata, and a shared sentence generation
  must be rebuilt/published from the base plus user edits. The TSF gate remains.
  Current changes are probe-verified; production DLLs have not been rebuilt
  since these Lexicon/UserStore changes and the installed version is unchanged.

## Sentence settings integration verification

- Added original-compatible sentence settings parsing, including explicit empty
  values, invalid/overflow integers, boolean aliases and Unicode whitespace.
  All 951 cases per ARM64/x64/x86 agree with extracted original getters.
- TSF resource selection now uses these typed settings. Decoding only requests
  prefix evidence when automatic early commit is enabled. The actual automatic
  commit policy and minimum-retained-code behavior still require implementation.
- Rebuilt ARM64, ARM64X and x86 DLLs after the settings changes. Hidden sentence
  TSF tests pass all four host/build combinations; ordinary activation regression
  passes all three supported staged variants with registration unchanged.
  Current DLL and source hashes are verified in `build/sentence-tsf-integration.json`.
  These are development builds, not installed or physical application acceptance.
- User-journal inspection identifies an additional ordering requirement:
  `GetSentenceLexiconSnapshot` enumerates original dictionary insertion order,
  while `UserStore::makeCheckpoint` enumerates a sorted edit map. New-code order
  must survive journal replay and compaction before sentence metadata can be
  regenerated faithfully. The existing no-edited-codes gate remains pending.

## TSF sentence worker and real edit-session integration

- Linked sentence model/lexicon/decoder/resources into the development DLL.
  `SentenceService.cpp` loads resources on a background worker for a schema
  containing 整句, then enables the Engine sentence mode. The optional
  `整句语言模型` path supports isolated tests; bundled model packaging is pending.
- `SentenceWorker` serializes tasks, replaces queued tasks per session, rejects
  obsolete generations and supports nonblocking close. It captures native data
  only, retains the DLL with a loader reference and uses FreeLibraryAndExitThread
  after destroying thread-local C++ objects. No Core process or IPC is involved.
- Timer delivery returns results to the TSF apartment and applies candidates
  through edit sessions after checking context/focus/resource/generation state.
  Composition completion cancels queued publications and releases its decoder;
  in-flight work owns its remaining lifetime without blocking teardown.
- Corrected the earlier queued-key-replay plan after reading original
  `EnsureSentenceDecodeCurrent`: ordinary code input is asynchronous, but Space,
  Tab, selection and punctuation synchronously complete current decoding. The
  adapter follows that behavior; preview does not decode or write text. The
  shared decoder lock prevents concurrent cache mutation. Commit-key latency
  remains to be measured; it must not be claimed fully nonblocking.
- Candidate UI now exposes/highlights the Engine's selected sentence row.
- Worker tests pass ARM64/x64/x86 coalescing, cancellation and nonblocking close.
  Real hidden TSF tests pass standalone ARM64, ARM64X ARM64/x64 and x86: immediate
  Space, background candidates, Tab selection, numeric selectors, punctuation,
  cancellation and context changes. Reports: `build/sentence-tsf*-validation.json`.
  No physical input or installation was performed.
- Ordinary staged TSF activation/mode/edit-lock/deactivation regression passed
  with unchanged registration. Final post-cache-release builds and all four
  sentence TSF runs pass; ordinary activation regression also passes against
  those exact DLL hashes. Evidence: `build/sentence-tsf-integration.json`.
- Deliberate temporary gate: a runtime user journal with edited codes disables
  sentence mode until journal-aware shared resource publication exists. Removing
  this limitation is required for goal completion, not optional fallback behavior.
  Full settings parity (including explicit empty/invalid values), automatic and
  empty-code commit policy, short-symbol transitions, original engine/golden
  traces, installed packaging and memory/latency/application acceptance remain.

## Sentence resource import and mapped loading

- `lexicon_import` now prepares sentence metadata from source encounter order,
  unpacks display/commit aliases and deduplicates commit text. It publishes
  `<ordinary>.sentence.tcd` and `<ordinary>.supplement.tcd` before the ordinary
  generation, validates both and switches the descriptor last.
- Folder cache format advanced to v2. Cache reuse validates both companions;
  missing/corrupt files rebuild into a new generation. Source supplement changes
  participate in the existing content fingerprint even with unchanged timestamps.
- Added original-compatible supplementary text parsing: escaped inline comments,
  default/positive Int64 weights, clamp/reward rules, invalid-line skipping and
  last-value replacement retaining first encounter order. Original method bodies
  are compiled by the UTF-8-only oracle adapter; encoding detection remains
  covered by the existing lexicon decoder tests.
- `SentenceResources::Open` loads the immutable generation and original model;
  separate compositions create private decoder caches over shared mappings.
  Missing/malformed resources throw so the future adapter can disable/fallback.
- ARM64/x64/x86 resource probes pass: 278 parsed entries match original across
  516 input lines; actual importer output loads and decodes with the 238,789,568
  byte model, with mapping reuse and commit-alias collapse verified. Cache reuse,
  corrupt/missing companion rebuilds, same-timestamp changes, missing model and
  failed-import selection preservation pass. `build/sentence-resources-validation.json`.
- Added the session source to the importer project as required by its Engine
  dependency. The development importer builds; installed tools are unchanged.
- Remaining: user-journal changes must produce matching shared sentence metadata
  without reconstituting the full lexicon inside each host; model/config lookup,
  TSF resource/decode workers, key replay and automatic commit policy are pending.
  Resource helpers are not yet linked into the TSF service or enabled by settings.

## Initial sentence Engine/session integration

- Added value-type `SentenceSession`, copied safely by Engine key previews.
  Decode tickets bind session identity, generation, resources, raw and required
  prefix; obsolete/duplicate publications cannot change the current selection.
- Engine now has an opt-in Sentence mode and adapter methods to request/apply
  results. Added continuous raw/selector input, backspace/cancel/literal Enter,
  Tab/up/down selection, Space and punctuation commits, selected-candidate
  snapshots, focus/schema/user-lexicon invalidation. The mode is disabled until
  an adapter explicitly enables it with available resources.
- Keys that need a not-yet-current result return `awaitSentenceDecode`; no Beam
  work occurs in Engine dispatch. TSF still needs a worker, queued-key replay,
  selection replay and edit-session publication before enabling this mode.
- Session tests cover prefix projection, no duplicate prefix commits, live-tail
  deletion, pending display, source revision changes, cross-context tickets,
  engine-preview isolation and the 128-key live-tail limit. Engine tests exercise
  basic key dispatch using injected decode completions, not original full-engine
  differential traces. Report: `build/sentence-session-validation.json`.
- Ordinary-input differential now runs the built Windows ARM64 Engine probe via
  the WSL Windows runner, with fresh original reference output. Final validation
  and build evidence are recorded separately; no installer has been run.
  Final run: 35,554 events, zero differences; session probes each pass 160 checks,
  ARM64 Engine passes 26 checks. ARM64X and x86 development DLL builds pass
  (`build/sentence-session-build.json`).
- Still required: original engine/golden trace adapter, automatic/empty-code
  commit policy, exact short-symbol/reverse-lookup transitions, resource/config
  wiring and import/publication, TSF async scheduling and candidate selection,
  performance/memory checks, packaging and installed application acceptance.

## Sentence decoder and incremental lattice

- Added `native/SentenceDecoder.{h,cpp}`: full/append/delete decoding, rank-aware
  duplicate representatives with merged log mass, n-gram/isolation/supplement
  scores, explicit selectors, stable candidate ordering and path boundaries.
- Added original early-commit prefix evidence, including incomplete-tail pool,
  boundary shares, truncation flags and required-prefix filtering. This supplies
  evidence only; engine early-commit policy still needs integration.
- Added unpruned complete-candidate queries (required prefix, excluded surface,
  grouping eligibility), normalized proper-prefix queries and cache reset.
- Generated all 20,000 original character ranks as pointer-free immutable scalar
  records. No per-host rank hash table; generator records original source hash.
- Final ARM64/x64/x86 probes pass 191 fixtures / 4,369 queries in each of four
  combinations (neutral/real n-gram × full/incremental). Candidate text/order,
  scores, segmentation, boundaries, evidence and expansion counts match frozen
  C#. Existence/prefix checks also agree. `build/sentence-decoder-validation.json`.
- Not linked into TSF or installed. Next: original committed golden/engine
  traces, engine state transitions and resource import/publication, followed by
  asynchronous TSF integration. Bounded scoring caches, cancellation, production
  latency and multi-process memory measurements are still pending.

## Shared supplementary corpus graph

- Added immutable graph serialization and `MappedSentenceSupplement`, which queries nodes/edges directly from the validated mapped container instead of rebuilding per-host maps.
- Re-ran 240 fixtures / 18,480 transitions per ARM64/x64/x86 through serialized-and-reopened graphs against original C#; all pass. Five malformed graph variants are rejected, including cyclic failure links and out-of-range/backward-depth edges.
- Evidence: `build/sentence-supplement-validation.json`. Decoder integration, source parsing, installed-host resource lifecycle and memory/latency acceptance remain pending.

## Sentence supplementary corpus matcher

- Added native weight/reward calculation and immutable traversal of a grapheme-based failure-link graph, preserving original maximum-reward semantics.
- ARM64/x64/x86 each pass 240 fixtures / 18,480 transitions against the original C# matcher (`build/sentence-supplement-validation.json`).
- Shared graph serialization, source parsing and decoder integration remain required. This component has not been deployed or linked into TSF.

## Sentence lexicon preparation and shared view

- Added source-order primary/optimal-code preparation and a separate version-marked mapped sentence lexicon. Candidate ranks and common/whitelist filtering match the current reference.
- Actual ARM64/x64/x86 serialized-reader probes each pass 252 fixtures / 7,420 queries against frozen C# SentenceLexiconIndex.Build, including empty input, filtered prefixes, ordinal aliases and graphemes.
- Evidence: `build/sentence-lexicon-validation.json`. Runtime importer/overlay integration, supplementary corpus and the decoder remain pending. Installed DLL unchanged.

## Sentence input port (Qwen excluded), 2026-09-09

- New active objective: port complete non-neural sentence behavior into TSF. Scope and remaining gates: `docs/SENTENCE_PORT.md`.
- Frozen the current reference sentence/engine sources and golden fixtures separately from the older ordinary-input oracle.
- Implemented native read-only n-gram model mapping/scoring. Actual 238,789,568-byte model compared with original C# across 13,006 queries on each of Linux/ARM64/x64/x86; all agree within 1.78e-15. Eight malformed model cases rejected on every implementation.
- Evidence: `build/sentence-ngram-validation.json`. Sentence decoder, shared lexicon metadata, TSF integration and packaging remain pending; existing installed word-input version is unchanged.

## Folder-based schema management

- Replaced the ordinary import/maintenance screen with root-directory selection, schema list, use and reload. Legacy tools remain in “更多维护”.
- Automatic content-addressed cache validation and immutable generation preparation; no per-key process/IPC changes.
- A source folder named 虎码字词 can override the bundled binary while preserving its existing user journal.
- Folder UI/cache and legacy manager/selector checks pass. TSF override mapping checks pass in all three staged host variants. ARM64X generation `5aa4bd69945cfb9c` and matching x86 companion are installed; registered TSF checks and package hashes pass.
- See `docs/FOLDER_SCHEMAS.md` for usage and precise limits.

## Ctrl+Space release handling

- User reported down toggles to English and Space up returns to Chinese.
- A new TSF regression reproduces leaked Space repeat/up in the previous DLL.
- TSF now retains ownership of Space repeat/up after a successful Ctrl+Space toggle. Preview does not mutate the latch; failed edits do not arm it; focus loss clears it. Engine parity behavior is unchanged.
- Staged ARM64 and both ARM64X host variants pass both toggle directions, auto-repeat and both modifier release orders (`build/ctrl-space-regression.json`). Real application confirmation is still required.
- Installed ARM64X generation `ccafa500dd2fe896` and matching x86 supplement; registered TSF regression passes. Evidence: `build/ctrl-space-installed-validation.json`.

## DirectWrite foreground validation

- Installed ARM64/x64/x86 Rich Edit probes each pass 27 injected taps.
- ARM64/x64 native candidate and UI-less integration fixtures pass; mouse selection uses a direct window message. Candidate screen captures visually inspected.
- Evidence: `build/directwrite-frontend-validation.json`. Actual app/cross-monitor/scaling and user quality acceptance remain outstanding. Keyboard/mouse released to user.

## DirectWrite / Direct2D deployment

- Installed ARM64X generation `0e1d68726621238b` and its matching x86 supplement.
- Installed hashes match the tested build; ARM64/x64 registered TSF checks and x86 COM activation pass.
- Foreground typing, mouse selection, real monitor transitions and user text-quality acceptance remain pending. No physical keys were injected during deployment.

# Ordinary TigerClaw native port

The requested goal is still active. The native ARM64 TSF service is now installed
independently of daily TigerClaw, using the former SampleIME profile with display
name 虎娘. `build/native-install.json` records the installed immutable generation.
The complete ordinary-input acceptance scope is not yet satisfied.

## Implemented and verified on 2026-09-08

- Resolved the DirectWrite migration's hidden TSF test blocker by isolating
  manual TIP activation from the fallback TIP. The fixture now selects an
  already-loaded plain English keyboard FORPROCESS after system deactivation
  and verifies its profile type/HKL before manual activation. Production mode
  code and lock/text assertions are unchanged. Installed baseline ARM64 passes
  with isolation; final standalone ARM64 and ARM64X ARM64/x64 staged checks all
  pass (`build/directwrite-tsf-background.json`). No deployment changed.
  Foreground/user visual and actual mixed-DPI monitor tests remain pending.

- Started new goal: DirectWrite/Direct2D candidate rendering with WPF-like
  quality and per-monitor DPI behavior. Native candidate now uses a private
  DirectWrite font collection, fractional DIP layouts and a reusable Direct2D
  WIC target with premultiplied transparency; removed GDI text-mask rendering.
  Candidate HWND uses scoped PMv2 awareness, caret conversion, DPI-triggered
  reflow and regenerated mouse rectangles. Host process DPI is unchanged.
  Final ARM64/ARM64X/x86 builds pass (`build/directwrite-build.json`).
- ARM64/x64/x86 offscreen probes each pass 90 cases and three host-awareness
  checks (`build/directwrite-render-validation.json`). Generated and inspected
  Direct2D/WPF visual samples. No physical monitor transition or user visual
  acceptance yet. Hidden TSF mode-rejection test fails for both installed
  baseline and migration DLL; bounded wait did not solve it and was removed.
  The migration is not installed or complete. Full current scope/evidence and
  remaining work are in `docs/DIRECTWRITE_MIGRATION.md`.

- User confirmed the taskbar 中/英 button test passed after installation of
  `8005a14ee64cefc0` and its x86 supplement, and requested a commit. This closes
  the reported missing mode-button issue. Evidence is user acceptance plus
  the installed ARM64/x64 mode callback and x86 COM activation checks.
  Broader ordinary-input acceptance scope in PLAN.md remains unchanged.

- User authorized retry; both primary and x86 installers exited successfully.
  Taskbar GUID fix is now deployed: primary `8005a14ee64cefc0`, ARM64X SHA256
  `4085138f657dfe2eaa02a2ead99c9f106e390ab9044c4b8dc9e19e53af9331ce`;
  x86 supplement `x86-8005a14ee64cefc0-0f97ae0a55f036fd`, SHA256
  `0f97ae0a55f036fd95725d857034dd7ed3c1b4e425e6d9182f9787e32e550065`.
  Fresh ARM64/x64 system-registered TSF activation passes mode/button tests;
  real 32-bit PowerShell COM activation also passes. Independently verified
  both installed DLL hashes. Evidence: `build/taskbar-mode-installed.json`,
  current installation records and `build/registered-tsf-validation.json`.
  User was asked to reopen an input app and check the actual taskbar button;
  visual/click acceptance is not yet claimed.

- Moved x86 migration ownership/hash validation before CheckOnly and elevation;
  recheck the 32-bit registration immediately before replacing it. Read-only
  preflight now validates migration from installed `781a7f63...` to staged
  taskbar-fix `0f97ae0a55f036fd95725d857034dd7ed3c1b4e425e6d9182f9787e32e550065`.
  Real x86 load/class creation passes. This preflight used the currently
  installed primary generation; rerun against the upgraded primary before
  deploying x86. No installation or registration changed; UAC retry question
  remains unanswered.

- Synchronized the taskbar GUID fix into standalone ARM64 output as well.
  Current standalone SHA256 is
  `17d7f581f76a84cd2fbb5ef88139bdf0251de07db091ef11ebafc1a675183914`.
  All three staged activation variants now pass: standalone ARM64,
  ARM64X/ARM64 and ARM64X/x64 (`build/staged-tsf-activation.json`).
  Rechecked system registration: primary remains `09d5f4f5445d1891`, and
  x86 remains its earlier supplement. No consent process remained after the
  canceled installation. Requested explicit retry availability; no new UAC
  or installation was launched. Visible taskbar acceptance remains open.

- Fixed missing taskbar mode-item registration: native language bar now uses
  Windows GUID_LBI_INPUTMODE instead of its private GUID and removes the
  unsupported SHOWNINTRAY style. ARM64X and x86 builds pass. Updated hidden
  host to assert standard identity and handle its own fallback-item handoff
  during manual service activation. Both ARM64/x64 mode/button/menu tests pass;
  `build/taskbar-mode-validation.json` records exact staged hashes. Taskbar
  visual verification remains pending.
- ARM64X installer ended with exit 1: Windows reported elevation was canceled
  by the user. The last registry read still pointed to `09d5f4f5445d1891`;
  the mode-button fix is not deployed. After an authorized installer retry
  succeeds, run `install_x86.ps1` to upgrade x86 too. Its new
  migration path verifies owned prior registration/hash and saves a snapshot.

- User reported ARM64 input working but Weixin/Word/x86 Pain failing. Process
  inspection found Weixin retaining old `e3b95d4b9183e325`; user confirmed
  restart fixes Weixin. Word loaded the current primary DLL but initially
  passed English keys; user later confirmed all three apps input normally.
  The exact Word recovery cause was not isolated; do not call it a proven fix.
- Registry32 had no TIP COM server. Built real x86 DLL with v145 (isolated
  Win32 output, static CRT and corrected release linker settings), validated
  with an x86 loader, then installed using `install_x86.ps1`. DLL hash:
  `781a7f63de077773e4b2e57ab280714b9335ddc09aeef111effe67b766a6fb1d`.
  Native ARM64X registration is preserved. Companions/dictionary are hardlinked
  from primary package; `build/native-install-x86.json` records the supplement.
  A 32-bit PowerShell COM activation also passes through system registration.
  Added x86 Rich Edit build/runner support (built, not yet automated-input run).
- Added `uninstall_x86.ps1`, which removes only matching x86 COM registration,
  retaining shared profile/data/files. Main uninstall refuses while x86 remains;
  documented removal order and restart requirement. No uninstall was performed.
  User app success is basic manual-input evidence, not complete visual/DPI/
  modifier/candidate/memory acceptance for all applications.

- User reported the Hook edition was running and exited it. Read-only process
  enumeration then found no Tiger-named or leftover Rich Edit probe processes.
  Repeated the identical diagnostic ARM64/x64 probes against the unchanged
  installed DLL: both pass all 27 taps across two real Rich Edit controls,
  including preedit/commit, Escape, context return, numeric selection,
  Backspace, newline and selected-text replacement. DLL/module/registration
  checks pass. `build/rich-edit-after-hook-exit.json` retains both reports;
  prior first-key failures remain in `rich-edit-tsf-*-hook-active.json`.
  This strongly suggests Hook interference for these first-key failures;
  no interception trace proves the exact hook path. It does not explain every
  older intermittent failure. Third-party apps, hardware keys and full visual
  acceptance remain open. No product DLL was changed during this comparison.

- User authorized foreground testing. After the idle gate, actual Rich Edit
  runs failed in both ARM64 and x64 hosts against installed `09d5f4f5445d1891`:
  expected text was absent (empty UTF-16 diagnostic). The old probe did not
  record the failing step, so the exact step is not yet proven. Both runs
  verified DLL hash and unchanged registration. Evidence preserved as
  `build/rich-edit-tsf-ARM64-before-diagnostics.json` and x64 counterpart.
  No real-app pass is claimed. User was released from the one-minute idle
  interval; follow-up foreground availability was requested.
- Added bounded, buffered key-test/dispatch observations and failing tap plus
  expected-text diagnostics to the Rich Edit probe. ARM64/x64 builds pass;
  `build/rich-edit-diagnostic-build.json` records hashes. No product DLL change
  and no diagnostic-version runtime result yet. Root cause remains open.

- Reviewed reference drift using normalized source diffs retained under
  `build/reference-diff-*.patch`. CoreRuntimeState removes 智能 from sentence
  auto-detection; InputMethodEngine removes smart-sentence behavior and changes
  sentence reranking. Shared key/reset paths only lose smart-state assignments.
  Protocol/launcher/reranker changes concern the sentence service lifecycle.
  No ordinary candidate selection or commit branch change was found in those
  diffs. `build/reference-source-current.json` records per-file assessment.
  Keep the frozen oracle baseline; no product source update is justified by
  this review. Runtime equivalence to the evolving reference is not claimed.

- Rebuilt the frozen ReferenceOracle (zero warnings/errors) and regenerated all
  six settings oracle traces in disposable staging copies. Current native ARM64
  engine matches all 41,100 events. `tests/settings_key_parity.py` now verifies
  upstream/overlay manifests and binds replay metadata to complete staging,
  oracle source/runtime and dictionary hashes. Report remains a component test.
- Read-only comparison with the currently supplied reference checkout's `next/`
  finds 24 of 34 upstream snapshot files identical and 10 changed/missing;
  `build/reference-source-current.json` records exact hashes. InputMethodEngine
  and CoreRuntimeState are among changed files; smart/sentence files also
  changed or disappeared. This does not invalidate frozen-snapshot parity,
  but parity with the present checkout is unproven. Review ordinary-input
  semantic differences before deciding whether a new isolated oracle is needed.
  Neither the frozen snapshot nor the daily reference runtime was modified.

- Rebuilt the ARM64 engine probe from current sources and replayed all six
  settings profiles against retained Core outputs: 41,100 events, zero
  mismatches (`build/settings-key-parity-arm64.json`). The runner now invokes
  Windows tools through PowerShell with `/init` fallback when WSLInterop is
  absent, and records source snapshots and the generated trace hash in addition
  to probe/dictionary/oracle-output hashes. Replay verified existing metadata
  for trace/settings/selection inputs; this run did not regenerate Core outputs
  or establish a fresh reference-table/source provenance chain. It is a native
  engine component check, not a TSF DLL or real-application input check.

- Independently rehashed all 11 installed artifacts of `09d5f4f5445d1891`:
  all match the installation record. Its rollback snapshot's old DLL also
  matches the captured hash. Actual rollback and uninstall scripts both pass
  `-CheckOnly` for the new deployment. Evidence:
  `build/installed-package-integrity-09d5f4f5445d1891.json`. No rollback or
  uninstall was performed. Corrected the application acceptance sheet to name
  the new installed generation and identify its Rich Edit evidence as historical.

- Post-install system-registered activation now passes for current package
  `09d5f4f5445d1891` in ARM64 and x64 hosts. The fresh report
  `build/registered-tsf-validation.json` verifies actual module/dictionary,
  records both host hashes, and covers five explicit key callbacks plus
  deferred mode handling. Registration remains unchanged. Hidden test windows
  do not exercise physical keys or real application controls. Updated install
  documentation to distinguish this deployment from the previous package's
  historical rollback/uninstall cycle.

- Installed ARM64X package `09d5f4f5445d1891` using the authorized installer.
  The installer exited successfully; its transcript confirms COM activation and
  profile installation. An independent registry read confirms the versioned
  Program Files DLL and SHA256
  `db270be795ece42b6502cf3c8e19551c5d4e2a17061a845d6529c31cbe2f1bcc`.
  `build/native-install.json` records matching installed artifacts and successful
  ARM64/x64 loader/class-instance checks. Previous generation is retained in
  `build/previous-install-09d5f4f5445d1891.json`. Maintenance helper and manager
  are now deployed. Older acceptance reports still refer to their recorded
  generations; real-application and visible-input acceptance remains pending.


- Synchronized the second cross-context UI fix into ordinary ARM64 staging
  (DLL `10c54e4d2654cb0752911ad79ec93f9359ea9786531574a2290dfb232d0b611f`).
  Both targeted selection scenarios and baseline background activation pass
  all three variants: ordinary ARM64, ARM64X/ARM64 and ARM64X/x64. Current
  reports are `build/staged-tsf-selection-race.json` and
  `build/staged-tsf-activation.json`. ARM64X preflight validates 11 artifacts
  and both loaders for package `09d5f4f5445d1891`; complete captured preflight:
  `build/package-readiness-09d5f4f5445d1891.json`. No registration/install changed.
  Actual app input, visible candidate/settings acceptance and full completion
  audit remain required; these background checks do not complete the goal.

- Reproduced a second asynchronous selection issue: with an old context's
  termination lock deferred, focusing another context and starting its
  candidates, then granting the old lock, hides the new candidates. Failure
  against DLL `89529...` is retained in
  `build/staged-tsf-cross-context-before-fix.json`. OnEndEdit now hides UI after
  termination only if that UI belongs to the ending context. New ARM64X DLL
  `db270be795ece42b6502cf3c8e19551c5d4e2a17061a845d6529c31cbe2f1bcc`
  passes ARM64/x64 targeted fixtures: returned-selection continuation,
  still-outside termination, preservation of another context's candidate UI,
  and its subsequent commit. Report `build/staged-tsf-selection-race.json`
  includes fixture/DLL/host hashes. These remain synthetic TSF owner/focus
  callbacks, not physical app switching. Ordinary ARM64 and package readiness
  `4ec8299bae309cd3` predate this second fix; installation remains unchanged.

- Rebuilt ordinary ARM64 with the same selection fix and assembled the complete
  ARM64X package, including current maintenance manager/helper. All three
  targeted selections (ordinary ARM64, ARM64X/ARM64 and ARM64X/x64) pass.
  `install_arm64.ps1 -Arm64X -CheckOnly` validates all 11 artifacts and both
  architecture loader/class-instance checks. Candidate generation is
  `4ec8299bae309cd3`; retained readiness record:
  `build/package-readiness-4ec8299bae309cd3.json`.
  Ordinary ARM64 DLL hash is now
  `516013174e6dc236bb3f568fa21a30badf5ee3312bd316b3d45e23d6cac088e1`.
  These are staged/preflight results, not installation or actual application
  acceptance. Installed generation remains `e3b95d4b9183e325`.

- Reproduced and fixed a specific stale-selection composition bug. New
  `selection_race_fixture.h` calls OnEndEdit under a real TSF read session,
  defers the resulting edit lock, moves selection back inside the composition,
  then grants it. The previous ARM64X DLL fails with “Stale outside-selection
  request ended the returned composition”; preserved in
  `build/staged-tsf-selection-race-before-fix.json`.
  Service::OnEndEdit now rechecks active context, composition and live selection
  when the queued write grant executes. Rebuilt ARM64X DLL
  `89529cd17a85ac9f88c5922bae1d9afa2ccc05b2c06ab47f5fbe1bb56c7725e4`
  passes both ARM64/x64 targeted fixtures, including the control that selection
  remaining outside still ends composition, and continuing input after return.
  Evidence: `build/staged-tsf-selection-race.json`; x64 ordinary activation also
  passes. ARM64 host rebuild additionally required correcting an old signed/
  unsigned WM_KEY message comparison in queued-test diagnostics.
  This establishes this race only, not the cause of every historical first-key
  failure. It uses synthetic owner notifications/selections and deferred test
  locks, not physical application caret movement. Installed DLL unchanged;
  ordinary ARM64 staging still predates the fix.

- Re-audited current scope and returned priority to the unresolved composition
  lifecycle issue. Service source inspection found a concrete asynchronous
  edge to reproduce: OnEndEdit observes an out-of-composition selection, queues
  termination, then checks only engine revision when the grant runs. A selection
  returning inside the composition without an engine revision change is not
  revalidated. This is a hypothesis for a targeted stale-selection test, not a
  demonstrated cause of the historical first-key failure; no production change
  was made. Updated ACCEPTANCE's stale staged DLL hashes and explicitly separated
  newer standalone maintenance tools from the older staged/installed DLLs.

- Verified maintenance for an imported schema through both actual ARM64 CLI
  and hidden manager handlers. The fixture uses a valid current generation and
  deliberately invalid unused legacy dictionary; CLI schema lookup also uses
  different ASCII case. Compaction, no-op repeat, explicit recovery and
  no-overwrite pass. Built-in and unselected-schema journals remain byte-exact,
  as do the selected schema's current descriptor, unused legacy file and main
  configuration. Reports: `build/user-store-maintenance-cli-imported.json` and
  `build/user-store-maintenance-manager-imported.json`; reproduce by adding
  `--imported` to `tests/user_store_maintenance_cli.py`, optionally `--manager`.
  Report field `unselected_schema_files_preserved: 4` counts the two unaffected
  journals plus the descriptor and unused legacy file, not four other schemas.
  Physical UI/file-picker acceptance remains pending.

- Added “恢复词库…” to the manager, using a file picker scoped initially to
  the selected schema's user directory and filtered to `.old` backups. It calls
  the existing recovery CLI on the worker thread and reports restored versus
  existing-live-preserved outcomes. Expanded the window client height for a
  separate recovery row without reducing the status area. ARM64 build and
  hidden manager compaction/recovery/no-overwrite/config-preservation tests
  pass, including the existing 96/144/192 DPI bounds/font checks. Evidence:
  `build/user-store-maintenance-manager.json`. Test mode supplies the selected
  backup only inside marked fixture roots; the actual file picker and physical
  clicks remain untested. This new manager is not installed yet.

- Added a “整理词库” button for the selected schema to the actual manager.
  It invokes the existing helper through the worker-thread/Completed-message
  path, disables actions while running, reports changed/no-op-or-busy results,
  and does not switch the configured schema. ARM64 build and hidden-window
  `--test-compact` checks pass, including existing 96/144/192 DPI bounds/font
  checks, real compaction and repeat no-op. Subsequent CLI recovery and exact
  config preservation also pass. Evidence:
  `build/user-store-maintenance-manager.json`; reproduce with
  `python3 tests/user_store_maintenance_cli.py --manager`.
  This uses the actual window command handler with a posted test command, not
  a physical click or visible screenshot. New manager/helper are not installed;
  visible UI acceptance, restore UI and automated recovery remain open.

- Exposed native maintenance through the existing `schema_select` utility:
  `--compact-user <schema>` and `--recover-user <schema> <absolute-backup>`.
  Both reuse schema-name validation/case resolution and the correct built-in
  or imported-schema journal/dictionary paths without changing configuration.
  Actual ARM64 CLI regression passes compaction, backup retention, no-op repeat,
  explicit restoration, refusal to overwrite existing live data and exact
  configuration preservation. Evidence: `build/user-store-maintenance-cli.json`;
  reusable runner `tests/user_store_maintenance_cli.py`; usage and limitations
  in `native/USER_STORE_MAINTENANCE.md`. Manager-window buttons, automatic
  compaction/recovery and installation of this newly built helper remain open.

- Added explicit native Windows `restoreCheckpoint(backup)`. It holds the
  journal sidecar, refuses an existing live path, verifies the selected backup
  is in the same directory and journal namespace, opens it without CREATE,
  validates the entire journal (including no interrupted tail), writes and
  flushes a separate copy, then moves it to the absent live path without
  REPLACE_EXISTING. If another writer recreates the live path, it preserves
  that file and the selected backup. Automatic backup selection is not enabled.
  Actual ARM64 regression passes valid-backup restoration, rejection of a
  truncated backup and an unrelated journal's artifact, refusal to overwrite
  the restored live journal, retained backup/displaced fixture, and subsequent
  edit equivalence. Evidence: `build/user-store-checkpoint-arm64.json` with
  tested native/fixture source hashes. This is explicit recovery, not yet
  injected ReplaceFile partial-failure recovery, UI integration or automation.

- Fixed a native recovery hazard: refresh/commit and checkpoint now check for
  journal-scoped publication artifacts under the coordination lock before
  opening a missing journal with OPEN_ALWAYS. If artifacts exist, they report
  recovery required instead of creating an empty live journal and silently
  presenting the base dictionary as if user edits never existed.
  ARM64 regression passes: after real native compaction, displacing the live
  fixture causes both refresh and checkpoint to refuse without creating a
  replacement; restoring the displaced fixture preserves the lexicon.
  `build/user-store-checkpoint-arm64.json` records the tested source/binary.
  This is fail-closed preservation, not automatic selection/restoration of a
  backup. Older installed clients lack this check, and API-internal failure
  recovery plus automatic compaction remain unfinished.

- Native compaction staging/backup names now include the originating journal
  filename (`<journal>.compact-<reserved-temp-name>` and `.old`), so recovery
  artifacts from several schemas in one directory can be distinguished.
  Naming uses a non-replacing move of a newly reserved empty file; existing
  artifacts are not overwritten. The ARM64 publication regression passes with
  an unrelated `.old` artifact present and verifies that artifact is preserved
  byte-for-byte. Backup checks now select only the fixture journal's prefix.
  Evidence: `build/user-store-checkpoint-arm64.json`; runner
  `tests/user_store_checkpoint.py`. This supplies artifact attribution, not
  automatic recovery or retirement; both remain unfinished.

- Retested the rejected MoveFileEx alternative with the same READ|DELETE
  sharing guard used by the successful ReplaceFile prototype. It still returns
  Win32 error 5; complete live and staging bytes remain intact. Thus the earlier
  failure was not explained by omitting read sharing. The full accompanying
  ReplaceFile race and two termination-boundary checks remain passing.
  Evidence: `build/journal-move-sharing-windows.json`; its overall `passed`
  describes successful assertions, while `move_with_read_sharing_error: 5`
  explicitly means this alternative did not publish. Continue with ReplaceFile
  failure-state recovery; do not remove its backup protection or infer that
  MoveFileEx can replace the guarded file on this system.

- Integrated explicit Windows publication into native `UserStore::compact()`.
  It holds the stable sidecar, acquires a read/write guard sharing READ|DELETE
  (deferring on legacy-handle sharing conflicts), replays/checks the latest
  journal, flushes same-directory staging, and calls ReplaceFile with a unique
  old-journal backup. It retains recovery artifacts on API failure and reports
  the staging path; automatic failure recovery is not yet implemented.
  Actual Windows ARM64 tests now compact 5,222,206 to 222,240 bytes on disk,
  verify backup lexicon equivalence, skip while a legacy handle is open, avoid
  republishing an unchanged image, and preserve subsequent edit behavior.
  Linux checkpoint regression also passes after the shared-code refactoring.
  Reports: `build/user-store-checkpoint-arm64.json` and Linux counterpart.
  This entry point is explicit maintenance only: no service, settings UI or
  per-key path calls it yet. Recovery/backup retirement must be finished before
  automatic compaction; old backups still consume storage. Existing installed
  and staged TSF DLLs predate this new publication method.

- Rebuilt ARM64 and ARM64EC UserStore probes from the current sidecar/checkpoint
  sources and ran the actual Windows mixed-architecture concurrency suite.
  Four writers retain all 48 entries; all 71 interrupted-tail cases recover;
  checksum-corrupt data is rejected without overwrite. Evidence with both
  executable hashes and UserStore source hash: `build/user-store-mixed-arm64ec.json`.
  Runner now invokes Windows probes through PowerShell, with /init fallback
  when WSLInterop binfmt is missing. The held-sidecar reader assertion remains
  Linux-only and is explicitly false in this Windows report. The existing TSF
  word-save failure fixture was inspected but not launched: secure reactivation
  shows a native candidate window, so its foreground-related verification is
  deferred while the desktop-availability question is pending.

- Verified actual TSF activation of the newly staged DLLs through private COM
  manifests: ordinary ARM64 in its ARM64 host and ARM64X in both ARM64/x64
  hosts pass. Each checks the loaded module, mapped dictionary, language-bar
  and mode compartments, plus five explicit key callbacks and deferred mode
  edit/focus/deactivation handling. Reusable runner:
  `python3 tests/staged_tsf_activation.py`; evidence with DLL/host hashes:
  `build/staged-tsf-activation.json`. Registry path is unchanged before/after.
  This is background synthetic TSF activation, not registered deployment,
  physical typing, live schema/journal refresh or application acceptance.

- Built the actual TSF DLL after the UserStore sidecar/checkpoint changes:
  ordinary ARM64 project build succeeds, and `build_arm64x.ps1` completes the
  dual-architecture DLL plus all four companion tools and loader verifiers.
  Explicit loading/class creation passes for ARM64 DLL in an ARM64 probe and
  ARM64X DLL in both ARM64 and x64 PE probes. Evidence with DLL/probe hashes
  and checked PE machines: `build/checkpoint-dll-load-validation.json`.
  New staged hashes: ARM64 `78572c9d964b548b88ac31dcc6e3604e2c69bb8fd1704b88b9d91bfb3dee94fe`,
  ARM64X `672fe1714df4ff42eda0522e9e681657374fe1e7dd5b4bbda8a773163a71e783`.
  These builds are not installed and have no new typing/activation acceptance.
  Before overwriting staging, preserved the installed generation's matching
  DLL/PDB in `build/diagnostic-symbols/8a9fd592e143d7df636c2acef0f510dde33bb8023329fb948e253a31022b734d/`
  with `hashes.json`. Earlier installed-stack symbolization must now use this
  archive instead of the rebuilt staging paths. Registration remains unchanged.

- Added actual owned-process termination to the Windows publication prototype
  at two explicit boundaries: after staging Flush(true) but before ReplaceFile,
  and after ReplaceFile returns while the old-file guard is still held. The
  first retains complete old live bytes and complete unpublished staging; the
  second retains complete new live bytes and complete old backup. After child
  termination, a new legacy-style read/write open and flushed append succeeds.
  Both cases and the 100-writer/50-publication race pass in
  `build/journal-replace-crash-windows.json`. Initial diagnostic failure was an
  empty PowerShell CLIXML header, not a file-content failure; the checker now
  accepts only that bare header or empty stderr and rejects actual payloads.
  These controlled boundaries do not test interruption inside ReplaceFile,
  physical power loss, API partial failures or integrated UserStore recovery.

- Extended the Windows publication prototype with an independent PowerShell
  writer using legacy read/write sharing and retrying sharing violations.
  During its 100 flushed appends, the parent performs 50 guarded ReplaceFile
  publications with unique backup files. All 150 distinct writer/publication
  markers survive exactly once; all 50 publications observed a writer history
  between W0 and W99, proving overlap with the write sequence. The child exits
  successfully. `build/journal-replace-race-windows.json` records source hash
  and result; fixture remains `tests/journal_publish_probe.ps1`.
  This validates actual cross-process sharing/replacement on this Windows
  filesystem, using marker records rather than UserStore records. It does not
  validate process termination, physical power loss, ReplaceFile error recovery
  or production integration. No installed file or live journal changed.

- Prototyped Windows publication against actual file-sharing behavior. A guard
  opened for read/write with share-delete only correctly excluded legacy
  read/write handles, but MoveFileEx replacement failed with Win32 error 5;
  `build/journal-publish-windows.json` preserves that failed approach.
  A read/write guard sharing READ|DELETE still excludes legacy UserStore
  read/write handles, while ReplaceFile succeeds with the guard held. The old
  guard retains old file identity; a new legacy-style handle sees the complete
  replacement and successfully appends. `tests/journal_publish_probe.ps1` and
  `build/journal-replace-windows.json` record this passing isolated Windows
  prototype. This removes the earlier assumption that all old clients must
  exit for publication: currently open legacy handles can instead defer it.
  Multi-process races, ReplaceFile failure states, crash recovery and production
  integration are still unverified. No live user journal was replaced.
  API references: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
  and https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew

- Improved duplicate-identity handling: checkpointing now retains original
  records only for the affected normalized codes, and compacts the remaining
  codes normally. Reconstructed output is still replayed and compared before
  return. The duplicate-alias fixture plus 1,000 redundant edits on another
  code shrinks from 50,048 to 98 bytes while retaining both alias candidates
  and the unrelated final word; live bytes stay unchanged. Linux report:
  `build/user-store-checkpoint-duplicates.json`. ARM64 build and baseline/
  100,000-adjustment checkpoint regression pass. The specialized duplicate
  fixture was run on Linux only. Online publication remains unimplemented.

- Added an explicit duplicate-commit checkpoint regression. The real main-table
  expected export has no duplicate commit identities, so the test creates an
  isolated valid dictionary copy with two `ab` candidates that display
  differently but commit the same text. After a user addition, checkpointing
  returns the original history byte-for-byte and retains both candidates;
  the live fixture journal is unchanged. Linux test passes:
  `python3 tests/user_store_checkpoint_duplicates.py`, report
  `build/user-store-checkpoint-duplicates.json` with fixture/binary hashes.
  The production dictionary is never modified. Current fallback preserves the
  whole history if any edited code retains duplicate identities; it protects
  semantics but cannot reduce history for that case. Online publication and
  old-client migration are still unfinished.

- Added a stable `.lock` sidecar to every UserStore commit, refresh and
  checkpoint operation. New clients acquire sidecar before the existing journal
  lock; the latter remains for interoperability with currently installed older
  clients. Sidecars must not be deleted while stores can be active. ARM64
  builds and baseline/100,000-adjustment checkpoint regression pass with this
  lock ordering. Online replacement is still disabled: old clients do not
  observe the sidecar, so publication requires an explicit migration boundary
  as well as durable replacement/recovery. This groundwork alone does not
  make replacement safe in a mixed old/new process set. Duplicate-identity
  fallback testing remains outstanding.
  Linux regression also passes the new held-sidecar reader check (child's open
  sidecar handle observed before releasing the lock; journal not yet created),
  four concurrent writers retaining 48 entries, all 71 interrupted-tail cases
  and checksum-corruption refusal. Report: `build/user-store-linux.json`;
  the sidecar assertion is in `tests/user_store_concurrency.py`.

- Expanded checkpoint validation to delete and reorder real base `ab`
  candidates, preserve a display/commit alias containing an escaped newline,
  and perform identical subsequent Add/Advance/Delete edits on the original
  and checkpoint journals. Linux and actual Windows ARM64 pass both baseline
  and 100,000-adjustment cases. The initial alias assertion incorrectly expected
  LF; source inspection confirmed existing parseEntry expands escaped newline
  to CRLF, and the fixture now explicitly checks that representation.
  Reusable runner: `python3 tests/user_store_checkpoint.py` and the same command
  with `--windows`; reports include dictionary, source and binary hashes.
  Extended long-history fixture shrinks 5,222,206 to 222,240 bytes. Online
  publication and duplicate-identity fallback coverage are still pending.

- Implemented `UserStore::checkpoint()` as a non-publishing journal-image
  builder. It holds the current journal lock, replays the latest state, emits
  existing v1 Delete/Add records, retains empty exact keys, and checks decoded
  overlay and quick-symbol equivalence before returning. If final candidates
  contain duplicate commit identities that Add cannot reconstruct, it returns
  the original valid history; it never returns a larger replacement.
  Linux and actual Windows ARM64 replay probes pass: a 100,000-adjustment
  history shrinks from 5,222,108 to 221,982 bytes, retaining final word order,
  empty-key metadata, restart equivalence and earlier snapshots. Evidence:
  `build/user-store-checkpoint-linux.json`, `build/user-store-checkpoint-arm64.json`.
  This is the compaction core only. Live publication, concurrent old-handle
  coordination, failure recovery and duplicate/alias-specific coverage remain
  incomplete. The installed production DLL and live user journals are unchanged.

- Measured repeated user-frequency edits with a new optional churn mode in the
  existing replay probe. On Linux, 4,000 edited codes produce a 221,988-byte
  journal and about 7.8 ms median baseline replay. Alternating 100,000 Top
  operations on one existing code grows the same logical overlay to 5,221,988
  bytes; one full replay takes 108.7 ms. Final candidate order, restart replay,
  and retained immutable snapshots pass. Evidence with executable, source and
  dictionary hashes: `build/user-store-journal-growth-linux.json`.
  This is a batch-generated history and Linux measurement, not Windows typing
  latency or 100,000 independently durable writes. It confirms that full replay
  grows with history even when live vocabulary does not. Compaction remains
  unimplemented; the 128 MiB hard limit is not a long-term growth solution.

- Extended the real Rich Edit probe with numeric candidate selection, preedit
  Backspace correction, an idle Enter followed by input on the second line,
  and replacement of a selected committed character. Each case checks actual
  control text; Enter permits Rich Edit's CR or CRLF representation but requires
  exactly two lines. Added a held-modifier guard before OS key injection.
  Both ARM64 and x64 builds pass; current source/binary hashes and PE machines
  are recorded in `build/rich-edit-build.json`. Runtime validation of these
  additions is pending the desktop-availability answer. The older 12-tap x64
  pass does not cover the new scenarios or current probe binaries.

- Recovered the completed real-control result from `build/rich-edit-tsf-x64.json`:
  the x64 Windows Rich Edit probe passed 12 OS-injected taps across two controls,
  checking incremental preedit, Space commit, Escape cancellation and return
  focus. It verified the loaded installed DLL path. This is independent of the
  hand-written text store, but one successful run does not resolve that fixture's
  intermittent failures or establish third-party application acceptance.
  ARM64 RichEditProbe now builds successfully; its visible run is pending the
  user's desktop-availability response. The runner now independently hashes the
  installed DLL before and after execution and records the probe source hash.
  Python syntax validation passes. Those new hash checks have not yet run in a
  visible test; the recovered x64 report contains the installation-record hash
  only. No production DLL or registration changed.

- The idle gate initially declined this turn's visible run, then admitted it
  after a later 17-second idle observation. The x64 queued-message path now
  passes both UI-less and native-UI variants. Three subsequent alternating
  direct/queued pairs also pass without verbose tracing; all six results are
  preserved in `build/dispatch-comparison-rqklghh5`. This does not establish a
  causal difference between dispatch paths or resolve earlier failures.
  The installed DLL is unchanged. Next independent check should use the Windows
  Rich Edit control's own TSF text store with OS-injected input, instead of relying
  solely on the hand-written text store. Rich Edit TSF enablement is documented
  via EM_SETEDITSTYLE/SES_USECTF:
  https://learn.microsoft.com/en-us/windows/win32/controls/em-geteditstyle
  That control test has not yet been implemented or run and is not an acceptance
  claim for Word, browsers or physical hardware keys.


- Added an explicit `--queued` comparison path to registered_tsf_test.py: the
  fixture posts its key messages to its own window and invokes TSF preview/
  dispatch while draining those messages, matching the documented message-loop
  calling pattern. This remains posted test input, not physical input, and is
  reported separately. x64 build succeeds; visible queued execution was refused
  by the live idle gate. Headless registered activation and the text-store lock
  contract still pass. The queued path itself remains unvalidated.
  Reference: https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfkeystrokemgr-testkeydown
  During this turn the WSLInterop binfmt entry disappeared, making direct Windows
  execution fail with Exec format error. Existing WSL interop sockets and /init
  still work; the runner now uses /init for PowerShell when that binfmt entry is
  absent. No system configuration was modified. Explicit /init invocation of the
  x64 host did not preserve the expected argument shape; invoking it through
  Windows PowerShell succeeds, which is the runner's chosen path.
  The user has been asked for a minute of uninterrupted desktop availability;
  existing registration and the production DLL remain unchanged.


- Added fixed-capacity raw termination-stack capture during explicit interactive
  key tests, without requiring verbose tracing. Callback-time work saves raw
  addresses only; module lookup/formatting occurs in the failure handler. x64
  host rebuilt successfully. All three attempted visible runs this turn were
  rejected before host launch by the live desktop-input inactivity gate, so an
  unexpected termination stack has not yet been captured by this build.
  Verified local llvm-symbolizer against the exact installed DLL's staged twin
  and matching PDB: baseline offsets 2945632/2946204 resolve to Service::end and
  Service::apply (Service.cpp:243/257). Evidence:
  `build/composition-stack-symbolization.json`. This establishes the symbol
  resolution path, not the cause of premature termination. Production unchanged.


- Removed diagnostic-only composition enumeration and open-compartment queries
  from each traced key; composition counts now come from owner notifications.
  Full stack capture is separately opt-in (`NATIVE_TIGER_TEST_STACK_TRACE`).
  Added a fixed 128-entry key-observation ring that copies only in-memory test
  state and dumps on failure, with no COM queries, allocations or output in its
  recording function. x64 build passes. An untraced controlled repetition now
  captures the failure: after the first A is handled, starts=1/ends=1; B creates
  a second composition and returns two candidates. Evidence archived in
  `build/key-ring-repeats-3bg37g9x/runs.json` and current x64 failure report.
  This reproduces premature termination without detailed trace queries. Next
  capture the unexpected termination stack without adding per-key I/O. No
  production behavior or installed DLL was changed.


- Added application-side composition start/update/end notification logging and
  end-callback module/offset stack capture through ITfContextOwnerCompositionSink.
  The sink accepts new compositions; source reference:
  https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfcontextownercompositionsink
  Five traced x64 repetitions passed and were archived in
  `build/composition-trace-km6fvw66`. Disabling trace reproduced `Second context
  commit failed`; exposing the composition sink even without logging did not
  eliminate that failure. This points to timing sensitivity, not a proven sink
  fix. Changed diagnostics to buffer stderr in memory until process exit to
  avoid per-field pipe writes delaying keystrokes. The rebuilt buffered-trace
  x64 host passes one visible run; root cause remains unresolved. Summary:
  `build/composition-trace-investigation.json`. Production DLL remains unchanged.


- Corrected the test TextStore's reentrant asynchronous lock handling: requests
  now coalesce (write takes precedence) and are granted after the current lock;
  a rejected synchronous request preserves the queued request. The previous
  unconditional TS_E_SYNCHRONOUS response violated the documented application
  contract. Added a no-UI/no-IME lock fixture; x64 build and two-grant assertions
  pass (`build/text-store-lock-x64.json`). Reference:
  https://learn.microsoft.com/en-us/windows/win32/tsf/document-locks
  This is a fixture correction, not a demonstrated product fix.
  Subsequent x64 visible test still fails. Detailed log shows the first A leaves
  text `a` but zero compositions immediately after event 1; B then starts a new
  composition, yielding two candidates. Open mode is 1, modifiers are clear,
  logged lock requests are not reentrant, and no queued keyboard entries appear.
  Evidence: `build/registered-tsf-interactive-x64.json` and
  `build/registered-x64-ui-less.stderr.txt`. Next investigate the cause of early
  composition termination, not just candidate rendering or mode switches.


- Built both architecture hosts with mode/modifier traces. After the idle gate
  initially refused an active desktop, ARM64 completed five controlled repeats,
  each containing UI-less and native UI (36 events each). Archived all runs and
  traces in `build/focus-mode-repeats-bt6ty75r`: zero queued keyboard messages;
  every observed open compartment was 1. This does not prove the earlier
  intermittent failure fixed. The subsequent x64 native-UI run failed to restore
  a visible candidate window within two seconds; composition was `ab`, open=1,
  Shift/Ctrl/Alt=0 and no queued key entries. Evidence:
  `build/registered-tsf-interactive-x64.json` and its native-UI stderr.
  Added optional TextStore lock-request/GetTextExt tracing in source for the next
  build; investigate whether asynchronous layout edits are actually granted.
  The production DLL and installed generation remain unchanged.


- Controlled visible rerun proceeded after a live 58-second idle observation;
  no old tsf_host/test process was running. Candidate restoration exposed a test
  timing assumption: production requests ASYNCDONTCARE layout edits, but the
  fixture asserted after one queue drain. Added a bounded two-second condition
  wait and process-owned candidate lookup. ARM64 rebuild succeeds.
  The next run passed candidate restoration but reproduced focus-return failure
  without queued-key trace entries: after the first composition terminated,
  subsequent explicit A/B were not handled and text remained `交ab` instead of
  `交ab疒`. Current evidence: `build/registered-tsf-interactive-ARM64.json` and
  `build/registered-ARM64-native-ui.stderr.txt`. Added compartment-open/modifier
  tracing for the next build to distinguish mode restoration from key state.
  This last tracing edit is not yet compiled. Production DLL remains unchanged;
  the earlier external-input observation does not explain this controlled failure.


- Added `tests/desktop_state.ps1` using GetLastInputInfo, GetForegroundWindow
  and input-desktop inspection. Live probes confirm active keyboard/mouse input
  (first observation: 171 ms idle). The registered interactive runner now requires
  Default desktop, a nonzero foreground window and 15 seconds of input inactivity
  before launching any host. The actual invocation was refused at this gate,
  without opening a test window or replacing the prior interactive result.
  `build/registered-tsf-desktop-precondition.json` records the latest observation.
  This detects an unsuitable starting condition; it cannot guarantee that no
  input will arrive during a later run, so queued-key tracing remains necessary.


- Expanded the actual-app inventory without opening foreground windows. Word's
  WINWORD.EXE has PE machine 8664 and version 16.0.20326.20132; LibreOffice's
  launcher and process binary and 7-Zip are AA64. Word provides an installed
  x64-PE editor target, although runtime architecture/module inspection is still
  required. `docs/APPLICATION_ACCEPTANCE.md` now gives concrete per-control
  ordinary-input checks and evidence requirements, linked to the offline browser
  page. No application input case is marked passed. Corrected stale architecture
  paragraphs in the main acceptance audit that still described pre-install state.


- Built the ARM64 host with optional queued-key and per-event diagnostics;
  headless registered activation still passes. Scoped runs now use separate
  report filenames instead of overwriting dual-architecture evidence.
  Inspected actual PE headers for installed Chrome, Firefox, Edge and packaged
  Notepad: all are ARM64 (AA64), including Edge in Program Files (x86).
  `build/application-inventory.json` records paths and explicitly marks input
  acceptance untested. Added offline `tests/application_input.html` with input,
  textarea and contenteditable fields, ordinary-code/focus/selection steps and
  exportable composition/input events. It makes no network requests or automated
  acceptance claims. The page has not been opened/rendered or exercised in a
  browser; real-app x64 coverage still needs an actual x64 application.


- Per-event diagnostics caught input outside the explicit fixture sequence:
  ARM64 native UI already had UTF-16 122 (`z`) in its document before event 0,
  then the first explicit `A` produced `za`. This run cannot establish a product
  composition failure. Evidence: `build/registered-tsf-diagnostic-repeats.json`
  and `build/registered-ARM64-native-ui.stderr.txt`. Stopped foreground repeat
  runs rather than continuing while extra input is arriving. Added optional
  queued keyboard-message tracing to pump() for the next controlled run; that
  additional tracing is source-only until rebuilt. Earlier candidate/count
  failures still lack a proven cause and are not dismissed by this observation.
  Registered DLL and deployment remain unchanged.


- Ran visible TSF integration against the actual installed ARM64X DLL in isolated
  user roots. One complete ARM64/x64 × UI-less/native-UI run passed (36 events per
  variant, two contexts, real edit sessions, candidate finalization and layout
  recovery), and the ARM64 candidate PNG was visually inspected. Repetition
  exposed intermittent failures: initially context text disagreed after focus
  return; later ARM64 native UI returned two candidates instead of four after
  text `ab`. Do not treat the single passing run as acceptance. Current evidence:
  `build/registered-tsf-interactive.json` (intermittent-failure),
  `build/registered-tsf-interactive-repeats.json`, stderr files and
  `build/registered-candidate-ARM64.png`. The runner now persists failed runs
  instead of leaving a stale passing report. Added composition/text diagnostics
  to the host; production DLL remains unchanged. Root cause is not yet known.
  These fixtures call the TSF keystroke manager with actual OS focus; they do not
  send physical hardware keys or exercise a third-party application.


- Completed the real elevated deployment cycle: rolled back to retained
  `19466b1647bcaa2b`, independently verified the old registration, confirmed a
  stale new-generation uninstall record is rejected, reinstalled ARM64X,
  uninstalled it, verified COM/profile/owned shortcut removal, then reinstalled
  ARM64X `e3b95d4b9183e325`. The final new installation remains active.
  User-data file hashes and daily TigerClaw's separate COM registration are
  unchanged. Both architecture hosts pass registered activation again after the
  final reinstall. Evidence: `build/deployment-cycle.json`,
  `build/native-install.json`, `build/native-uninstall.json`, and refreshed
  `build/registered-tsf-validation.json`. This closes the deployment lifecycle
  gate for the installed generation; application input, rendered candidates and
  the remaining ordinary-input acceptance scope are still open.


- Installed and registered ARM64X generation `e3b95d4b9183e325` using the real
  elevated installer. Source and destination dual-loader checks pass, along with
  installed hashes, native COM construction, profile enablement and the manager
  shortcut. `build/native-install.json` and `install-arm64.log` record the run;
  an independent registry read confirms the exact new Program Files DLL path.
  Previous generation `19466b1647bcaa2b` is retained in
  `build/previous-install-e3b95d4b9183e325.json`.
  Added `--registered-activation-only` to tsf_host, bypassing private manifests
  without displaying windows. Fresh ARM64/x64 hosts both pass system profile
  activation, exact module/dictionary checks and explicit mode/deferred-lock
  callbacks against the installed DLL (`build/registered-tsf-validation.json`).
  This is actual installed registration activation, not real-application input
  acceptance. Rollback/uninstall/reinstall remain open; daily TigerClaw remains
  independently registered and its runtime directory was not modified.


- Installer now validates copied destination bytes and, for ARM64X, loads the
  DLL using both verifiers from that exact destination before regsvr32 runs.
  The shared Test-NativeTigerDeployment function is used for source preflight
  and destination verification; installation records retain separate
  `installed_load_checks`. ARM64 CheckOnly and ARM64X package regression pass.
  `tests/deployment_hash_test.ps1` imports only this validator via the parsed
  function definition, verifies both loaders and rejection of a mismatched DLL
  hash without executing the registration body. Evidence:
  `build/deployment-hash-validation.json` and refreshed
  `build/native-package-arm64x-preflight.json`. Actual Program Files deployment
  and registration remain unexecuted; the destination integration is not yet
  accepted by a real install run.


- Added a Linux journal flush-failure fixture, linked against the production
  UserStore/Lexicon/Dictionary/Text code with `--wrap=fsync`. Injected EIO after
  writing a complete two-Advance batch, once with successful rollback flush and
  once with a second injected rollback-flush failure. Both propagate the proper
  failure stage, restore exact original bytes in the running system, reload the
  original candidates, and permit a retry identical to one successful batch;
  retained immutable snapshots remain unchanged. Report and source/binary hashes:
  `build/user-store-flush-failure.json`; fixture:
  `tests/user_store_flush_failure.cpp`. This does not test physical storage
  failure, power loss, crash durability or Windows FlushFileBuffers. In the
  double-failure case, bytes readable after truncation do not prove durable
  rollback. Existing production behavior passed; no persistence code changed.


- Extended `tests/run_tsf_host.ps1` with `-Arm64X` and `-X64Host`, requiring
  private activation and selecting a matching host/manifest. Report and candidate
  capture names include architecture so x64 acceptance cannot overwrite ARM64
  evidence. Ran `-PrivateBuild -Arm64X -ActivationOnly` with and without
  `-X64Host` using an isolated temporary user root: both pass module/dictionary
  verification, mode compartments, explicit mode/focus callbacks and deferred
  mode-lock lifecycle checks. Registration stays at `19466b1647bcaa2b`.
  Evidence: `build/tsf-private-arm64x-ARM64-validation.json` and
  `build/tsf-private-arm64x-x64-validation.json`. No visible windows, physical
  keyboard routing, candidate capture or real-application acceptance was exercised.
  Full desktop variants are prepared but have not been run through this wrapper.


- Rebuilt the ARM64 engine probe and replayed all six non-default settings
  profiles: 41,100 events, zero mismatches against the retained original-Core
  outputs (`build/settings-key-parity-arm64.json`). Replay verifies the existing
  trace/settings/selection fingerprints; the report now records the executable,
  dictionary and oracle-output hashes and explicitly excludes physical input and
  TSF DLL execution. This refreshes component evidence, not application acceptance.
  The build exposed MSB8028 shared intermediate files among imported probe
  projects. DictionaryProbe now derives its intermediate directory from project,
  platform and configuration. EngineProbe and DynamicEngineProbe both rebuilt
  without that warning in distinct directories; building the latter preserved
  the exact executable hash used by the settings replay.


- Fixed rollback preflight accepting snapshots with an absent/empty hash.
  SHA256 is now mandatory before elevation; snapshot paths are read literally,
  including names containing brackets. `tests/rollback_preflight_test.py` checks
  the actual retained generation `74bdb4d2303e29a1`, five missing/invalid/mismatched
  hash cases in both CheckOnly and non-CheckOnly validation, and a valid bracketed
  filename. All pass (`build/rollback-preflight-arm64.json`). Invalid cases stop
  before elevation or registration; this is not an actual rollback acceptance.


- ARM64X installer integration now passes package preflight. `-Arm64X` selects
  the isolated package and requires actual ARM64/x64 loader and class-instance
  checks before elevation. Both verifier executables are hashed and deployed.
  `build/native-package-arm64x-preflight.json` records generation
  `e3b95d4b9183e325`, missing/wrong verifier refusal and real error-193 refusal
  when an ordinary ARM64 DLL is substituted. No installation was performed.
  Ordinary ARM64 package regression also passes after rebuilding companion tools
  (`build/native-package-preflight.json`), including default scheme selection
  and missing/wrong-architecture tool rejection. The ARM64X artifact manifest
  was refreshed from verified file hashes and now includes both verifiers.
  Full ARM64 and x64 management-menu reports now both pass synthesized popup
  selection, child foreground, close/exit and unassisted host focus restoration.
  Configuration and registration are unchanged. The ARM64 input-settings PNG
  was visually inspected; full candidate/application/DPI acceptance remains open.


- ARM64X cross-architecture sharing and synchronization now have direct evidence.
  `tsf_memory_test.py --arm64x --pair` runs exactly one ARM64 and one x64 TSF
  host after confirming no existing tsf_host process: each reports 10,229 of
  10,229 dictionary pages resident and multiply shared. Idle activation adds
  about 0.75 MiB/0.77 MiB private memory over the respective host baselines;
  total process private memory is 4.10 MiB/13.64 MiB. This does not duplicate the
  approximately 40 MiB dictionary into each private heap. Evidence:
  `build/tsf-memory-pair-arm64x.json`.
  The four-host active fixture (two of each architecture) also shares every
  dictionary page; median explicit TSF tap time is 10.8–11.2 us for ARM64 and
  14.8–14.9 us for x64-hosted ARM64EC code. This is not physical input latency:
  native rendering and watcher pumping during the measured loop are excluded,
  and the live-reader fixture ran concurrently (recorded in the report).
  Evidence: `build/tsf-memory-active-arm64x.json`.

  `tsf_live_readers_test.py --arm64x` passes all eight stages/32 observations
  with continuously pumping mixed hosts: settings, user words, schema and
  generation changes, explicit-save cancellation/default mode, and duplicate
  saves preserving new input. `UserStoreECProbe.vcxproj` additionally builds the
  actual storage implementation for ARM64EC. Mixed ARM64/ARM64EC probes pass four
  concurrent writers/48 retained entries, 71 interrupted-tail positions and
  checksum refusal on both ABI paths (`build/user-store-mixed-arm64ec.json`).
  This tests storage concurrency separately from TSF UI dispatch. Both rebuilt
  probe hashes are recorded. `tests/tsf_architectures.py` validates host PE
  machines and emits matching private manifests. The experimental DLL remains
  `8a9fd592e143d7df636c2acef0f510dde33bb8023329fb948e253a31022b734d`;
  registration and production binaries are unchanged. Installer integration and
  physical application acceptance remain open.

- Implemented an isolated ARM64X build and passed actual ARM64/x64 TSF hosts.
  `build_arm64x.ps1` builds Release|ARM64EC with BuildAsX, compiling both ABIs
  into `build/ARM64X/ARM64EC/Release/Tigirl.dll`; the native half uses separate
  intermediate/output directories. The script stages all four ARM64 companion
  executables plus the dictionary/fonts. The ordinary ARM64 DLL remains byte-
  identical (`ddfbc5cf...`); registered generation remains `19466b1647bcaa2b`.
  Both loader probes now load and construct COM objects from experimental DLL
  `8a9fd592e143d7df636c2acef0f510dde33bb8023329fb948e253a31022b734d`
  (`build/architecture-load-both.json`). `TsfHostX64.vcxproj` builds a real x64
  host, using an amd64 activation manifest beside the same ARM64X DLL.
  `tests/arm64x_tsf_test.py` passes both hosts: private system activation and
  mapped-dictionary checks, explicit input callbacks, failed adjustment warning/
  restored order, explicit retry over an interrupted journal tail, persisted
  order read by an independent process, secure reactivation, and both real
  language-bar management launch/close actions. Config/registration are preserved.
  Evidence: `build/arm64x-tsf-validation.json`, `build/arm64x-artifacts.json`.
  These are not physical keys, popup rendering, foreground restoration or real
  applications; cross-architecture simultaneous sharing/concurrency, installation
  integration and the remaining acceptance matrix are still open.

  The v145 ARM64EC link exposed LNK4279 for incompatible SetFilePointerEx exit
  thunks between UserStore and static UCRT. The ARM64EC-only journal seek now
  uses the equivalent low/high-offset SetFilePointer call, preserving 64-bit
  positioning and error checks; other platforms keep their existing calls.
  The warning no longer appears after relinking. The final dual-host test
  explicitly repairs an injected incomplete tail before persisting the retry.
  API semantics: [SetFilePointer](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfilepointer).

- Added a requirement-by-requirement audit (`docs/ACCEPTANCE.md`) and the actual
  build/install/rollback/uninstall workflow (`docs/INSTALL.md`). The audit exposed
  an architecture gap, now reproduced rather than inferred: an actual ARM64
  loader creates the current DLL's COM object, while a separately built x64
  loader fails LoadLibraryExW with error 193. Both run on the same ARM64 Windows
  system; probe PE machines and all hashes are recorded in
  `build/architecture-load-arm64-only.json`. The small loader does not claim TSF
  activation or application input. `ArchitectureLoadProbe.vcxproj` and
  `architecture_load_test.py --expect both --dll <experimental DLL>` provide
  the next architecture gate. Local v145 ARM64EC platform files/libraries exist;
  the current service project has no ARM64EC/BuildAsX configuration. Building and
  validating an isolated ARM64X output is the next priority before claiming
  compatibility with x64 applications on Windows on Arm. Current ARM64 package
  and registration remain unchanged. Full application and deployment acceptance
  are still open, along with the other gaps enumerated in the audit.

- Fixed candidate-style state surviving Service::Deactivate. A retained TSF
  instance could load the user's hidden-candidate setting in normal mode, then
  reactivate with TF_TMAE_SECUREMODE and keep that style even though secure
  activation bypasses user configuration. The expanded real-service fixture
  reproduces the missing native candidate window on the previous DLL
  (`build/secure-reactivation-before.json`, SHA-256 `16457ebd...`). Deactivation
  now resets CandidateStyle alongside Config. The same fixture passes on DLL
  `ddfbc5cf22da4f6941fa9d7824f8b7c620aaac4dc5350e64c196d870c7ff6be7`:
  normal reactivation loads hidden style and edited order; secure reactivation
  restores the default candidate window and base order, and suppresses the
  adjustment warning. Previously saved user ordering remains available to an
  independent reader. Evidence: `build/word-save-failure-arm64.json`, host
  `71d4b52647a9c8cdfc287fe2a5c8f13c77f5b9d4c19a66c5bd1eb01d7d310157`.
  This is explicit service activation flags, key callbacks and native-window
  existence, not Windows sign-in or physical desktop rendering. Full ARM64
  package rebuild, read-only installer validation and existing private activation/
  mode callbacks pass (`build/native-package-check.json`,
  `build/tsf-private-validation.json`). Registration was not updated.

- Failed user-word adjustments now publish a persistent-within-service language
  bar warning: a warning icon, `中!`/`英!` text and a tooltip explaining that the
  last adjustment failed and must be explicitly repeated after checking storage.
  The existing persisted-order restoration remains. Ordinary typing and reads
  do not clear the failure; a successful user-word write or schema switch clears
  it. No automatic retry, modal dialog or extra process was introduced. Failed
  operations are still not durably queued, and this status does not survive
  service deactivation. Secure-mode bars suppress it.
  `tests/word_save_failure_test.py` activates the private system profile, then
  exercises an actual service/context via explicit TSF callbacks: preview has no
  warning; an OS sharing lock makes Ctrl+2 fail; original candidate order and
  subsequent ordinary commit survive; tooltip/text/icon are available; explicit
  retry clears the warning; an independent journal reader sees the saved order.
  All pass (`build/word-save-failure-arm64.json`). This verifies TSF status and
  resource contracts, not physical language-bar rendering or keyboard input.
  The warning icon is a sized owned copy of a shared system resource, following
  [LoadImageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadimagew)
  and [CopyImage](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-copyimage)
  ownership requirements. Current DLL SHA-256 is
  `16457ebd90dc09ab02bb160a4d2c82e012c16e12b475720eb8459e67f31db32a`,
  host `642cba789895b263b64e1bb2f085d7ab129d396868d201a961b94a9d9b60a714`.
  Full package rebuilt; read-only installer validation passes
  (`build/native-package-check.json`). Existing private activation/mode/menu
  callbacks pass again on this binary (`build/tsf-private-validation.json`).
  No installation was performed; physical desktop acceptance remains pending.

- Selection-settings recovery now also handles malformed Unicode. File reading
  and decoding are separate bounded operations, so actual read/access errors
  still prevent editing instead of becoming permission to overwrite. The editor
  starts with defaults and a recovery notice for encoding or syntax failures;
  repair compares the exact opening bytes under the stable configuration lock,
  copies the original to a unique `.invalid.{GUID}` backup, then publishes valid
  bindings atomically. This strengthens the former decoded-text comparison and
  leaves ordinary TSF rejection of invalid Unicode unchanged. Native-control
  tests pass broken UTF-8, truncated UTF-16, an unpaired UTF-16 surrogate and an
  invalid UTF-32 scalar: cancel preserves bytes, concurrent byte changes reject
  saving, denied reads never enter repair, and successful backup bytes match
  exactly. Existing settings/binding/control checks also pass
  (`build/input-settings-arm64.json`, manager SHA-256
  `e3c1bec1432b5a589270ab38ddaae40f8283a9dcbfb7c4a45dbda5123cf9d476`).
  Rebuilt configuration probe passes nine encoding cases, four concurrent writers
  (52 toggles), two readers (2,000 iterations), denied replacement and invalid/
  oversized-content preservation (`build/config-store-arm64.json`, probe
  `2de882304c91704d1527a963fd25a674ea2948a063524e5e15e587b638e44a26`).
  Rebuilt Linux lexicon probe retains 27-operation original-Core parity. Source
  and package fingerprints are in `build/selection-unicode-recovery-validation.json`.
  Full ARM64 package and isolated package preflight pass; DLL SHA-256 is
  `e86480de87667e38440ec65556698cdd592e62ee347411013cc703c22bf69d28`.
  `build/native-package-preflight.json` records current staged artifacts. No
  installation was performed; earlier TSF runtime reports retain their own hashes.
  These are hidden native controls, not physical desktop acceptance. Windows
  remains on the `Screen-saver` input desktop with foreground HWND zero.

- Added actual language-bar-to-manager process integration coverage using the
  system-activated private TSF instance. `tests/run_management_menu.ps1
  -DirectActions` obtains the installed TSF item interface, invokes each actual
  management action, verifies exactly one own child, waits for the corresponding
  visible-style schema/settings window, closes it and checks successful process
  exit. Both actions pass; isolated config bytes and registered DLL path remain
  unchanged. Evidence: `build/management-actions-arm64.json`, DLL
  `d5788f3e65e7c91cdc6494834b29739462f4bc750c01acaeb03408fcf36c4277`,
  host `9df9a9c32643b4bd845f18aa77b8c2408e8c8fa7ce7ebe3c721abf769fb1a8ab`,
  manager `7c802bd3939972dd253bad1f57f476109c899649dcc97e0546d495ab61f04a22`.
  This closes the earlier gap between separate menu-contract and utility-launch
  tests; it does not validate popup input or foreground behavior.
  The same script without `-DirectActions` is prepared to exercise OnClick's
  actual popup with OS-synthesized Down/Enter input, capture both manager windows,
  and require unassisted host-foreground restoration after close. It is **not
  passed**: current Windows input desktop is `Screen-saver` and foreground HWND
  is zero. The script now checks this before opening a fixture and explicitly
  reports the interactive test as unverified. No desktop switch/unlock was
  attempted. Popup/rendering/focus and real-application acceptance remain open.
  Production binaries were unchanged in this step.

- User-journal replay and batch commits now build a fresh unpublished Lexicon
  in place, avoiding a copy of the entire edited-code map for every record.
  Public `Lexicon::changed` retains immutable snapshots; only UserStore can call
  the private mutation method, and it starts with a newly decoded object under
  the existing journal lock. In a 4,000-distinct-code fixture, three-replay median
  on Linux fell from 338.898 ms to 10.2504 ms; current ARM64 median was 6.3346 ms
  (no ARM64 before measurement). Retained snapshots, ordered add/delete/top/
  advance batches, restart equivalence and base-dictionary pointer sharing pass.
  Evidence: `build/user-store-replay-before.json`,
  `build/user-store-replay-after.json`,
  `build/user_store_replay_probe-arm64.json`, with executable hashes.
  Rebuilt Linux public-mutation probe also matches original Core for 27 operations
  (`build/lexicon-parity.json`). Rebuilt ARM64 journal probe passes four concurrent
  writers/48 retained entries, 71 interrupted-tail positions and corrupt-record
  refusal (`build/user-store-arm64.json`). Linux short-write injection passes all
  88 positions with complete batch rollback and retry
  (`build/user_store_short_write-linux.json`). Whole-file reading/replay and the
  128 MiB journal limit remain; this change does not provide compaction or caching.
  Full ARM64 package rebuilt; DLL SHA-256 is
  `d5788f3e65e7c91cdc6494834b29739462f4bc750c01acaeb03408fcf36c4277`.
  Package preflight passes tool inclusion, wrong/missing artifact refusal and
  isolated default-schema manager selection (`build/native-package-preflight.json`).
  No installation was performed. Aggregate source/binary fingerprints and checks
  are recorded in `build/user-store-replay-validation.json`; earlier TSF runtime
  reports retain their own binary hashes and are not current-package retests.

- Added startup handoff fault coverage without changing production binaries.
  A bounded no-acknowledgement executable exercises the native launch API's
  five-second timeout (observed 5,098 ms); accepted request/deadline remain
  unchanged and the previous timer still expires normally. A separate controller
  starts the real armed helper, then lets its parent exit without signalling
  either GO or CANCEL. Before publication the helper exits without firing; after
  publication it reads the accepted ledger and fires despite the missing GO.
  Both outcomes and clean helper exit pass. Evidence:
  `build/reminder-launch-arm64.json`, `build/reminder-handoff-arm64.json`;
  helper SHA-256 remains
  `5932d8362607903b4e5513fe50fb9c8fae5536840dcdc34c9cf4a3e1fb0762ba`.
  Fixtures are `ReminderStallProbe.vcxproj`, `reminder_launch_test.ps1` and
  `reminder_handoff_test.ps1`. The stall fixture exits on its own after six
  seconds; event-based tests do not open user-facing reminder windows.
  Restrictive job objects, unavailable ledger recovery and locked-desktop
  behavior remain open, along with the broader ordinary-input acceptance scope.

- Added direct TSF host-exit/replacement validation. `tsf_host --timer-detach`
  activates the real private service, submits `Ds1` through explicit key sink
  callbacks, verifies empty committed text and exactly one reminder child,
  deactivates TSF, verifies the child remains alive, and exits the host process.
  `tests/timer_host_exit_test.ps1` runs two such hosts against one isolated root:
  their distinct host PIDs exit before expiry, the second command terminates the
  first pending helper without a popup, and the remaining helper displays the
  expected reminder after 60,031 ms. The controller dismisses that helper's own
  button and verifies process exit. Registration remains unchanged.
  Evidence: `build/timer-host-exit-arm64.json` records both host/helper PIDs,
  twelve explicit callbacks per host, deactivation, timing and executable hashes.
  Host SHA-256 `1951bb7e6e840e715a049ba95b82c19a82049d80c2e279604e888274542eeed0`;
  production DLL/helper are unchanged from the preceding build. Shared popup
  inspection code is now in `tests/reminder_window.ps1`.
  This proves actual TSF deactivation/host-exit survival and replacement between
  separate TSF processes. It is not a physical-keyboard or real-application
  matrix check, and does not cover restrictive jobs or locked desktops.

- Connected actual TSF timer dispatch to `ReminderLaunch` and packaged
  `Tigirl.Reminder.exe`. The native launch API uses separate readiness and release
  events: it publishes the accepted ledger ticket only after the child reports
  ready, and cancels startup on failure. The child checks its parent's creation
  time to avoid PID reuse and can recover publication from the ledger if that
  parent exits before sending release. Secure-mode dispatch remains excluded.
  TSF deactivation no longer owns/cancels a manual timer; live data refresh keeps
  its existing internal scheduler. Native launcher probes verify real API launch,
  paths containing spaces, survival after the caller exits, and preservation of
  the old timer for missing executables or unsuccessful child startup.
  ARM64 packaging and updated install preflight pass, including required helper
  presence/hash/architecture checks. Fixed the preflight's GUI invocation to
  explicitly wait for manager completion; the earlier invocation returned zero
  while the expected config file was absent. The waiting version passes.
  Both actual foreground TSF timer variants pass: preview creates no helper,
  submission creates exactly one child, `Ds1` leaves no literal commit, reminder
  text/button are correct, and dismissal ends the child. Expiry observations:
  UI-less 60,062 ms, native UI 60,078 ms. The harness identifies its own helper
  child and cleans it up on test failure, avoiding unrelated reminder windows.
  Evidence: `build/reminder-launch-arm64.json`,
  `build/tsf-private-timer-validation.json`, `build/native-package-preflight.json`.
  DLL SHA-256 `55afa85be4cd93409932687a86d523b9bcf8bb61c3f2e7b98b13f5e770211722`;
  helper SHA-256 `5932d8362607903b4e5513fe50fb9c8fae5536840dcdc34c9cf4a3e1fb0762ba`.
  Installation/registration is unchanged. The subsequent entry above verifies
  TSF host-exit/deactivation and cross-process replacement. Startup timeout/
  crash-window fault injection, restrictive job objects, unavailable ledger
  recovery and locked-desktop behavior. This does not complete the full goal.

- Connected the reminder helper to the durable ledger via `--ledger` and an
  explicit event-based test mode. Startup opens its acknowledgement channel
  before accepting a reserved ticket; the running helper checks the accepted
  epoch/sequence and deadline from locked snapshots. Old and duplicate launches
  now exit even after every preceding helper has completed, and tickets from a
  removed/recreated ledger cannot supersede its new epoch. The expanded process
  suite verifies reserve-without-cancel, startup failure without cancellation,
  actual replacement, delayed/duplicate startup after all helpers exit, recreated
  epoch isolation, launcher-exit survival and the actual popup/button/exit path
  using the ledger protocol. `build/reminder-process-arm64.json` records helper
  SHA-256 `6cc370cb3f7e9b0d9b1458d0dfa01745a9331d58c337fa9adb6d7496ec1bfeab`.
  TSF launch integration and packaging remain pending. This does not establish
  survival under restrictive host job objects or recovery when the ledger path
  becomes unavailable; current helper I/O failures still terminate with an error.
  The old ephemeral protocol remains available for prototype regression tests;
  production TSF still uses its existing in-process timer.

- Added a durable reminder ledger API (`native/ReminderLedger.h`, implemented
  using ConfigStore's existing stable lock and atomic replacement). Reserving
  a monotonically increasing ticket leaves the accepted timer and deadline
  unchanged. Acceptance rejects older, duplicate, unreserved and wrong-epoch
  tickets; a recreated ledger receives a fresh UUID epoch. Persisted acceptance
  survives the disappearance of all shared-memory/process owners. Empty or
  malformed existing records and exhausted counters fail without replacement.
  `tests/reminder_ledger_test.py` passes 100 reservations from four independent
  ARM64 writers, preserved active state, stale acceptance after all earlier
  processes exit, denied replacement cleanup and recreated-epoch rejection.
  Evidence: `build/reminder-ledger-arm64.json`, probe SHA-256
  `982362009fa0eaab89648f048c15d35b6f06303f7e3c7020a730e174c0f27b05`.
  The subsequent entry above connects this ledger to the helper. TSF launch
  integration remains necessary before production ordering/lifetime gaps can be
  considered resolved. No installation
  or existing user timer state changed; the tests use disposable ledger paths.

- Verified the standalone reminder's real popup path: its own process displays
  the expected caption/text, the discovered confirmation button dismisses it,
  and the process exits. The test filters windows by the exact owned process ID.
  Added an optional startup-acknowledgement event to normal `--timer` launches;
  it is opened before publishing shared timer state and signalled after startup.
  A missing acknowledgement event causes startup to fail before replacing an
  existing timer. The expanded lifecycle suite verifies this failure preserves
  the older active request, alongside replacement, stale-request rejection,
  parent-exit survival and completion exit. `build/reminder-process-arm64.json`
  records passing actual-popup/startup checks and helper SHA-256
  `2169a8034571abc66cd6a465fefc1e34a02eefbe61f82bbb7139e4a1f77703e8`.
  TSF caller integration, durable request ordering, stable scope and packaging
  are still pending; the prototype remains outside the installed input path.

- Added a standalone ARM64 reminder lifecycle prototype in
  `tools/timer_reminder.cpp` / `tools/Reminder.vcxproj`. It does not load the
  dictionary or participate in input. A scoped, mutex-protected shared mapping
  lets a newer request supersede a pending older one; replaced/completed helpers
  exit. `tests/reminder_process_test.ps1` verifies cross-process replacement,
  stale-request rejection while the newer request is active, no early expiry,
  completion exit and expiry after the launching process has already exited.
  The initial lifecycle tests use explicitly named result events;
  `build/reminder-process-arm64.json` records the executable hash and test scope.
  This is not yet the production timer implementation. TSF launch/acknowledgement,
  stable per-user scope, launch-failure recovery, delayed stale launches after
  all mapping owners exit and package integration
  remain pending. The prototype is not included by `build_arm64.ps1` or installed;
  the existing TSF timer still has its previously documented lifetime limits.

- The foreground desktop became available again. Re-ran the previously blocked
  current-package timer acceptance with disposable configuration: both UI-less
  and native candidate-window variants pass all 44 TSF events, actual one-minute
  popup, expected reminder text, button dismissal and scheduler teardown.
  `build/tsf-private-timer-validation.json` matches current DLL and test-host
  hashes and confirms unchanged installed registration. These are
  ITfKeystrokeMgr callbacks with verified OS/TSF focus, not physical keyboard
  injection. Reference source inspection reconfirms that the original single
  ProtocolHandler owns the engine timer in the long-lived Core. The native
  per-service timer still cancels on deactivation; cross-application replacement,
  host-exit survival and locked-desktop behavior remain open. Passing expiry
  checks does not resolve those lifecycle differences.

- Added individual binding removal and per-candidate defaults to the native
  selection-key editor. Each candidate now has a keyboard-accessible dropdown
  listing its bindings, Delete Binding, Row Defaults, Add Key and Clear actions.
  Empty rows display an unbound placeholder and disable deletion; dropdowns
  expand wider than the closed control so long key names remain accessible.
  Native control tests verify deleting the selected binding preserves the other
  binding, row defaults leave all other candidates untouched, and empty-row
  deletion is disabled and harmless. Existing save/merge, cancellation, malformed
  recovery and 96/144/192 DPI client-bound checks pass. ARM64 packaging passes;
  `build/input-settings-arm64.json` records manager SHA-256
  `a1f191d217bb537d70e695f24894aaca47239828de8d59eb6ee564c5efcfbe24`.
  This is native control/dispatch verification; physical dropdown interaction
  and visual acceptance remain pending. Installed registration is unchanged.

- The selection-key editor now opens on decodable but malformed selection text,
  explicitly displaying default bindings for recovery. Save repairs the file
  through the existing locked atomic replacement and first copies its exact
  original bytes to an adjacent uniquely named `.invalid.{GUID}` backup. The
  repair checks that the current text still matches the editor's snapshot and
  rejects a concurrent change; Cancel leaves the file intact. Error details stay
  visible with edits retained in the window. The stale refocus-required notice
  now says selection changes apply automatically. Original ProtocolHandler's
  `set_selection_key_config` path confirms this is a selection-map update, not
  the full settings-save mode/composition reset.
  ARM64 packaging and `tests/input_settings_test.py` pass, including real native
  control actions for malformed-file Cancel, concurrent modification rejection,
  recovery to all ten default bindings and an exact-byte backup assertion. The
  existing settings/shortcut/layout checks also pass. Current manager SHA-256:
  `036dd264a27479d679ffa570f6a8d560107ecac8541f62612f30d01372809f17`;
  staged and tested manager binaries match. Current rebuilt DLL SHA-256:
  `b6a01aa30c080175e894c3508e45fad0e65e85b21df9c84123957652eb9c620a`.
  Evidence: `build/input-settings-arm64.json`, `build/native-package-build.log`.
  This recovery covers selection syntax errors; undecodable/oversized files and
  read-access failures are still reported by the reader rather than opened for
  repair. The subsequent entry above adds individual removal and per-row defaults;
  physical window acceptance remains unfinished. Earlier
  TSF behavior reports retain their own prior DLL hashes. Installation unchanged.

- Extended the four-process live TSF test through explicit settings save and
  duplicate delivery. A separate native writer calls `saveInputConfiguration`
  once to select default English. All four continuously pumping TSF processes
  discard their existing `ab` composition without committing it, end candidate
  UI and publish English mode. Each reader then explicitly returns to Chinese
  and types fresh `ab`; republishing the identical request plus an unrelated
  notification preserves that code and mode throughout 5.5 seconds, including
  periodic reconciliation. The six previous settings/journal/schema/generation
  propagation stages also pass: eight stages, 32 observations, four distinct
  Windows PIDs. This verifies cross-process explicit-save propagation, without
  a resident Core or per-key IPC. It uses explicit TSF callbacks, not physical
  keyboard input or real application acceptance. Registration is unchanged.
  `build/tsf-live-readers-arm64.json` records DLL/host/writer hashes; the DLL is
  unchanged from the previous settings-save build. Atomic request persistence
  tests also pass with the rebuilt saver (52 concurrent saves, 1,000 snapshots).

- Connected InputSettings Save to the atomic reload-request helper. Each copied
  engine tracks the applied request; a new identifier cancels ordinary/mixed raw
  input, resets digit punctuation and paging, and applies the default mode while
  preserving history and held Shift state. Duplicate notifications preserve new
  input and manually selected mode. Schema switching with a pending explicit
  save also cancels old code. Fresh engines adopt the existing identifier.
  TSF edit sessions apply composition changes, retaining pending requests across
  lock denial, deferred grants and document focus changes. Focus transient state
  is cleared at the actual notification rather than at a later edit grant;
  deferred focus edits check context identity and foreground before applying.
  ARM64 packaging, native settings-control tests and the dedicated engine reload
  probe pass. Expanded private TSF tests prove cancellation without raw commit,
  duplicate-request idempotence, newest settings at deferred grant, denied-lock
  retry, stale background edit rejection and reload on returning to a document.
  The current ARM64 engine also passes all 41,100 settings-key events against
  the original-core oracle. Evidence: `build/configuration-reload-arm64.json`,
  `build/input-settings-arm64.json`,
  `build/tsf-private-schema-activation-generation-validation.json`
  (`settings_save_reload`), and `build/settings-save-validation.json`.
  Final DLL SHA-256:
  `57b3938dd2139b606aeb7108d9f25fd4326db0c21649878972ea7e9a7236f0e3`.
  Registration remains generation `19466b1647bcaa2b`. Cross-process explicit-save
  propagation is verified in the subsequent entry above. Physical keyboard and
  settings-window acceptance, and the full application/installation scope,
  remain unfinished.

- Added `saveInputConfiguration`, which publishes changed settings and a fresh
  UUID reload request in the same validated, locked atomic replacement. Empty
  changes do not create a file or request; validation and denied replacement
  preserve the previous request. A denied replacement now removes its temporary
  file only after verifying the original configuration is unchanged and no
  backup exists; uncertain replacement failures still retain recovery artifacts.
  ARM64 `settings_reload_request_test.py` passes three unique sequential requests,
  52 concurrent saves and 1,000 paired reader snapshots, with immediate cleanup
  checks after no-op, rejected validation and denied save. Existing config-store
  regression tests also pass (nine encoding cases, four writers, two readers and
  2,000 reads). Reports: `build/settings-reload-request-arm64.json` and the
  existing config-store report; probe SHA-256
  `4380283b10520fee914c6b95388eac22a44ce3358b009344652cd036ace10d43`.
  This report covers persistence only; subsequent GUI and TSF integration is
  verified separately in the entry above.
  No package installation or registration changed during these tests.

- Fixed a deterministic duplicate-focus discrepancy against original
  ProtocolHandler.HandleFocusMessage: the original clears key state only when
  focus metadata changes, while the native service unconditionally called
  Engine::focusChanged and reset configuration paging. A repeated notification
  for the same context now preserves the page, held modifiers, observed-key
  tracking and mode-request revision. Effective settings refresh remains
  idempotent. Actual key-sink foreground loss explicitly clears transient key
  state, matching the existing thread-focus-loss path. The expanded private
  generation suite verifies duplicate focus while on page two with Shift held,
  subsequent Shift release, and duplicate focus while an English-mode edit is
  deferred. All pass, alongside the existing live-update and recovery cases.
  Pre-fix failure is preserved in `build/tsf-duplicate-focus-before-fix-validation.json`
  and `build/tsf-duplicate-focus-before-fix.stderr.txt`; current evidence is
  `build/tsf-private-schema-activation-generation-validation.json` with
  `duplicate_focus_preserves_state`. ARM64 packaging passes; installed registration
  remains unchanged. This fixes the reproduced duplicate-focus defect, but does
  not prove that it was the sole cause of the earlier foreground Ctrl-repeat
  failure.
  Reference inspection also identified the settings-save discrepancy:
  ProtocolHandler initializes mode from GetDefaultChinese in its constructor and
  again for reload_config. ConfigWindow's save path sends set_config for changed
  fields, then reload_config; that command resets composition and reapplies the
  default mode. Native live file refresh intentionally preserves active code.
  This finding motivated the separate explicit-save behavior implemented above.
  The old “new activation only” notice has been replaced accordingly; ordinary
  file invalidation remains distinct from explicit Save.

- OS foreground became available, allowing the current DLL's foreground TSF
  suites to run in both UI-less and native-window modes. Scheme switching passes
  162 events per variant, ordinary mixed input 76, and add-word input 74 plus 12
  dialog key events with focus restoration. These exercise ITfKeystrokeMgr with
  verified OS/TSF focus and real edit sessions; they are not physical keyboard
  injection. Mouse selection sends the native window's mouse message directly.
  Reports are `build/tsf-private-{schema,mixed,add-word}-validation.json` and record
  the exact private DLL/host hashes. Installed registration is unchanged.
  Capturing the layered candidate through its client DC produced black pixels;
  desktop capture also needed physical DPI-aware coordinates. The helper now
  captures the borderless popup's physical bounds from the desktop with CAPTUREBLT
  after DwmFlush, restoring the previous thread DPI context afterward. Inspected
  `build/native-candidate-current.png` shows the four ab candidates, annotations,
  selected-row background and complete popup bounds at 259 by 203 pixels.
  `build/native-candidate-capture.json` records its provenance. This is a real
  composited crop, not the WM_PRINTCLIENT rendering surrogate considered during
  diagnosis. Broader DPI/font/theme and application visuals remain unverified.
  Startup failures showed open-mode compartment 0 despite an enabled language
  bar and no Ctrl/Shift pressed. Foreground tests now explicitly establish Chinese
  mode through the standard compartment before testing Chinese input. The policy
  between defaultChinese and externally restored input mode still needs review;
  this setup is not proof of default-mode fidelity. An earlier held-Ctrl schema
  repeat assertion also failed once and passed in subsequent runs; no production
  fix for that intermittent observation is claimed. Timer tests subsequently
  failed their OS-focus gate before event 0, so foreground automation stopped
  when another application occupied the foreground. Timer acceptance remains open.

- Added and passed `tests/tsf_live_readers_test.py`: four independent ARM64 TSF
  host processes continuously pump their own message loops against one shared,
  isolated user root. Each holds a real text-store composition containing `ab`.
  Six successive mutations cover page-size changes, a separate native writer
  appending a built-in-schema user word, switching to an imported schema, a user
  word in that schema, and publishing a same-name immutable dictionary generation.
  All four readers acknowledge every stage (24 observations), verify candidate
  counts/pages and the actual `live0` candidate label, and preserve the raw text
  and active composition throughout. The generation stage verifies the newly
  mapped file in each process. Each reader closes its refresh scheduler on exit;
  the installed COM registration path is unchanged. The controller's immutable
  per-stage files live outside the watched user root; no test-control protocol is
  part of the production DLL. This uses explicit initial key/focus callbacks and
  UI-less candidates, not physical keyboard routing or native rendering. It does
  not measure update latency or physical page sharing for the new generation.
  Evidence: `build/tsf-live-readers-arm64.json`, with DLL, host and writer hashes.
  The production DLL remains
  `3e81d1a14b3613d484d64ab0c384c011a7e1fa7d0e471917d2588eab0e9b35c3`.
  Broader recovery, application/visual acceptance and the other full ordinary
  experience requirements in PLAN.md remain open.

- Fixed a reproduced live-refresh recovery bug: temporarily moving the user root
  away made an already active service recreate it and fall back to default
  settings. Initial activation may still initialize missing user data, but a
  service with a loaded schema now retains cached configuration/lexicon when its
  root is unavailable. Both periodic reconciliation and focus reload honor that
  distinction. The private fixture moves the root away for 5.5 seconds, verifies
  no replacement directory appears and candidate paging/raw code survive,
  exercises focus reload while it is absent, restores it, waits for notification
  reopening and verifies a subsequent page-size save applies without changing
  raw input. The fixture uses the bundled dictionary outside the moved root:
  the earlier variant with mapped schema descendants could not rename the root
  on this Windows host. A standalone directory-watch probe confirms the watcher
  itself permits root relocation and releases its handle correctly.
  The failing pre-fix evidence is preserved in
  `build/tsf-live-root-before-fix-validation.json` and
  `build/tsf-live-root-before-fix.stderr.txt`.
  After the fix, the full private generation suite passes twice with 40 explicit
  key callbacks; reports are `build/tsf-live-root-pass1-validation.json` and
  `build/tsf-private-schema-activation-generation-validation.json`, including
  `live_root_restore`. An intermittent immediate mode-publication assertion was
  changed to allow the existing posted compartment retry to complete within one
  second; final Chinese mode and the later language-bar checks remain required.
  Current DLL SHA-256 is
  `3e81d1a14b3613d484d64ab0c384c011a7e1fa7d0e471917d2588eab0e9b35c3`.
  ARM64 packaging passes and installed registration remains unchanged. This
  proves temporary relocation/restoration of the same user directory, not every
  fresh-directory replacement or permission-denial scenario. Multi-reader live
  propagation, broader recovery and actual application acceptance remain pending.

- Added passing live-refresh background/foreground checks to the private TSF
  host. Both `ITfThreadFocusSink::OnKillThreadFocus` and
  `ITfKeyEventSink::OnSetFocus(FALSE)` are exercised with a deferred refresh.
  Granting the old edit and publishing another settings change while backgrounded
  preserves raw text and keeps candidates hidden. Foreground recovery uses the
  newest settings. The key-sink variant deliberately sends only foreground TRUE,
  without a new document-focus notification: pending refresh resumes through the
  scheduler. The report now records `live_refresh_background` separately, and
  existing schema/journal, malformed-file, lock-denial, stale-document-focus and
  reactivation checks also pass. The production DLL/installed registration are
  unchanged. This verifies explicit TSF callbacks with message pumping, not actual
  OS foreground routing or rendered application UI. Root removal/recreation,
  multiple live readers and application acceptance remain pending.

- Expanded the actual private TSF host's live-refresh failure/race coverage.
  A denied text-store lock leaves the current candidate pages and raw code
  unchanged; releasing the denial permits retry without a new file mutation.
  Selecting a nonexistent schema preserves the previous configuration, then a
  subsequent valid save recovers. A malformed selection file preserves the
  working Q-to-second-candidate binding while an independently valid page-size
  change still applies. A deferred refresh granted after focus moves does not
  change either document or restore the old candidate UI. Finally, the service is
  deactivated and reactivated on the same context before granting the old edit:
  old requests cannot replay text/candidates into the new activation, old
  composition cleanup finishes, and exactly one new refresh scheduler remains.
  All cases pass alongside the existing generation, journal, mode and language-bar
  checks. No production fix was needed for these cases; the tests verify the
  previously implemented retry, last-good-value and context-identity guards.
  `build/tsf-private-schema-activation-generation-validation.json` now records
  `live_refresh_error_recovery`, `live_refresh_stale_focus` and
  `live_refresh_reactivation` separately. The tested DLL is unchanged from the
  implementation entry below; the report records the newly rebuilt host hash.
  Root removal/recreation, multiple live TSF readers and physical application
  acceptance remain pending; subsequent background-focus checks are recorded above.

- Wired directory invalidation into the ARM64 TSF service. A message-only callback
  timer polls the notification handle every 250 ms on the owning TSF thread;
  a five-second reconciliation/reopen path retries missed notifications and
  unavailable roots/files. No resident Core or per-key IPC was added. Secure
  activation does not create this user-data watcher. Deactivation closes its
  timer/window and notification handle. Settings, selection bindings, user
  journals, selected schemas and same-name dictionary generations now refresh
  without a focus transition. The settings page reflects automatic updating.
  `Engine::refreshConfiguration` skips unchanged effective settings; lexicon
  overlay/style equality avoids rebuilding candidate UI for duplicate or
  unrelated notifications and periodic reconciliation. Invalid selection files
  retain the last good dispatch map. Live refresh does not call `focusChanged`.
  Key preview/dispatch reconcile with the latest loaded state. Composition edits
  use the owning context's TSF write session and construct the engine snapshot
  at grant time, with active/focus/context checks rather than replaying an old
  captured engine. Reentrant refresh during key handling/text editing is deferred.
  The rebuilt package and expanded private generation integration pass with
  34 explicit key callbacks: page-size change while composing, live Q selection,
  held Shift preserved through refresh, no page/UI reset across a 5.5-second
  unchanged reconciliation, deferred edits using newer settings, a separate
  native writer updating active candidates, schema switching with raw-code
  retention, and same-name generation remapping without a focus event. Scheduler
  creation/deactivation checks and prior mode/language-bar checks also pass.
  Current DLL SHA-256 is
  `9ef9fb703fede347b2875113705d02630e6dbfc442ea30cea000c485a394e809`;
  evidence is `build/tsf-private-schema-activation-generation-validation.json`.
  Installed generation remains `19466b1647bcaa2b`.
  The current four-process active probe also passes: every process shares all
  10,229 dictionary pages; after 20,000 commits private usage is
  4,149,248–4,358,144 bytes. Warm-code tap medians are 11.4–11.7 microseconds,
  p99 37.9–43.2 microseconds, maximum 1.320 ms. This benchmark does not pump the
  normal message loop during its timed key sequence, so it does not measure the
  reconciliation work or native rendering. Current hashes/samples are in
  `build/tsf-memory-active-arm64.json`; the preceding history-chunk-only run is
  preserved as `build/tsf-memory-active-after-history-chunks.json`.
  These are explicit callbacks and message pumping in a private TSF host, not
  physical keyboard/native-rendering or general application acceptance. Dedicated
  live-refresh root-removal/recreation, multi-reader live propagation and
  reconciliation-cost measurements remain. Subsequent denied/stale-focus and
  reactivation checks are recorded above.

- Added the native `DirectoryChanges` invalidation primitive for the remaining
  live-settings/schema/journal synchronization work. It owns a recursive Windows
  directory notification handle, polls without blocking, rearms before the caller
  reloads authoritative files, and closes failed handles. It creates no worker
  thread and makes no cross-apartment TSF calls. See Microsoft's
  [directory notification contract](https://learn.microsoft.com/en-us/windows/win32/fileio/obtaining-directory-change-notifications).
  The ARM64 probe and `tests/directory_changes_test.py` verify two independent
  watcher processes receive all ten mutation stages from separate native writer
  processes, while a third watcher on another root remains unchanged. Stages
  include atomic configuration replacement, nested generation publication,
  existing-journal append, same-size selection-file rewrite, deletion, bursts and
  directory rename. Repeated rearming, quiet behavior, invalid roots, shutdown and
  100 open/close cycles without handle growth also pass. Evidence is
  `build/directory-changes-arm64.json`.
  At this checkpoint the primitive was not yet wired into the service; the later
  integration is recorded above. Its requirements are to schedule work on the owning TSF thread, preserve active code
  and held-key state, guard deferred edits against stale contexts, and handle root
  removal/recreation and retry after unavailable files. A notification signifies
  invalidation; it does not guarantee that a multi-file update has finished.

- Compacted full-session history into persistent chunks of 64 grapheme entries.
  Previews share existing chunks; editing a shared partial chunk copies that
  chunk. No history is truncated. Both Linux and ARM64 pass 633 original commit
  cases / 4,919 actions, full 100,000-element backspace and snapshot checks, and
  branch edits around chunk boundaries. Rebuilt engine probes each match the
  original over 17,220 key events in Chinese/English profiles. Reports are
  `build/history-parity-{linux,arm64}.json` and
  `build/history-key-parity-{linux,arm64}.json`.
  The rebuilt ARM64 package and private generation/mode integration pass.
  Tested DLL SHA-256 is
  `c541802b396aa64d57efcf5a371f87bd6e933f08da46b89730702137d977a3a1`;
  installed registration still points to generation `19466b1647bcaa2b`.
  A fresh active-memory run after parity work finished measures 4,210,688–4,222,976
  private bytes after 20,000 commits, versus 5,263,360–5,332,992 before chunking.
  Warmup-to-20,000 growth falls from 1,601,536–1,667,072 to 544,768–548,864 bytes.
  These process samples include allocator behavior and are not a measurement of
  history storage alone. All 10,229 dictionary pages remain multiply shared in
  all four live processes. Tap medians are 10.5–10.8 microseconds, p99 30.9–54.5
  microseconds, maximum 1.809 ms; no general latency improvement is claimed.
  Full history still grows with session length. Physical input, native rendering
  and general application performance remain unverified. These measurements
  are preserved in `build/tsf-memory-active-after-history-chunks.json`; the pre-change baseline is preserved
  in `build/tsf-memory-active-before-history-chunks.json`.

- Added an active TSF workload to the four-process memory probe: actual native TIP
  instances, real text stores, explicit key preview/dispatch/release callbacks and
  UI-less candidate interfaces. Each process warms with 100 ab+Space commits,
  measures 15,000 taps over 5,000 commits, then continues to 20,000 commits. Exact
  output and balanced candidate begin/end calls are checked. Latest tap medians
  are 10.6–10.8 microseconds, p99 29.1–60.4 microseconds; worst samples reach about
  2.54 ms. This repeated warm-code workload excludes physical keyboard delivery,
  normal message scheduling, native candidate rendering and general application
  latency. Timing workloads run sequentially while prior processes remain alive.
  All 10,229 dictionary pages remain multiply shared in all four processes.
  Private usage continues growing from about 3.66 MB after warmup to 5.26–5.33 MB
  after 20,000 commits, so bounded active-memory behavior is NOT established.
  Source inspection confirms History.h retains the full persistent history chain;
  recent(20) only limits its presentation, not storage. That storage contributes
  to growth and needs a more compact representation without truncating original
  backspace/history semantics. These pre-chunk measurements and hashes are now
  preserved in `build/tsf-memory-active-before-history-chunks.json`. Its passing status denotes correctness/shared-page
  checks, not completed performance or memory-stability acceptance.

- Measured four simultaneously live, privately activated ARM64 TSF services,
  rather than only standalone dictionary readers. Each verified the tested DLL
  and mapped the same bundled dictionary, touched every page, then remained alive
  until all measurements completed. All 10,229 dictionary pages in every process
  were resident/shareable and had ShareCount greater than one. Total process
  private usage was 2,961,408–2,973,696 bytes (about 2.82–2.84 MiB). The increment
  over the pre-profile-activation host baseline was 268–272 KiB, including a
  194,208-byte page-query vector; no exact allocator-overhead subtraction is claimed.
  `build/tsf-memory-arm64.json` records all four samples and DLL/host hashes.
  This is idle activation after page touching, with independent disposable user
  roots and no keyboard input or candidate rendering. Working-set figures include
  shared pages and must not be summed as unique physical memory. It does not
  establish active-typing, font/GDI system-memory or latency acceptance. The
  expanded TSF host builds, the four-process measurement passes, and existing
  private generation/mode integration checks still pass.

- Fixed silent interactive manager startup failures. Normal manager/direct-settings
  launches now show an explicit startup error dialog with the underlying failure;
  automation modes retain noninteractive failure exits. The isolated-desktop launch
  test now supplies an unreadable configuration fixture (a directory in place of
  config.txt), verifies the exact error-window title, closes it, checks exit code 1
  and confirms the fixture is unchanged. Both normal launch paths still exit 0
  after cancellation. ARM64 build, launch, manager and settings/engine-dispatch
  regressions pass. Evidence is in `build/manager-launch-arm64.json` and the existing
  manager/settings reports. This verifies window creation and failure behavior,
  not physical desktop rendering; installed files and user data are unchanged.

- Added actual manager CLI launch tests on disposable, inactive Windows desktops.
  Both normal launch and `--settings` create the expected visible-style window,
  reach input idle, close on WM_CLOSE and exit successfully. Closing either leaves
  the isolated user configuration unchanged. The test never switches the input
  desktop and only enumerates/posts to the child process it created. Evidence and
  staged executable hash are in `build/manager-launch-arm64.json`.
  Off-desktop capture experiments were unreliable (partially missing controls or
  blank images), so capture was removed from the passing launch test and the
  diagnostic images renamed `invalid-*-capture.png`. They are not visual evidence.
  This verifies actual utility startup/close behavior, not the language-bar popup
  launch chain, physical clicks, foreground restoration or rendered appearance.
  No installed configuration or registration was modified.

- Added language-bar management menu entries for 方案管理 and 输入设置. The latter
  launches the same native manager with `--settings`, opening its settings dialog
  directly using the normal per-user root resolution. Child creation uses an
  explicit DLL-adjacent executable path, no shell and no inherited handles.
  Secure-mode and detached buttons refuse management actions. A short-lived popup
  owner is used for right-click menu tracking. Real TSF item-interface tests verify
  both menu labels/IDs, unknown-ID rejection and detached-action rejection; existing
  mode/selection/settings checks remain green. ARM64 package build passes.
  Physical popup interaction, focus restoration, actual launcher success/failure
  and the direct-settings CLI launch still require runtime acceptance. No manager
  process was launched from the menu during these tests, and installed registration
  or daily input-method data were not changed.

- Extended settings acceptance from file inspection to actual ARM64 engine
  dispatch using files produced by the native controls. With the real dictionary,
  Q selects the first candidate for ab, a cleared second-candidate binding no
  longer selects that candidate, and the Restore Defaults button followed by
  Save reinstates numeric second-candidate selection. The UI-saved Ctrl+Alt+K
  shortcut also produces the engine's add-word action. Tests use the saved default
  English configuration and exercise Shift to enter Chinese before selection.
  The defaults restoration runs in a separate disposable selection file. The
  settings report records both manager and engine executable hashes and explicit
  dispatch flags (`build/input-settings-arm64.json`). ARM64 build and expanded
  settings tests pass. This proves control-to-file-to-engine behavior, not physical
  recording, actual TSF application dispatch or rendered UI acceptance. No live
  settings or installed registration were changed.

- Added an independent native 选重键 editor, opened from the shortcuts settings
  page. Each of ten candidate rows supports recording additional single keys,
  clearing bindings and restoring defaults. Existing integer bindings remain
  round-trippable; duplicate cross-row bindings retain original first-row
  precedence. Recorded key repeats/releases are consumed to avoid accidentally
  activating dialog actions after capture. Saving uses the stable configuration
  lock and changes only edited candidate rows in the latest selection-key file,
  preserving comments and other rows. Hidden-control tests record Q, clear the
  second candidate, preserve an intervening F9 change to the ninth candidate,
  preserve a non-VK value/comment, and cancel after a distinct R edit. Layout
  bounds pass at 96/144/192 DPI. ARM64 build, settings tests, manager regression
  and private TSF generation/mode activation tests pass. Physical recording,
  restored-default persistence and real application dispatch of the UI-saved
  selection file still need acceptance. No installed user data or registration
  changes were performed.

- Added an 操作快捷键 settings page with native hotkey-entry controls and enable
  switches for manual add-word and recent-schema switching. The saved shortcuts
  reuse the existing engine parser; duplicate, unsupported and reserved bindings
  fail before configuration replacement. ConfigStore now permits validation of
  the merged latest configuration while holding its existing stable lock. The
  form displays the failure reason and retains edits for correction. Hidden
  control tests pass Ctrl+Alt+K/J save, duplicate rejection, bare-key and reserved
  Ctrl+Space rejection, cancellation after shortcut edits, and the existing
  appearance/input/concurrency/layout checks (`build/input-settings-arm64.json`).
  ARM64 package build and schema-manager regression tests pass. Physical hotkey
  recording and application dispatch with UI-saved shortcuts remain pending;
  configurable selection-key editing is still separate unfinished work. Installed
  configuration and registration remain unchanged.

- Added all nine existing candidate themes to the native appearance settings
  dropdown. The renderer and manager now share the theme-name array. An existing
  unknown name is retained as a selectable current value rather than silently
  replaced. Hidden-control tests verify all theme choices and saved 清晨 selection,
  while preserving the prior settings validation/cancel/concurrency/DPI checks.
  ARM64 package and theme-probe builds pass. Original-source theme parity still
  passes all nine palettes, unknown-name fallback and 60 compositing cases.
  Settings evidence is in `build/input-settings-arm64.json`; renderer evidence is
  in `build/theme-parity-arm64.json`. Physical theme selection and visible rendering
  in applications remain pending. No installation or live settings were changed.

- Split the native settings window into input-behavior and candidate-appearance
  pages. Appearance now edits font name (with a readable bundled-font choice),
  decimal font size in the existing 3–200 range, vertical layout, candidate
  numbering, code display and candidate visibility. Save uses the existing style
  parser's configuration names and only edits changed effective values. Initial
  size formatting preserves double precision; blank font names are rejected.
  ARM64 build and expanded real hidden-control tests pass page switching, saved
  font/17.5-point size/layout/code values, invalid 201/NaN/2.5 sizes, cancel after
  font edits, unrelated-field preservation and 96/144/192-DPI control bounds
  (`build/input-settings-arm64.json`). Candidate rendering with the newly selected
  font, visual layout, actual monitor transitions, theme and shortcut editors
  remain pending. No installation or live-user configuration changes were made.

- Added a native input-settings dropdown for all four existing paging modes:
  minus/equal, brackets, Shift+Tab/Tab and PageUp/PageDown. The selected value is
  saved only if changed and uses the existing engine configuration strings.
  Expanded hidden-control checks verify all four choices against the engine
  parser, saved PageUp/PageDown selection, cancellation, invalid numeric input,
  concurrent-field preservation and bounds at 96/144/192 DPI. ARM64 package build
  and `build/input-settings-arm64.json` pass. This adds a settings entry point;
  it does not replace existing key-engine parity evidence or establish physical
  dropdown/mouse interaction. Installed binaries/configuration are unchanged.

- Added an “输入设置…” entry to the native schema manager and a modal Win32
  settings window for 15 existing engine boolean options, maximum code length
  and candidate page size. Controls load effective values through the engine's
  existing parser. Save validates integer ranges before mutation and applies only
  changed fields to the latest locked configuration; cancel makes no mutation.
  Concurrent edits to other fields, schema selection, comments and unknown options
  are preserved. The window explains the current focus-based refresh behavior and
  the activation-time default-language setting. `build/input-settings-arm64.json`
  verifies actual hidden controls, invalid-range rejection, successful save,
  cancel preservation, an intervening unedited-field change, and control bounds
  at 96/144/192 DPI. ARM64 package build and existing native schema-manager tests
  pass. These checks do not establish visible layout or physical monitor behavior.
  Font/style controls, shortcut and selection-key editors, complete settings
  coverage and immediate cross-process notification remain pending. No installed
  configuration, daily TigerClaw data or registration was modified by these tests.

- Expanded pending mode-edit tests across document focus changes, thread-focus
  loss and service deactivation. The thread-focus test initially failed: the
  context pointer stayed valid and the queued mode change still executed after
  OnKillThreadFocus. Both thread-focus loss and keystroke foreground loss now
  invalidate pending mode revisions. Tests verify that the old request changes
  neither document nor restored mode; deactivation followed by granting the
  pending lock leaves raw text intact and no composition active. Latest ARM64
  package build and private generation host tests pass, with five explicitly
  invoked key callbacks and separate result flags for these races. This remains
  callback-driven testing. A fresh foreground probe still returned HWND 0/Idle,
  so actual foreground keyboard/mouse acceptance remains unavailable. Registration
  and the daily TigerClaw installation are unchanged.

- Fixed mode-state recovery when the application rejects or defers a TSF edit
  lock. The real text-store fixture exposed that writing back the open/close
  compartment inside its own change callback fails. ModeCompartments now uses a
  thread-local message-only window to coalesce the desired state and retry after
  the callback; close destroys that window. The service republishes the actual
  engine mode after requesting an edit, so pending/refused requests do not leave
  an uncommitted English state displayed. Explicit same-value open/close requests
  can supersede an older pending edit; unrelated conversion-bit changes still do
  not notify the consumer. Expanded real-TSF tests inject a rejected host lock,
  a deferred request superseded by Chinese mode, and a deferred request that later
  gets its lock and commits raw code/ends composition. All pass with the latest
  ARM64 DLL and host hashes in the generation activation report. Component and
  existing language-bar tests pass too. Foreground/mouse validation, pending-lock
  focus/deactivation races and repeated retry failure remain separate pending
  coverage; no new registration or installation was performed.

- Added a native TSF language-bar button with Chinese/English text, tooltips and
  existing embedded mode icons. Left click writes the standard open/close
  compartment; engine publication updates the button. No focus, read-only or
  disabled contexts disable the button, and deactivation detaches its compartment
  and removes the item. The COM object separately retains the DLL lifetime for
  outstanding shell references. A native-specific item GUID avoids collisions
  with another input method's standard input-mode item in the thread manager.
  ARM64 package build and expanded private generation host checks pass, including
  item ownership, both text/icon states, left/right click behavior, no-focus
  disablement and a retained button reference after service deactivation.
  `build/tsf-private-schema-activation-generation-validation.json` records
  `language_bar_callbacks: true`; previous explicit mode/composition checks also
  still pass. Item absence is checked by the returned COM pointer because
  GetItem can return a successful non-S_OK result when absent. No foreign item is
  removed by the test. These are real TSF objects with explicit callback driving,
  not physical taskbar or mouse validation. Visible placement, theme/DPI rendering,
  accessibility and actual foreground transitions remain pending; the independent
  item does not establish Windows' reserved taskbar mode-icon behavior. Installed
  registration remains unchanged.

- Integrated standard TSF open/close and native-conversion mode synchronization
  into the ARM64 input service. Initialization follows the configured default;
  engine changes publish the focused context's mode, new contexts inherit the
  current mode, and existing contexts retain their mode. External mode changes
  use a TSF write edit session when a composition exists, preserving the engine's
  raw-code commit behavior. Deferred requests are invalidated on focus changes,
  newer mode requests and deactivation. Other conversion bits are preserved.
  `build/mode-compartments-arm64.json` verifies the real compartment component,
  own-write suppression, unsubscription and destruction during notification.
  ARM64 package build and private generation activation pass. The expanded
  `build/tsf-private-schema-activation-generation-validation.json` also verifies
  the actual service via explicit focus callbacks and four direct key callbacks:
  context mode restoration, raw-code preservation, composition termination,
  English pass-through and Shift publication. This is not physical keyboard or
  foreground-focus acceptance. Hidden-window system activation delivered no TIP
  focus callbacks; the direct-callback fixture obtains the TIP client ID through
  ITfClientId instead of using the text-store owner's client ID. Language-bar UI,
  real foreground switching, deferred-lock rejection/race coverage and complete
  original cross-application mode-policy parity remain pending. Registration is
  unchanged; the new DLL was only privately activated.

- Moved shortcut-namespace conflict detection before installation/rollback
  registration changes. Shared preflight is read-only and rejects unowned links
  and parent-file conflicts. Tests verify no directory creation or owned-link hash
  change; expanded shortcut tests and both CheckOnly paths pass. This does not
  establish failure-atomic installation for later I/O errors. No real installation,
  shortcut mutation or registration changes were performed.

- Added an installation-record-checked uninstall script with read-only preflight,
  exact native profile/path/hash checks, post-elevation registration recheck and
  post-unregister COM/language-profile checks. Owned shortcut removal is integrated;
  user data and binary generations are retained. Live preflight plus four invalid
  record cases pass with unchanged registration/profile plan
  (`build/uninstall-preflight-arm64.json`). No uninstall/elevation was executed.
  Actual unregister/reinstall, remaining category checks and binary cleanup remain
  pending; this does not complete uninstall acceptance.

- Added an installed Start Menu manager shortcut and shared ownership-checked
  shortcut lifecycle. Rollback retargets to a manager-capable generation or removes
  only an owned link. Actual shell-link tests pass create/update/remove and preserve
  unowned links/unrelated files in an isolated Programs directory
  (`build/shortcut-arm64.json`). Install/rollback CheckOnly pass; installer preflight
  includes the planned shortcut path. Real Start Menu and registration are unchanged.
  Independent uninstall is still absent; interactive install/launch/rollback checks
  remain pending.

- Integrated all three management executables into ARM64 build staging and immutable
  installer artifacts/hash identity. Preflight now checks ARM64 PE headers for each
  executable. Default manager startup resolves sibling dictionary/tools and the
  standard user root/override. Read-only package tests pass missing-tool and x64
  rejection plus actual hidden-window default-path selection in an isolated root
  (`build/native-package-preflight.json`). Manager regressions pass. No installation
  or registration was performed; start-menu/language-bar entry and interactive
  installation/visual checks remain pending.

- Added per-monitor-v2 DPI handling and scaled font/control layout to the manager.
  Actual bounds/font checks pass at 96/144/192 content DPI, followed by all existing
  manager operation tests. Corrected frame sizing to use the window's actual DPI
  during simulated content scaling. Report: `build/schema-manager-arm64.json`.
  Physical monitor transitions and visible text/layout inspection remain unverified;
  folder-picker interaction and install/launch integration remain pending.

- Improved manager version choices with local timestamps, short identifiers,
  current-version markers and a Chinese label for the initial import, sorted newest
  first. Display labels bind separately to full generation IDs. Real-window tests
  make two updates and correctly restore both the initial and newest versions;
  prior GUI/error/close checks pass (`build/schema-manager-arm64.json`). Visual/DPI
  checks and install/launch integration remain pending; installed runtime unchanged.

- Expanded real Win32 manager event-flow validation: Import creates the scheme,
  Update retains its original binary, Versions populates the combo and Restore
  consumes that selection. Global schema selection stays unchanged for these
  operations. A close request during Use waits for worker completion and the
  selected configuration is committed. All previous GUI error/path tests still
  pass (`build/schema-manager-arm64.json`). Thread-start failure now restores
  enabled controls (inspection fix; no allocation fault injection). Visible layout,
  folder-picker interaction, DPI and install/launch integration remain pending.

- Added a Win32 schema manager with selection, folder pickers, import/update and
  version/restore actions wired to companion native tools. Background execution
  captures results without blocking the window; busy controls disable and close
  waits for completion. Hidden-window tests pass actual Use-command dispatch,
  worker completion/config publication, Unicode/space paths, corrupt-target
  preservation and missing-tool errors (`build/schema-manager-arm64.json`).
  Interactive visual/DPI/folder-picker checks, other button flows, closing during
  work and install/launch integration remain pending. Tests use isolated roots.

- Added validated retained-version listing and restore commands to `schema_select`.
  Restore can repair malformed descriptor bytes and choose a retained generation or
  explicit `legacy`, after mapping/journal validation; schema selection/MRU stay
  unchanged. Two restores, two update cycles and four failed-restore preservation
  checks pass, plus selector regression and actual DLL generation activation.
  See `build/schema-generation-arm64.json` and `SCHEMAS.md`. Active-composition
  restoration, GUI recovery and unselected-file cleanup remain pending.

- Added live ARM64 generation stress: four processes retain original mappings while
  six concurrent importers publish updates. All 1,118 reads preserve old candidates
  and see only complete expected new generations; readers converge on one selected
  version. Actual TSF DLL activation also maps the descriptor-selected generation
  while the legacy file exists. Reports: `schema-generation-concurrency-arm64.json`
  and `tsf-private-schema-activation-generation-validation.json` under `build`.
  The first activation fixture had mixed path separators; correcting its expected
  Windows path resolved the false mismatch. Registration remains unchanged. HWND 0
  still prevents active-input validation; this does not establish composition
  refresh, full-host memory or typing latency.
- Added immutable named-schema generation updates (`lexicon_import --update`). New
  binaries retain prior generations; existing journals are validated before atomic
  `current.txt` publication. Catalog/selector/TSF resolve descriptors with legacy
  fallback only when absent; same-name TSF reload checks mapping identity. Two
  update cycles, retained old files, selector loading, bad-journal rollback and four
  descriptor failures pass (`build/schema-generation-arm64.json`), along with import
  and selector regressions. ARM64 DLL private activation passes (zero key events),
  registration unchanged. Live-reader/concurrent-update stress and actual TSF
  composition across same-name updates remain pending; importer UI/recovery remain open.
- Named import now reuses an empty leftover ordinary directory on retry, preserving
  its canonical Unicode spelling. Expanded publication tests recover and select such
  a scheme, while preserving user-journal and interrupted-temporary-file fixtures
  byte-for-byte when nonempty retries are rejected. Selected configuration remains
  unchanged on failure, and previous import/concurrency/binary regressions pass.
  Evidence: `build/lexicon-publish-arm64.json`. Reparse points are rejected by code;
  nonempty crash-artifact recovery and injected-failure testing remain open.
- Added `lexicon_import --schema` to publish directly into a named user schema.
  Imported schemes pass actual catalog listing and selector validation/configuration
  publication. Shared name validation rejects builtin/invalid/existing names while
  preserving selection and binary; a stable schema-import lock ensures concurrent
  Unicode-equivalent names create only one scheme. Expanded publish tests pass,
  including all prior full-binary and no-replacement checks. Same-schema generation
  updates, failed-import directory recovery and importer UI remain open. Tests use
  a disposable user root; installed registration and daily runtime are unchanged.
- Added the short-lived ARM64 `Tigirl.Import.exe`, connecting actual source files
  through all six maps to validated immutable v2 publication. The real tiger data
  reproduces the original 41,897,112-byte export byte-for-byte. Publication writes/
  flushes a unique temporary file, validates with the production reader, then moves
  without replacement. Four concurrent publishers yield one success; existing output
  preservation, temporary cleanup, empty-main rejection and missing optional pinyin
  pass (`build/lexicon-publish-arm64.json`). See `native/IMPORT.md` for invocation.
  Packaged importer/UI and schema generation selection/update integration remain;
  disk fault/power-loss injection is not covered. Installed runtime is unchanged.
- Added full six-section directory import, including pinyin stable merging and
  comment/split loading in native enumeration order. Seventeen complete snapshot
  comparisons pass against original Core, including actual main/auxiliary files,
  multi-file annotation/overwrite cases and suffix-only filenames. All six maps,
  candidate order and main flags agree (`build/lexicon-all-directory-parity-arm64.json`).
  Complete snapshot-to-binary integration, immutable publication and importer UI
  remain open. The missing optional pinyin branch lacks a separate integration
  case. No installed runtime changes were made.
- Connected ordered main-directory loading to file decoding/parsing and snapshot
  construction. Dedicated construction input, uncoded inference, sentence-file
  exclusion and ordinary rows/directives in the adjustment file preserve original
  sequencing. Seventeen full main-snapshot comparisons pass against original Core,
  including the actual tiger schema and four-culture multi-file fixtures: 171,205
  main / 172,136 indexed entries, zero differences in ordering/maps/flags. Report:
  `build/lexicon-directory-parity-arm64.json`. Auxiliary directory loading, binary
  publication and importer UI remain unfinished; installed runtime is unchanged.
- Added Windows main-table directory ordering with TXT-then-YAML enumeration,
  schema-name priority and stable ICU current-culture collation. Forty actual
  directory comparisons pass against original `GetOrderedLexiconFiles` across
  eight cultures, including real staging names, collation ties and Unicode names.
  Missing-directory rejection passes; original source hashes are unchanged.
  Evidence: `build/lexicon-order-parity-arm64.json`. The caller supplies culture;
  ordered paths still need connecting to snapshot construction and auxiliary-file
  enumeration. This does not complete the importer or change installed TSF.
- Connected actual native file reading to byte decoding and row parsing through
  `readLexiconText` and `parseLexiconFile`. Unicode paths and case-insensitive YAML
  extension detection work. The decoder passes all 2,292 cases through disk files,
  plus missing-file/directory rejection; the row parser passes 1,234 disk fixtures
  across five encodings against original results (253,280 coded / 198 uncoded rows).
  Updated decode/rows ARM64 reports record current hashes and actual-file coverage.
  Directory enumeration/culture ordering, generation publication and importer UI
  remain pending. Mid-read device faults and concurrent source changes are not yet
  verified. Installed runtime remains unchanged.
- Added native raw-byte table decoding, preserving the original .NET 10 BOM and
  malformed-sequence replacement behavior, including UTF-16 precedence over the
  apparent UTF-32 LE BOM. ARM64 passes 2,292 exact UTF-16 comparisons against the
  original encoding detector and .NET file reader: random/damaged input, buffer
  boundaries and all nine actual main/auxiliary files. Report:
  `build/lexicon-decode-parity-arm64.json`. All 34 reference sources remain unchanged.
  This is the decoded-byte component; ordered filesystem loading, generation
  publication and complete importer integration/UI remain open. Installed TSF
  registration and daily TigerClaw were not changed.
- Added native six-section v2 serialization for the import utility. The ARM64
  writer reproduces the entire 41,897,112-byte existing export byte-for-byte, plus
  independent boundary/minimal fixtures; mapped-reader round trips compare every
  record/value. Sorting, interning, raw UTF-16 preservation and five invalid-input
  categories pass (`build/lexicon-serialize-arm64.json`). Serialization returns bytes
  only. File decoding/order, immutable publication, full pipeline integration and
  importer UI/package remain pending. No installed runtime was changed.
- Auxiliary content import passes 181 comparisons against original pinyin/comment/
  split loaders, including all four actual files, with zero differences. Annotation
  escaping stays separate from alias parsing; comment concatenation and split
  replacement preserve supplied file order. Shared escape-decoder regression passes
  all 633 add-word cases. Reports match current ARM64 probe hashes. Rebuilt DLL
  alternate-schema private activation passes with zero keyboard events and unchanged
  installed registration. See `native/IMPORT.md` and auxiliary-import/add-word reports.
- Import-time user adjustments and main-index metadata are implemented. Replay
  preserves commit-identity/alias behavior, one-step advance, all-match deletion
  and empty source keys; construction lookup remains pre-adjustment and full codes
  are rebuilt afterward. Derived prefix/unique/auto-symbol/exact-source flags and
  quick-symbol bits match the original snapshot/export semantics. The expanded
  ARM64 pipeline check passes 364 cases, including 152 adjustment/metadata fixtures,
  with 187,734 indexed records and zero differences (`build/construct-import-parity-arm64.json`).
  Main/construct/full maps also agree; real table row volume is included. This does
  not prove filesystem precedence or a finished importer. Remaining import stages
  are decoding/file order, binary writing/publication
  and packaged UI. No TSF DLL or registration changes were required in this step.
- Import construction lookup, uncoded-word inference and full-code maps now pass
  212 original snapshot comparisons, including all five real main-table files'
  row volume. Totals: 177,572 main, 106,092 construction and 240,745 full-code entries,
  with exact candidate/insertion order and zero code-unit differences. Dedicated
  construction overrides, rank/tie rules, aliases, graphemes and short-code failures
  are covered. The shared add-word construction routine now accepts a lookup callback;
  its existing mapped-dictionary API still passes 633 original cases. Report:
  `build/construct-import-parity-arm64.json`. Reference serialization was strengthened
  to preserve unpaired UTF-16 units rather than JSON replacement characters. ARM64
  DLL build and alternate-schema private activation pass. See `native/IMPORT.md`.
  File decoding/precedence, adjustment replay, other auxiliary maps/metadata, binary
  publication and importer UI/package are still pending; no complete-import claim.
- Added import-time stable frequency merging and exact packed-candidate deduplication,
  preserving original candidate order and code-group insertion order. The actual
  original snapshot builder matches 106 all-coded fixtures containing 25,517 rows,
  including a 20,000-row duplicate group (`build/lexicon-merge-parity-arm64.json`).
  The first run exposed Unicode identity mismatches with Windows ordinal comparison;
  shared .NET-compatible ICU ordinal casing now fixes final sigma/micro-sign grouping
  and is also used by schema catalog/MRU/service/selector paths. Unicode schema
  alias resolution and MRU deduplication pass, as do selector/concurrent-recent/
  configuration regressions. ARM64 DLL build and alternate-schema private activation
  pass. See `native/IMPORT.md` for source attribution and exact scope. Import-time
  decoding, file ordering, construction/inference, adjustments, auxiliary maps and
  binary publication remain unfinished; no end-to-end importer or keyboard claim.
- Implemented the original import row parser in `native/LexiconImport.*`, including
  YAML body boundaries, code-first versus tab/word-first rules, signed frequency
  handling, Unicode trimming, escaped comments, aliases and adjustment-line skips.
  `tests/lexicon_rows_parity.py` compares directly to the unchanged original
  `ParseMbFile`: ARM64 passes 1,234 cases including all five real main-table files,
  with 253,280 coded and 198 uncoded rows and zero differences. Evidence:
  `build/lexicon-rows-parity-arm64.json`; all 34 upstream source hashes unchanged.
  This is an import-time component, not a finished importer or a TSF code path.
  See `native/IMPORT.md` for remaining decoding/sorting/inference/adjustment/maps/
  binary-publication stages. The native schema selector still needs prepared v2
  dictionaries. No registration or daily-runtime writes occurred.
- Added the prepared-schema catalog and recent-target fallback, and connected
  configurable recent-schema shortcuts to a locked target-selection/publication
  transaction in real TSF dispatch. Preview reads only; unavailable/failing actions
  fall through without latching. Engine state preserves composition and suppresses
  schema/add-word rollover until modifier release, matching the inspected original
  control flow. Shortcut conflict handling disables both ambiguous actions.
  Linux/ARM64 shortcut state checks, ARM64 catalog/16 concurrent selector commands,
  corrupt-target preservation, existing config transaction tests, Linux 35,554
  original ordinary-key events and ARM64 20,460 add-word events pass. DLL and test
  builds and alternate-schema private activation pass. See `SCHEMAS.md` and
  `build/schema-shortcut-{linux,arm64}.json`, `build/schema-recent-arm64.json`.
  The full TSF fixture now includes recent-shortcut preview/repeat/composition
  checks, but foreground inspection still reports HWND 0; no full-input success
  is claimed. UI, original-format import and end-to-end validation remain open.
- Added a short-lived ARM64 native schema selector (`tools/SchemaSelect.vcxproj`),
  which validates the target mapping and journal, canonicalizes its directory name
  and atomically writes current-schema/recent-pair metadata before exiting.
  `selectSchemaConfiguration` shares the existing locked file replacement path
  with candidate visibility changes. Five ordered selections, 124 concurrent
  schema writes plus 34 visibility toggles and 2,000 coherent reader snapshots
  pass; blocked publication preserves the original. The executable also passes
  three real selections and four invalid/corrupt-target rejections with unchanged
  configuration. Existing configuration encoding/concurrency/failure checks pass.
  Reports: `build/schema-config-arm64.json`, `build/schema-selector-arm64.json`,
  `build/config-store-arm64.json`. ARM64 DLL builds. This utility is not yet
  packaged, and schema UI, recent shortcut and original-format import remain open.
- TSF now reads `当前码表` and opens prepared alternate schema binaries from
  `schemas/<name>/tiger-v2.tcd`, with a separate `user.tcu` journal per schema.
  The bundled schema keeps its existing dictionary/journal paths. Target open
  and journal replay precede resource replacement; invalid or unavailable targets
  retain the current schema, and initial failure attempts the bundled fallback.
  Focus refresh switches each context's engine and uses a write edit session for
  surviving composition text. See `SCHEMAS.md` for the interim interface and limits.
  ARM64 build and private alternate-schema activation/mapped-path verification
  pass (`build/tsf-private-schema-activation-validation.json`, zero input events).
  The new full two-context fixture in `tests/run_schema_tsf.ps1` failed before its
  first key because OS foreground focus was unavailable; Windows foreground
  inspection returned HWND 0. This is not evidence of input-switching correctness.
  Full fixture validation, schema UI/recent shortcut/import and update lifecycle
  remain pending. No installation or daily-runtime changes were performed.
- Schema composition refresh now has an explicit `Engine::switchSchema` entry
  point. It preserves raw keys and input mode, resets the candidate page and
  mixed decoder cache, and reinterprets long input with the target maximum even
  when the target disables unlimited mixed input. History remains unchanged.
  `tests/switch_composition_parity.py` passes 11,040 cases on each of Linux and
  ARM64 against the original `RefreshCompositionAfterSchemaSwitch`: all two-letter
  codes plus long input, paging, pinyin and uppercase; both mixed settings and
  target maximum lengths 1/2/4/16. Reports:
  `build/switch-composition-parity-{linux,arm64}.json`.
  This differential check uses the same base dictionary with changed settings.
  A separate assertion in each probe replaces the immutable lexicon, verifies
  decoded prefixes and final commits use the new candidate, and verifies an
  existing preview retains its old composition. The ARM64 DLL builds successfully;
  all 34 original source hashes remain unchanged. Schema catalog/import, recent
  shortcut and TSF dispatch integration are still pending; this is engine-level
  evidence, not an end-to-end table-switching claim. No installation was performed.
- Immutable UTF-16 dictionary format and C++ read-only mapping with process-local
  reuse. The full index, not only raw text, lives in the mapping.
- Offline original-Core snapshot/export/trace oracle, source hashes preserved.
- Actual TigerClaw word table and pinyin table copied into `data/staging/`.
  This is an independent copy, not the reference checkout's daily runtime.
- Exhaustive reader parity: 757,399 records, 863,489 values across all six
  sections, including candidate order and flags. Result:
  `build/dictionary-parity.json`.
- Malformed file tests: 18 bad headers/offsets/counts/lengths/truncations rejected
  with a controlled error, without a crash.
- Windows ARM64 four-process sharing measurement:
  41,897,112 mapped bytes; all 10,229 pages were resident and multiply shared in
  every reader; process private bytes were 970,752–978,944. This measures the
  reader executable, not a complete TSF host/UI. Result:
  `build/dictionary-memory-arm64.json`.
- Initial native ordinary key engine and 33,424 differential events with zero
  mismatches against original Core: handled/cancel flags, Chinese mode,
  composition state/raw code, page, candidate display/order and annotations.
  Tests use 250 deterministic real-table samples with selection, overflow,
  punctuation and editing endings, plus pinyin, quick symbols, uppercase,
  Shift, Ctrl+Space and shortcut cancellation. Result: `build/key-parity.json`.
  This does NOT prove full operation/configuration parity.
- The same 33,424 trace events also pass in the native Windows ARM64 executable,
  and all 34 upstream snapshot hashes match. Result: `build/key-parity-arm64.json`.
  Pass long traces via a Windows-local file argument; a large WSL stdin pipe
  stalled in the test harness. File-based input completed normally.

## Remaining before completion

- Verify native add-word UI through actual TSF/application typing; complete
  shared history, notifications and journal compaction lifecycle.
- General native configuration loading and settings/table-switch UI; custom
  selection-key TSF host validation, action shortcuts, one-shot/repeat handling.
- Manual timer behavior and full Unicode text-element history semantics.
- Ordinary mixed-input compatibility and more exhaustive modifier/config tests.
- Broader TSF edit-session lifetime/reentrancy/focus validation and configurable
  candidate layout/font options; input-mode compartments and language bar.
- Import/settings utility, uninstall and update lifecycle beyond versioned install.
- Real application typing checks and complete engine/UI memory/latency evidence.

## User-word implementation update

- Dictionary v2 (`data/tiger-v2.tcd`) adds an exact-source-key bit so empty
  original entries differ from synthetic prefixes. The original v1 generation
  remains intact but is deliberately rejected by the current reader; re-export
  is required. All 757,399 v2 records match the original Core export, and all
  18 malformed dictionary cases still fail cleanly. Evidence:
  `build/dictionary-parity-v2.json`, `build/dictionary-corruption-v2.json`.
- `Lexicon` is an immutable overlay of edited codes on the shared mapping.
  Adds replace matching commit identities, while top/advance retain stored
  aliases. Empty exact keys preserve the original short-symbol behavior.
  Unchanged table records are not copied. Source-key inventory is scanned only
  for the first edit in a snapshot chain, then updated incrementally.
- Engine lookup now uses that snapshot. Ctrl+number top,
  Ctrl+Shift+number delete, Alt+number advance, page-relative indexes and held-key
  one-shot handling are implemented. Copied-engine previews cannot persist data;
  actual TSF dispatch drains explicit user changes and persists them through the store.
- Expanded key parity passes 35,554 events on Linux and Windows ARM64, including
  2,130 adjustment events. Evidence: `build/key-parity.json`,
  `build/key-parity-arm64.json`. The mutation oracle now uses disposable filesystem
  copies as well as the existing HKCU registry sandbox.
- 27 direct add/delete/top/advance cases match original Core values and metadata,
  including new prefixes, deletion of all candidates, aliases and escaped text:
  `build/lexicon-parity.json`.
- `UserStore` implements a per-schema append journal, OS file locking, rebasing
  against concurrent writers, checksums, flushes and interrupted-tail recovery.
  Four independent writers retain all 48 edits after restart. Every one of 71
  interrupted final-record positions recovers and permits another append;
  complete-record checksum corruption is rejected without overwriting the file.
  Both Linux and native Windows ARM64 pass: `build/user-store-linux.json`,
  `build/user-store-arm64.json`.
- The v2 four-process Windows reader measurement again shares all 10,229 mapped
  pages, with private bytes 970,752–978,944. This remains a reader measurement,
  not a TSF or complete user-store memory benchmark.
- Details and adapter contract: `native/USERDATA.md`. The engine, journal and
  native candidate UI are connected to the installed TSF service.

## Native TSF integration update

- `native/tsf/Service.*` supplies the COM factory implementation. Each context
  owns its engine/composition. Consumed-key tests use copied engine state;
  actual dispatch writes through TSF edit sessions. Idle Enter does not insert
  a duplicate newline. Ended compositions cannot resurrect a stale engine buffer.
- `native/tsf/CandidateUI.*` supplies the UI-less candidate COM interfaces and
  a vertical native popup with annotations, caret positioning and mouse selection.
  A real Windows TSF host passed 36 events across two contexts, both with the host
  suppressing native UI and with native popup/mouse selection enabled.
  These are custom application text-store tests, not broad application coverage.
- Repeated native-window runs exposed intermittent failures. The fixture now
  uses separate HWND associations for its two documents and explicitly validates
  OS/TSF focus. Repeat stability remains under investigation.
- A deterministic missing-layout regression was added: typing `ab` while the
  text store returns TS_E_NOLAYOUT left the old `a` candidate count (2 instead
  of 4). Candidate model refresh is now independent of caret geometry; the popup
  hides until layout resumes. Build passes; installed regression validation is
  pending and must not be inferred from the earlier 36-event results. The UAC
  request for this fix was canceled; installed generation remains
  `19466b1647bcaa2b`. Do not repeat elevation without renewed user authorization.
- The current rebuilt ARM64 engine still matches all 35,554 previously
  oracle-verified normalized events (`build/key-parity-arm64-current.json`).
- `tests/run_tsf_host.ps1` captures both UI variants serially, with test/DLL hashes
  and failure logs. Its layout-recovery checks require the pending fixed build.
- Installation copies DLL/data into immutable hash-named directories under
  Program Files/Tigirl/versions and verifies registration and hashes.
  `rollback_arm64.ps1 -CheckOnly` validates the saved original baseline target;
  an actual rollback has not been performed. Daily TigerClaw remains separate.

## Custom selection configuration update

- `native/SelectionKeys.*` implements the original selection-label/key parser,
  last-row replacement, deduplication, lowest-candidate conflict precedence,
  default bindings, and canonical round trips. All 208 cases match the unchanged
  original parser on Linux and Windows ARM64 (`build/selection-parity-linux.json`
  and `build/selection-parity-arm64.json`).
- Twelve file-loading checks per platform cover a missing file, UTF-8 with and
  without BOM, both UTF-16 and UTF-32 byte orders, malformed data and preservation
  of input bytes. The loader has a 4 MiB bound and rejects malformed Unicode;
  the original .NET reader can substitute replacement characters.
- TSF source now loads `%LOCALAPPDATA%/Tigirl/自定义选重键.txt` at activation
  and document focus, using defaults for missing/invalid files. No file I/O occurs
  in key preview. This source builds for ARM64, but is not installed because the
  previous elevation was canceled. Custom key-trace parity now passes as detailed below; actual
  TSF configuration reload tests remain outstanding.
- Usage and exact semantics: `native/SELECTION.md`. General settings, schema
  switching and settings UI are still required. The oracle adds a selection
  inspection mode without changing its upstream snapshot.

## Custom selection key dispatch update

- Four custom binding profiles now exercise 17,696 additional original-Core
  differential events on Linux and Windows ARM64. Generic and sided modifiers,
  CapsLock, repeated key-downs, out-of-range candidate indexes, conflicts with
  editing keys, and pinyin reverse lookup are included. Evidence:
  `build/selection-key-parity-linux.json`, `build/selection-key-parity-arm64.json`.
- These checks exposed and fixed CapsLock missing from the engine's modifier
  classification and incorrect pinyin selection priority relative to Backspace,
  Enter, Tab and Escape. The unchanged original source remains the oracle.
- All 35,554 previously verified default-binding events still match after these
  changes (`build/key-default-regression.json`). ARM64 DLL build passes.
- These are native engine traces, not Windows key injection or installed TSF
  validation. The installed DLL remains the previous immutable generation;
  canceled elevation has not been repeated. Candidate missing-layout recovery,
  file reload and actual application checks on the newer build are still pending.

## Ordinary settings update

- `native/Settings.*` parses 17 original ordinary-engine settings. The TSF source
  loads the independent `%LOCALAPPDATA%/Tigirl/config.txt` at activation and
  document focus, then overlays the separate selection bindings. New contexts
  receive the configured default language; existing contexts retain their current
  language on reload. No per-key settings I/O was added.
- 224 actual original config reload/getter cases match native parsing on Linux
  and ARM64: boolean forms, duplicate/unknown keys, separators, integer overflow,
  bounds and page-key strings. Results: `build/settings-parity-linux.json` and
  `build/settings-parity-arm64.json`. This proves parser/getter parity, not full
  input-operation parity under every configuration.
- The Unicode file reader was extracted into `Text` and reused by both settings
  files. Original source remains unchanged. The ARM64 TSF DLL builds successfully;
  installation/real-host reload validation still await renewed elevation approval.
- Exact supported keys and defaults: `native/SETTINGS.md`. Candidate layout/font,
  schema switching, settings UI and the remaining ordinary options are still
  required; unsupported options are documented rather than presented as active.

## Non-default settings key traces

- Six ordinary settings profiles each pass 6,850 original-Core differential events
  on Linux and Windows ARM64 (41,100 per platform). Profiles cover default English,
  disabled toggles, punctuation, clear/auto-commit options, lengths 1/2/16, pages
  1/3/10, annotations and all four paging choices. Evidence:
  `build/settings-key-parity-linux.json`, `build/settings-key-parity-arm64.json`.
- The trace oracle now applies the configured default language after construction,
  as the original ProtocolHandler does. Previously the wrapper instantiated only
  InputMethodEngine, which starts Chinese regardless of the default-language
  setting. Upstream source remains unchanged (all 34 hashes verified).
- Cached settings trace replay checks input fingerprints to reject stale config,
  selection bindings or key events. Native EngineProbe can now take a config file.
- `data/native-config.example.txt` is an editable example of the 17 connected
  ordinary settings. This is not an installed settings UI. Actual TSF reload,
  pending DLL installation, candidate styling/schema lifecycle and the remaining
  ordinary features still require completion.

## Candidate presentation update

- Native source now reads horizontal/vertical mode, candidate indexes, code
  visibility, candidate hiding, font name and font size from the same settings
  file. Candidate text uses original index/annotation/escape conventions.
  GDI measurements produce both drawing and mouse hit-test rectangles.
- 80 text/visibility combinations match unchanged original Overlay formatting on
  Linux and ARM64 (`build/presentation-parity-linux.json`,
  `build/presentation-parity-arm64.json`). Three reference Overlay files have a
  separate hash manifest; the existing 34 upstream source files remain unchanged.
- Expanded settings/getter parity passes 285 cases on both platforms, including
  candidate display flags, font names and finite font-size bounds.
- Candidate layout builds for ARM64. It is not installed/render-verified: canceled
  elevation has not been repeated. The real TSF mouse fixture was updated to find
  the rendered highlight instead of assuming the old fixed row height/header.
- Installed font-family selection and bundled `#` font loading are implemented
  (see the update below); missing families use GDI fallback. Themes, delayed
  expansion, extreme-size clipping and full DPI behavior remain incomplete.
  Exact limits are in `native/CANDIDATES.md`; prior popup screenshots do not
  validate this new layout.

## Bundled font and package update

- The original local reference font is copied unchanged with provenance and OFL
  notice. ARM64 builds include its `字体` directory. `PrivateFonts` registers it
  privately for the host process, reuses shared leases across services, and removes
  it after the last candidate/service reference releases the resource.
- The ARM64 probe retrieves exactly all 26,234,636 original font bytes through GDI
  after selecting the Chinese family name, checks candidate glyphs, shared leases,
  final release and reload. Evidence: `build/private-font-arm64.json`.
  This does not establish cross-process font memory sharing or new TSF rendering.
- Installation now derives a generation from all five package artifacts, including
  font/license/provenance. Font-only updates no longer collide with an existing DLL
  generation. All artifact hashes pass `install_arm64.ps1 -CheckOnly` and an
  independent file comparison (`build/package-preflight.json`).
- Preflight requests no elevation and changes no registration. The pending package
  has not been installed; the previous canceled UAC request has not been repeated.
  Actual new-window rendering, themes, delayed expansion and broader app testing
  are still required. Details: `native/tsf/FONTS.md`.

## Candidate theme update

- Nine original named theme palettes, border widths/corners and unknown-name
  fallback are implemented. Native candidates now render to premultiplied BGRA
  and use a layered window, retaining background/selection transparency without
  reducing text opacity. GDI glyph coverage uses the existing font resources.
- Palette declarations match the original read-only Overlay source, and 60
  independent alpha-compositing samples pass on Linux and ARM64. The probe also
  checks premultiplied-channel bounds over every pixel. Evidence:
  `build/theme-parity-linux.json`, `build/theme-parity-arm64.json`.
- The ARM64 600x200 pure composition measurement averages about 0.50 ms over
  20 renders. This excludes text-mask creation, window updates and full typing
  latency. The renderer and updated TSF fixture compile successfully.
- Expanded configuration/getter parity passes 296 cases on Linux and ARM64,
  now including theme names. Full package preflight and artifact hashes pass.
- Installed rendering remains unverified; no canceled elevation has been repeated.
  Prior screenshots do not prove layered-window behavior. See `native/THEMES.md`
  for exact evidence and limits. Theme config is included in the editable example.

## Private TSF activation update

- `tests/run_tsf_host.ps1 -PrivateBuild` can select the build DLL through a
  process-local COM activation manifest. It does not modify installation or
  registration. The existing enabled profile supplies TSF profile metadata.
- ARM64 activation-only validation passes: actual Windows TSF startup, exactly
  the intended module generation, mapping of that generation's dictionary and
  teardown. The runner records matching registration paths before/after and
  explicitly reports zero key events. Evidence: `build/tsf-private-validation.json`.
- Full private UI-less runs stopped before event zero: GetForegroundWindow is
  null in the current Windows session. The test retains its OS/TSF focus checks.
  Input, missing-layout recovery and new layered candidate rendering are still
  unverified. No administrative installation was performed or retried.
- `-PrivateBuild -ActivationOnly` is a separate startup check, not a substitute
  for the full suite. The test manifest is kept out of the deployable package.

## Dynamic output update

- Native whole-entry date/time/weekday and random-set expansion now follows
  the original conversion rules; aliases keep their display labels. Copied
  engines own copied PRNG state, isolating previews from real dispatch.
- Both platforms pass 20 original-oracle token cases, 168 fixed-clock cases,
  64 random draws, 34 engine commit/alias/repeat/nested-token cases and 64
  engine preview cycles. Probe hashes accompany the dynamic test reports.
- Public candidate selection now runs output/history postprocessing, fixing
  a reproduced failure where repeating a mouse-selected dynamic candidate
  returned the old repeat buffer. Native engine coverage does not prove a real
  mouse event traversed the TSF adapter; that application check remains pending.
- Default 35,554-event regression replay passes on Linux and ARM64. ARM64 DLL,
  package preflight and private TSF activation pass for the dynamic build;
  the registered DLL remains the previous installed generation.
- See `native/DYNAMIC.md` for reproducible commands and nondeterministic limits.
  Manual timer, mixed input, settings/schema UI and
  full TSF/application lifecycle remain incomplete.

## Uppercase currency update

- `UppercaseText.cpp` and the uppercase key path now implement the original
  `S` currency conversion and conditional decimal/comma buffering. String-based
  decimal arithmetic preserves rounding and the 96-bit input range.
- Linux and ARM64 pass 1,853 original conversion/prefix cases and 4,574
  original key events. The default 35,554-event regression also passes on both.
  Test reports record probe hashes; see `native/UPPERCASE.md` for exact scope.
- The original's empty zero output and numeric-prefix quirks are preserved.
  Timer scheduling/reminders remain pending; these tests do not start timers.
  Real-application currency typing is still unverified.

## Native add-word update

- Added native manual-add UI, default Ctrl+=, configurable shortcuts and
  `{添加}`/`{加词}` dictionary actions. Preview engines only report an action;
  real dispatch posts a UI request, and actual dialog save commits to UserStore.
- The dialog supports recent-text suffix arrows, automatic code construction,
  editable code, multiline/tab word text, add/continue/cancel and visible save
  errors. It refreshes the service's immutable user-word view after saving.
- 633 original model cases pass on ARM64, including construct-code lookup,
  aliases/escapes, invariant code casing and text-element boundaries. Default
  ICU Indic tailoring differed from .NET StringInfo; explicit matching boundary
  rules now use Windows ICU property data. No whole-table heap copy is added.
- Eleven add-word shortcut profiles (20,460 events) pass on Linux and ARM64.
  Conflict rules preserve reserved bindings and suppress ambiguous actions.
- Native dialog controls pass a disposable-journal test: two saves, three
  dialog lifetimes, automatic code/suffix editing, validation, continue-add,
  cancel and close with no retained DLL lease. No physical input was sent.
  `build/add-word-dialog.png` is an inspected PrintWindow capture.
- Fixed duplicate keyboard-selection postprocessing and Ctrl/Alt+CapsLock
  composition cancellation, both found while extending add-word coverage.
  Separate action/history checks and existing input regressions cover the fixes.
- ARM64 package/build and registration-free TSF startup pass. Installation
  remains unchanged. Real TSF-to-dialog opening, IME behavior within edit fields,
  complete history fidelity, DPI/focus/application lifecycle remain unverified.
  See `native/ADDWORD.md` for the exact evidence and outstanding scope.

## Candidate visibility action update

- `{隐藏候选}` now returns a pure engine action and is applied only on actual
  TSF dispatch. Candidate UI style is rebuilt; previews do not mutate settings.
- New Windows configuration writer serializes toggles across cooperating hosts,
  preserves other text/options, flushes a complete replacement, and uses a
  recovery backup for existing files. See `native/CONFIG_STORE.md` for limits.
- ARM64 tests pass nine encoding cases, 52 toggles from four writers alongside
  2,000 reads from two readers, blocked replacement, malformed-input and output-
  size-limit preservation. Windows text readers now allow whole-file replacement.
- The new ARM64 DLL builds, package preflight and private TSF activation pass;
  the registered installed generation remains unchanged.
- Dynamic engine tests cover six additional hide-action/preview cases. Runtime
  failure keeps the current service's immediate toggle; focus reload can restore
  the disk value. Notification, durable retry and real-application UI validation
  remain pending.

## Native dialog validation and full private TSF verification (2026-09-08)

- Add-word validation now matches the original dialog's empty-code/empty-word
  messages and focus target, including code-first priority when both are empty.
  The ARM64 dialog probe passes four focus checks, two saved words, cancellation
  and shutdown across three dialogs, with no retained DLL leases.
- Rebuilt the ARM64 DLL and passed package preflight. Full private TSF tests
  pass in both UI-less and native-popup modes: each exercises 36 events across
  two contexts, real edit sessions and missing-layout recovery; native-popup
  mode also checks mouse selection. `build/tsf-private-validation.json` records
  the tested DLL hash and identical registered paths before and after.
- This supersedes activation-only evidence for these host checks. The fixture
  is a controlled TSF host, not broad application coverage. TSF-to-add-word
  opening and Chinese typing, ordinary mixed input, schema switching and the
  remaining acceptance items above are still unfinished.

## Ordinary mixed decoder foundation (2026-09-08)

- Added a native fixed-length mixed decoder, matching the unchanged original
  decoder in 1,931 stateful cases on both Linux and ARM64. Coverage includes
  raw-English fallback, preferred candidates, segment boundaries, shrinking
  input, cache invalidation, copied previews and long input. See
  `native/MIXED_INPUT.md` and `build/mixed-parity-*.json`.
- The input-engine setting, key transitions, candidate commit composition and
  TSF display still need integration and differential key tests. The new
  component does not enable mixed input in the packaged DLL yet.

## Ordinary mixed engine integration (2026-09-08)

- Connected the original mixed-input setting, full raw-code retention, resolved
  prefix, preferred page candidates, backtracking, selection, punctuation and
  language transitions. TSF composition and candidate code display now consume
  the resolved surface; trace snapshots retain the complete original keys.
- Three mixed profiles pass 39,474 original-Core key events on Linux and ARM64,
  including raw and resolved display comparisons. Default non-mixed regression
  passes 35,554 events per architecture. Reports and remaining gaps are in
  `native/MIXED_INPUT.md`.
- ARM64 build, package preflight and the existing full private TSF host tests
  pass. Those TSF tests use default settings; mixed-enabled application behavior
  still needs direct validation. Current system registration remains unchanged.

## Mixed input through real TSF (2026-09-08)

- Added an optional absolute `NATIVE_TIGER_USER_ROOT` for process-specific
  configuration/bindings/user-journal storage. Secure activation bypasses user
  storage. Normal activation keeps its existing LocalAppData default.
- A disposable-directory runner now activates the private DLL with mixed input
  enabled and two-character codes. Both UI-less and native-popup tests pass
  76 events across two contexts, checking prefix composition, cross-segment
  deletion, candidate finalization, raw Enter and Escape. The popup test clicks
  a mixed candidate; the UI-less test selects its second candidate.
- Default private TSF regression (36 events per mode), ARM64 build and package
  preflight pass. Reports match the current DLL hash, registration is unchanged,
  and the temporary user directory is removed. See `native/MIXED_INPUT.md`.
- Controlled TSF hosts are not broad application coverage. Remaining ordinary
  input features and application/lifecycle checks are still incomplete.

## Add-word through real TSF (2026-09-08)

- Added a private TSF add-word fixture with disposable configuration/journal.
  Both candidate modes pass 66 events across two contexts: the real Ctrl+=
  dispatch opens the dialog outside the host edit lock, controls save a word,
  and that word commits through both contexts after explicitly restoring host
  focus. Repeated TestKeyDown is included before actual dispatch.
- The report `build/tsf-private-add-word-validation.json` verifies the current
  DLL/test hashes, unchanged registration and successful runs. Temporary user
  data is removed. This fixture sends messages to the dialog controls, not
  physical Chinese keys; field IME behavior and application focus/lifetime
  coverage remain pending. See `native/ADDWORD.md`.

## Native add-word keyboard/focus verification (2026-09-08)

- Removed the fixture's manual host-focus restoration. Both candidate modes
  pass checks that saving restores the original foreground window, keyboard
  focus and TSF document automatically, then continue input in both contexts.
- Added 12 Windows SendInput events per run, targeted only after confirming
  the native dialog and edit control have focus. `ab`+Space becomes `交` in the
  word field and remains `ab ` in the code field. The existing 66 host events,
  outside-edit-lock check, save and subsequent new-word commits still pass.
- Report: `build/tsf-private-add-word-validation.json`. These results supersede
  the prior control-message-only limitation for these specific field checks.
  Arbitrary applications, user-initiated focus changes and DPI/shutdown cases
  remain unverified; no production DLL or registration changed in this step.

## Add-word recent-text bounds (2026-09-08)

- Matched the original window's maximum of 20 recent grapheme elements. The
  native UI probe verifies the two-element initial suffix, repeated expansion
  and contraction at both bounds, and a supplementary-plane emoji without
  splitting its UTF-16 pair. Save/cancel/focus/lifetime checks still pass.
- ARM64 rebuild, package preflight and real TSF add-word checks pass for the
  updated DLL. Per-commit grapheme history, deletion semantics and shared
  cross-application history remain unfinished; this change only fixes the
  window's history-selection limit.

## Grapheme history and preview sharing (2026-09-08)

- Shared the add-word segmentation rules with the engine using generated,
  read-only Windows ICU Unicode 15.1 property ranges. Add-word parity remains
  633 cases with no differences. Both platforms now use identical properties.
- Replaced UTF-16 history deletion/truncation with per-commit grapheme entries.
  A persistent stack lets previews share old history without copying it; long
  unique chains release iteratively. The add-word dialog receives the most
  recent 20 existing entries without re-segmenting their concatenated text.
- History probes pass 4,919 actions per platform and a 100,000-entry lifetime
  check. Engine checks cover separate combining-mark commits and whole emoji,
  combining, flag and ZWJ deletion. Default key regression passes 35,554 events
  per platform. Native UI, real TSF add-word/mixed checks, ARM64 build and package
  preflight pass. See `native/HISTORY.md` for scope and evidence.
- Shared cross-application history, broader runtime/application verification
  and the remaining acceptance scope are still unfinished.

## Pass-through history fidelity (2026-09-08)

- Added missing English punctuation, shifted digit symbols and numeric-keypad
  digits to guessed history, removed the extra English Enter entry, and matched
  original ordering of guessed text before explicit result text.
- A new oracle wrapper mode reads original `GetLastCi(20)` after real key
  processing, without modifying any original source. Chinese/English history
  parity passes 17,220 events per architecture across all virtual keys and
  modifier combinations plus continuous append/delete history.
- Fresh default original-Core regression passes 35,554 events on Linux and
  ARM64. Current ARM64 build, package preflight and real TSF add-word checks
  pass. See `native/HISTORY.md`; global/shared history remains unfinished.

## User-journal write failure recovery (2026-09-08)

- Journal append/flush exceptions now trigger whole-batch truncation and flush
  back to the previous valid length under the existing exclusive lock. Failed
  rollback is reported distinctly. TSF restores persisted (or last known
  persisted) lexicon state after failed adjustments instead of displaying an
  unsaved local reorder; unavailable storage follows the same path.
- Linux real short-write injection passes 88 positions of a two-Advance batch,
  checking exact rollback bytes and non-duplicating retry. Concurrent writer,
  interrupted-tail and corruption tests still pass on Linux and ARM64.
- Both private ARM64 TSF candidate modes pass a denied-journal-write Ctrl+2
  case and verify unchanged candidate order/Space output. The combined add-word
  fixture now covers 74 host events plus 12 dialog keyboard events. ARM64 build
  and package preflight pass.
- This does not prove rollback under failing flush/storage or crash atomicity.
  Durable pending operations and recovery UI remain incomplete; see
  `native/USERDATA.md` for exact guarantees and limitations.

## Manual timer parsing foundation (2026-09-08)

- Added `Ds`/`DS` parsing with distinct consumed/no-timer/immediate-delay states,
  comma handling, original final-LF regex behavior, underflow and INT_MAX delay
  clamping. Both architectures match 2,150 original-regex/.NET arithmetic cases.
- No timer is started by the oracle or probes. Actual uppercase commit actions,
  scheduling, replacement and reminder UI/lifetime are still unimplemented.
  The parser does not enable timers in the currently registered runtime.
  See `native/UPPERCASE.md` and `build/manual-timer-parity-*.json`.

## Native timer actions and scheduler (2026-09-08)

- Connected uppercase timer commits to an explicit pure-engine delay action
  and actual TSF scheduling. Ten engine cases pass on both architectures,
  including preview isolation, punctuation, clearing and key-release behavior.
- Added a generation-checked Windows scheduler. ARM64 tests pass timer
  replacement, cancellation, one-shot callbacks and reentrant owner destruction.
  The actual reminder caption/text and dismissal also pass after moving default
  desktop notification to a separate thread with a loader reference.
- Pending timers currently belong to one TSF service and cancel on deactivation.
  Global replacement, host-exit survival, locked desktop and full TSF-to-expiry
  validation remain open. See `native/UPPERCASE.md` and manual-timer reports.

## Full TSF timer command-to-expiry verification (2026-09-08)

- Both private TSF modes now pass the complete `Ds1` command, one-minute wait,
  expected notification text, button dismissal and scheduler teardown. Each
  run exercises 44 host events across two contexts. Preview must leave no
  scheduler window; only actual submission creates one.
- The first run reached expiry but failed dismissal because the fixture assumed
  IDOK. The fixture now finds the actual button and verifies window destruction.
  The short scheduler probe was strengthened too: its earlier close request
  alone was insufficient evidence of dismissal.
- `build/tsf-private-timer-validation.json` matches current DLL/test hashes,
  records both actual expiry times, and confirms unchanged registration.
  Disposable user data is removed. Global timer replacement, host-exit survival
  and locked-desktop behavior remain open; see `native/UPPERCASE.md`.

## Oracle isolation correction

The original Core synchronizes the user's Run registry key during configuration
initialization. Initial filesystem-only staging did not prevent this side effect.
The daily TigerClawCore autorun entry was restored using the enabled runtime
configuration and the confirmed running daily executable, with its original
Core convention `--with-overlay`. No source or daily-runtime data files were edited.
RegistrySandbox now redirects the oracle process's HKCU into a unique disposable
subtree before any original-Core initialization. A subsequent smoke run verified
that the real TigerClawCore Run value remained identical before and after.

## Useful commands (WSL unless indicated)

```
g++ -std=c++17 -Wall -Wextra -Werror -O2 -Inative native/Dictionary.cpp tests/dictionary_probe.cpp -o build/dictionary_probe
g++ -std=c++17 -Wall -Wextra -Werror -O2 -Inative native/Dictionary.cpp native/Lexicon.cpp native/Text.cpp native/SelectionKeys.cpp native/Settings.cpp native/DynamicText.cpp native/UppercaseText.cpp native/Engine.cpp tests/engine_probe.cpp -o build/engine_probe
g++ -std=c++17 -Wall -Wextra -Werror -O2 -Inative native/Dictionary.cpp native/Lexicon.cpp native/Text.cpp tests/lexicon_probe.cpp -o build/lexicon_probe
g++ -std=c++17 -Wall -Wextra -Werror -O2 -Inative native/Dictionary.cpp native/Lexicon.cpp native/Text.cpp native/UserStore.cpp tests/user_store_probe.cpp -o build/user_store_probe
python3 tests/check_dictionary.py build/dictionary_probe data/tiger-v2.tcd
python3 tests/check_dictionary_corruption.py build/dictionary_probe data/tiger-v2.tcd
python3 tests/key_parity.py
python3 tests/lexicon_parity.py
python3 tests/user_store_concurrency.py
python3 tests/user_store_concurrency.py --windows
```

Windows builds use VS 18 / v145 and `tests/DictionaryProbe.vcxproj`,
`tests/EngineProbe.vcxproj`, `tests/LexiconProbe.vcxproj`, or
`tests/UserStoreProbe.vcxproj`, Configuration=Release, Platform=ARM64.
Run `tests/measure_dictionary_memory.ps1` in native Windows PowerShell after
building DictionaryProbe. ReferenceOracle builds with .NET 10 and must only be
run against a staging directory containing `.native-tiger-staging`.

## 2026-09-09 候选与注解延迟展开

- 对照原版 `MainWindow.Candidate.cs`、`CandidateTextFormatter.cs` 和
  `CoreRuntimeState.cs`，增加两个原名配置键及“候选外观”中的毫秒输入框。
- 共用输入会话起点、分别计时，持续输入不重置，展开后保持；拼音反查注解
  立即显示；保留原版隐藏候选与正延迟的组合行为。
- 使用候选窗口所属线程的 Win32 定时器，不增加 Core 或 IPC；没有光标布局
  时保持隐藏，UI detach 销毁定时器。UI-less 数据与按键提交不受延迟影响。
- 默认值均为 0，现有用户配置不会被自动改成正延迟。
- 新增可重复的 `tests/candidate_reveal_test.py`；设置测试覆盖保存、重开、
  取消及八种非法延时输入。前台 Word、微信、32 位程序的延时观感仍需实测。
- 验证通过：延时状态/解析边界测试、设置控件保存/重开/取消/非法值测试、
  96/144/192 DPI 边界检查；144 DPI 截图人工查看无重叠。
- ARM64X 与 Win32 构建、三架构加载/创建实例检查通过，已安装版本
  `48c888085b86fec3`；两套注册表视图、DLL 哈希及已测设置程序一致。
  记录：`build/candidate-reveal-validation.json`、`build/input-settings-arm64.json`、
  `build/candidate-reveal-installed-audit.json`。

## 2026-09-09 候选窗右键菜单与滚轮字号

- 候选窗右键复用任务栏的方案、主题、管理菜单；使用候选窗口作为 owner，
  不激活临时窗口。滚轮每 120 单位调整 0.5，范围 3–200，保留小幅滚动。
- 加锁读取最新字号后原子保存，只写字体大小，不发出清空输入的设置重载请求；
  当前窗口立即重排，其他实例通过已有配置监视同步。
- 实际 TSF 窗口 ARM64、x64、Win32 测试通过：放大/缩小、持久化、12 项菜单、
  取消菜单后焦点/编码保留、随后空格上屏。测试在临时前台宿主中发送定向消息，
  未注入全局输入；独立非活动桌面不能取得真实前台焦点，未用它宣称焦点通过。
- 字号边界、小幅滚动、8 次并发更新及其他配置保留测试通过。物理鼠标在用户
  常用应用中的体验仍待实测。记录位于 build/candidate-mouse-validation.json、
  build/candidate-wheel-validation.json 和 build/candidate-mouse-installed-audit.json。
- 已安装版本 `842f19b2e76b60c8`，两套注册表及已安装 DLL 与测试产物一致。

## 2026-09-09 中键布局、模式边距与定位

- 中键松开循环横排/竖排/仅编码，分别记忆横、竖排显示编码偏好；三个公开
  配置值加锁一次写入，保持当前编码和输入焦点。
- 对齐虎爪三种模式的 padding、字号行高、最小宽度；横排采用实际空格宽度
  与编码补空格，不再固定 10 DIP 或自动换行。主题边框参与外部尺寸计算。
- 定位改为原版 5 物理像素光标间隔、右下 2 像素预留；底部不足时择上方，
  本次输入缩小时保持上方，跨监视器重置。仍使用真实 TSF 光标及 PMv2。
- 模式/字号/边缘定位自动测试与前台临时宿主的中键循环测试已补充；
  多实体屏幕拖动及 Word/微信人工并排观感仍需实测，不以模拟测试替代。
- 已通过：三架构 270 个渲染/DPI 用例、模式尺寸/边距/定位断言、模式偏好
  保存测试、原有延迟展开回归及 ARM64X 加载预检。已查看竖排、横排和仅编码
  的 144 DPI 图像。
- 本轮前台中键测试停在首键前的 OS 焦点检查，尚未验证中键消息路径；
  不沿用上轮鼠标测试的通过状态。当前报告 candidate-mouse-validation.json
  如实记录该失败。已询问用户是否可暂停键鼠测试。
- 安装尚未完成：ARM64X 安装等待 Windows UAC（WSL session 77862）；提前
  启动的 x86 安装等待进程已停止，待主版本成功后再执行 install_x86.ps1。
  截至此记录，注册版本仍是 842f19b2e76b60c8。

- 后续确认：ARM64X UAC 已完成，已继续完成 Win32 安装；两者版本均为 `9f83ef2f5a2a7364`，安装 DLL 哈希核验一致。中键前台焦点测试仍待重测。

## 2026-09-09 编码伪装

- 添加原版“编码伪装”设置、Unicode 字素替换器、已解析混合前缀标记，
  候选窗编码和 TSF 预编辑共用显示变换，查码/候选/提交不变；留空关闭。
- 修正配置读取过早 trim 行尾的问题，保证后置空值可清除旧值。
- 通过 Unicode/空白/前缀/候选不变单测、延迟展开回归、设置保存/重开/取消
  与 96/144/192 DPI 检查，144 DPI 截图已人工检查。
- ARM64、x64、Win32 的真实 TSF 编辑会话通过多码元伪装退格/恢复、原始候选、
  上屏、混合前缀及取消验证。专用宿主显式激活服务，不借此宣称物理前台通过。
  前台鼠标宿主尝试停在焦点检查；已保留报告，不沿用旧结果。
- 记录：build/code-mask-validation.json、build/code-mask-tsf-validation.json、
  build/input-settings-arm64.json；详细行为见 docs/CODE_MASKING.md。
- 已安装版本 `789f4a97fb336ad0`；双架构注册表、三架构已测 DLL 以及设置程序哈希核验一致，记录见 build/code-mask-installed-audit.json。
