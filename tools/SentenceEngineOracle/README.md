# Full sentence engine oracle

This project compiles the frozen original `InputMethodEngine`, `CoreRuntimeState`,
model and decoder from `../SentenceOracle/upstream`, without editing their method
bodies. Additional Core/Shared source dependencies were copied byte-for-byte from
the same reference checkout and recorded in `engine-dependencies-sha256.json`.
The test verifies both source manifests before running.

`Driver.cs` accepts an isolated runtime root and JSONL physical-key events. A
`.native-tiger-staging` marker is mandatory. The reused RegistrySandbox redirects
HKCU before the original runtime initializes. The driver starts no pipe server,
Overlay or resident Core and assigns no rerank service. RerankContracts contains
only the original request and interface declarations, excluding the client and
its service-launch dependencies entirely.

The default suite uses the original synchronous-decoding constructor option and
automatic commits disabled. `SENTENCE_TRACE_AUTO=1` selects the original runtime
asynchronous worker and waits for its pending flag to clear after each key. The
native adapter publishes each completed result without committing on completion.
`SENTENCE_TRACE_RETAIN=0` or `3` selects the minimum retained raw setting (default
3). Both engines use matching duplicate-single-character eligibility, a small
imported scheme, reverse-lookup data and the real mapped n-gram model.

The comparison covers handled/cancel status, language and input modes, page,
full raw, output text, candidate order/annotations, selected row, display code,
cumulative committed text and committed raw boundary. It does not compare
implementation-specific generation counters. `SENTENCE_TRACE_LEGACY=1` with
automatic mode simulates the former erroneous native completion-time policy;
it is a negative control and is expected to fail parity.

`SENTENCE_TRACE_BURST=1` adds a seeded per-event publication schedule. The driver
holds the original engine's reentrant synchronization lock across deferred keys,
then releases it and waits for the unchanged worker at designated boundaries.
The native harness defers result production/publication at the same boundaries;
manual selection/commit keys still complete decoding as in the original. Each
case ends with a release. This controls queued work before capture; it does not
simulate a result already computed and then delayed in transit. Original method
bodies and source manifests remain unchanged.

Run `python3 tests/sentence_engine_parity.py` after building this project and
`tests/SentenceTraceProbe.vcxproj`. Set `SENTENCE_TRACE_PLATFORM` to `ARM64`
(default), `x64` or `Win32` to select the independently built native executable.
The project uses separate architecture output/intermediate directories, with
warnings treated as errors. Reports include platform and binary/source hashes.
Reports and both complete traces are
written under `build/sentence-engine-*`, with separate names for each mode. The
default asynchronous schedule settles each key; burst mode covers deferred-worker
queues. Already-computed stale completion injection, larger real-world schemes
and resource changes still require additional traces. The real TSF suite separately checks that six
letters followed by a pause do not commit, while the seventh appended letter does.
