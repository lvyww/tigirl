# Native sentence input (without Qwen)

Goal: port TigerClaw sentence input into the native TSF implementation. Qwen,
neural reranking, model-server IPC and a resident Core are excluded. Ordinary
word input, independent TSF contexts, architecture coverage, existing user data,
DirectWrite candidates and shared dictionary storage must remain intact.

Current status (2026-09-09): the development DLL includes mapped sentence
resources, decoder, automatic/empty-code policies, background decoding and TSF
publication. ARM64/ARM64X/x86 isolated TSF checks pass. Sentence generation
`56502c3a10a47997` and its matching x86 component are now installed. Installed
DLL hashes, both COM registry views and shared artifact hard links were verified;
foreground ordinary-input Rich Edit checks pass on ARM64, x64 and Win32.
Decoder parity and eight real-table engine configurations cover the current
decoder; 18 synthetic-table engine configurations are earlier checkpoint evidence.
Physical model/index sharing is verified across architectures. Long-input
private state and commit latency have been measured, including the remaining
x86 long-burst delay. Actual installed sentence application acceptance remains
pending. Model packaging and default resource lookup now pass ARM64X
ARM64/x64 and isolated hard-linked x86 TSF checks. See PROGRESS.md (newest
entries first) and SENTENCE_PERFORMANCE.md for current evidence. The checkpoints
below are historical: statements such as "not linked" describe their checkpoint,
not the current development DLL. The development settings dialog now exposes
all six non-neural sentence options; see SENTENCE_SETTINGS.md.
Computed-result callbacks held behind a real TSF edit lock now pass positive
delivery and stale context/thread-focus rejection on ARM64X ARM64/x64 and x86.

Reference: current supplied checkout, frozen separately in
`tools/SentenceOracle/upstream` with `upstream-sha256.json`. Do not substitute the
older ordinary-input oracle for the newer sentence implementation. The original
checkout and running daily TigerClaw remain untouched.

Required before completion:

1. Read the original `sentence-ngram-v2.bin` format and reproduce non-neural
   unigram/bigram/trigram interpolation, unknown/scalar handling and observed
   bigram queries. Keep the large model file mapped read-only across processes.
2. Prepare a shared sentence lexicon: optimal single-character codes, high
   frequency character threshold, full-code whitelist, duplicate-character
   policy, ranks, text elements, prefix metadata and supplementary corpus.
   Do not reconstruct the full sentence lexicon independently in every host.
3. Port decoder lattice/beam expansion, suffix selection markers, path scores,
   n-gram/supplement/isolation scoring, duplicate elimination, deterministic
   candidate order, segmented-code boundaries and incremental decoding/cache
   invalidation. Compare full and incremental results to the frozen original.
4. Port the non-neural sentence engine state machine: activation by scheme and
   configuration, continuing input, delete/escape/commit, paging/selection,
   punctuation and reverse-lookup transitions, empty-code decisions, early
   prefix commit evidence and minimum retained raw length. Preserve ordinary
   input semantics and mode/focus changes.
5. Integrate responsive decoding and stale-result rejection into TSF edit
   sessions and candidate UI. Context/schema/lexicon changes and deactivation
   must not apply old results. Wire user-facing sentence settings; omit Qwen.
6. Package native ARM64X plus x86 and required immutable model/resources. Verify
   import/reload/fallback, model sharing/private memory, latency and real input
   with the user's tables. Validate on the installed DLL, not just decoder tests.

Evidence must include original golden decoder/engine traces, randomized parity,
model scoring parity, malformed-file handling, lifecycle tests, current native
builds and application acceptance. Early component successes do not complete
sentence input or authorize claiming it is installed.

Initial inspection: reference n-gram file is 238,789,568 bytes; provenance is in
`build/sentence-model-source.json`. Existing native word tables discard original
numeric frequencies but retain ordering; sentence import must explicitly derive
all metadata needed by the original sentence index. Current sentence sources
are substantially newer than the ordinary-input oracle snapshot.

Progress 2026-09-09:

- Added standalone `native/SentenceNgram.{h,cpp}`: original-format read-only
  mappings, weak process-local mapping reuse, bounded header validation, exact
  Unicode-scalar resolution and non-neural probability/observed-bigram queries.
  Mutable decoder caches are deliberately separate from the shared model view.
- `tests/sentence_ngram_test.py` compares 13,006 queries per platform against
  the freshly built frozen original C# reader: Linux, ARM64, x64 and x86 pass.
  Queries sample real model bigrams/trigrams plus unknown tokens, surrogate pairs,
  lone surrogates, multi-element strings and both include-unigram modes.
  Maximum absolute score error is 1.78e-15; observed-bigram results agree.
- Eight malformed model cases are rejected by the original and all four native
  probes. Report: `build/sentence-ngram-validation.json`.
- No sentence code has been linked into the installed TSF DLL yet. Decoder,
  shared sentence lexicon metadata, engine/UI lifecycle and installation remain
  required. Model mapping reuse is tested within a process; physical-page sharing
  and actual sentence input latency remain unmeasured for this component.

Next implementation dependency: `ChooseShorter` intentionally resolves equal
code lengths by source-table encounter order. Existing TCD serialization sorts
keys, so rebuilding sentence metadata from TCD key order would be incorrect.
Prepare optimal/primary-code metadata from `ImportedLexicon.main` before sorting
and preserve it in shared storage. Candidate commit-text aliases must be removed
and duplicates collapsed before assigning sentence ranks, matching
`GetSentenceLexiconSnapshot` and `SentenceLexiconIndex.Build`.

Sentence lexicon checkpoint:

- `native/SentenceLexicon.{h,cpp}` prepares primary/optimal single-character
  codes from original encounter order. It preserves ranks after commit-text
  deduplication and supports dynamic common-character and whitelist filtering.
- Shared storage reuses the validated immutable TCD container with a mandatory
  `_sentence_format=1` marker in Split. Main contains deduplicated commit text,
  FullCode contains primary codes, and ConstructCode contains optimal codes.
  This is a separate sentence sidecar, not an ordinary input dictionary. Runtime
  queries retain string views into its file mapping; only queried candidates'
  grapheme arrays and small configuration/length sets are allocated privately.
- `tests/sentence_lexicon_test.py`: 252 source/config fixtures and 7,420 queries
  per ARM64/x64/x86 probe match frozen `SentenceLexiconIndex.Build`. Tests include
  source-order ties, first-candidate priority, duplicate ranks, empty tables,
  filtered proper prefixes, whitelist overrides, ordinal Unicode aliases and
  .NET-compatible grapheme segmentation. Each fixture is serialized and reopened
  through the actual mapped reader. Evidence: `build/sentence-lexicon-validation.json`.
- The builder input contract is already-unpacked commit text, as returned by
  original `GetSentenceLexiconSnapshot`. Production import integration still
  needs to unpack display/commit aliases before preparation and associate the
  sidecar with its ordinary dictionary generation and user-word revision.
- The oracle now compiles the frozen decoder/support classes for original index
  queries as well. No decoder has yet been integrated into the native runtime.
  Supplementary corpus indexing and sentence resource settings remain required.

Supplement matching checkpoint:

- Added `native/SentenceSupplement.h`, reproducing entry weight bounds/log rewards
  and the grapheme-based failure-link automaton. Duplicate/suffix hits retain
  maximum reward rather than summing. Traversal is immutable and preserves
  original node IDs; invalid states and empty input follow original behavior.
- `tests/sentence_supplement_test.py` compares 240 fixtures / 18,480 state and
  reward transitions on each ARM64/x64/x86 probe against the frozen original.
  Includes weight extremes, repeated/overlapping entries, missing transitions,
  empty inputs, surrogate pairs, lone surrogates and combining/ZWJ sequences.
  Report: `build/sentence-supplement-validation.json`, all passed.
- This is the import-time automaton implementation. Its graph still needs a
  shared serialized representation before integration into TSF; do not build
  and retain a complete graph independently in every host. Supplement text-file
  parsing and decoder integration remain pending. Installed version unchanged.

Mapped supplementary graph checkpoint:

- Added `SentenceSupplementMatcher::serializeGraph()` and
  `native/MappedSentenceSupplement.h`. Main records store fixed-width hex node
  metadata (failure, depth, IEEE double reward); Pinyin records store transitions
  keyed by fixed-width state ID plus grapheme. A Split-section version marker
  distinguishes graphs from ordinary and sentence lexicons. The reusable TCD
  reader validates offsets, sorted keys, string ranges and cardinality.
- The runtime graph keeps only its shared mapping and node count. It validates
  node numbering, finite rewards, bounded IDs, strictly shallower failure links
  and increasing transition depths. Queries deserialize only scalar metadata
  for visited nodes, not complete node/edge dictionaries.
- Existing 18,480-transition parity per ARM64/x64/x86 now runs through serialized
  files reopened as mapped graphs. Five corrupt graph tests reject missing
  markers, infinite rewards, cyclic failure links, out-of-range targets and
  backward-depth transitions. Report includes builder/reader/probe hashes.
- This replaces the earlier pending shared-graph representation task. Text-file
  parsing, supplementary resource publication, decoder integration, actual
  multi-process memory measurement and final installed behavior remain pending.

Decoder checkpoint:

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
