# Sentence resource and decoder measurements

Run `python3 tests/sentence_measure_test.py` after building
`tests/SentenceMeasureProbe.vcxproj` for ARM64, x64 and Win32, plus the ARM64
LexiconImport tool. The runner copies the reference 虎整句 source into an isolated
build directory, imports it, runs three ARM64 benchmark processes and one fresh
context-memory process, then holds three different-architecture mapping readers
alive together. Source/binary/model hashes and raw outputs are recorded under
`build/real-sentence-measure/validation.json` and its staging directory.

These are optimized standalone decoder/resource measurements on the current
Windows ARM64 machine with a warm filesystem cache. They do not include installed
TSF edit-session scheduling, application overhead or cold-storage startup.

Resource-sharing baseline (unchanged by the scoring optimization):

| Measurement | Result |
| --- | --- |
| Imported real scheme | 14,429 ordinary records |
| Model | 238,789,568 bytes, 58,299 pages |
| Sentence index | 2,791,708 bytes, 682 pages |
| Sharing | All model/index pages resident and multiply shared in concurrent ARM64/x64/x86 readers |
| Mapping protection | MEM_MAPPED, PAGE_READONLY in all three readers |
| Private resource-loading increment | ARM64 0.590 MiB, x64 0.617 MiB, x86 0.535 MiB |
| Four live 32-code decoder contexts | About 5.48 MiB private increment in one fresh process |

The model is not copied into each process's private heap. Active lattice state is
private and grows substantially with sentence length. Full-decode medians over
three runs of the fixed real-table primary-code workload before optimization:

| Raw codes | Time | Process private bytes after decoding |
| --- | --- | --- |
| 8 | 0.58 ms | 2.06 MiB |
| 16 | 0.67 ms | 2.12 MiB |
| 32 | 5.03 ms | 3.77 MiB |
| 64 | 83.41 ms | 35.39 MiB |
| 128 | 281.63 ms | 117.10 MiB |

Incremental decoding over 64 appended codes has per-run p95 of about 32–34 ms.
Private bytes include allocator retention and earlier phases; the context count
measurement therefore runs in a separate fresh process. This is one representative
workload, not a comprehensive latency distribution or a real-table accuracy test.
Long-input memory and synchronous commit-key decoding remain release concerns.

An exploratory `vector::shrink_to_fit` after beam pruning reduced the 128-code
sample from roughly 117 MiB to 86 MiB, but its measured full/incremental decoding
became slower. That change was reverted and is absent from the accepted
optimization. The experiment's before/after artifacts are retained for
comparison, not as evidence of an accepted optimization. Further work should avoid
reallocating recently mutable beam buckets and investigate repeated scoring cost,
while preserving exact original candidate order, scores and prefix evidence.

Sentence overlay cache lifecycle is now bounded independently of decoder memory.
After a sentence revision is opened or rebuilt, best-effort cleanup keeps the
active revision plus the seven newest inactive revision directories. A revision
whose `.import.lock` is currently held is skipped, and reparse-point directories
are never traversed. If `Tigirl.Import.exe --ensure-sentence` exceeds the 30-second
resource-load wait, the TSF worker terminates and reaps that helper before returning
an error so abandoned helpers cannot accumulate behind the cache import lock.

The accepted scoring/state optimization adds a direct-mapped 4,096-entry cache
for immutable n-gram queries whose tokens fit two UTF-16 units. Exact keys are
checked on every hit; longer tokens and non-file/custom models bypass it. Each
lattice cache is at most 128 KiB; replacing a lattice can temporarily retain old
and new caches. It avoids grapheme/string allocation when an entire candidate
contains only basic unified Han or ASCII letters, retaining the original Unicode
path otherwise. State context tokens now reference immutable mapped lexicon text
instead of owning two strings; the decoder owns that lexicon for their lifetime.

Three-run medians after these changes:

| Raw codes | Time before → after | Process private before → after |
| --- | --- | --- |
| 32 | 5.03 → 3.64 ms | 3.77 → 3.88 MiB |
| 64 | 83.41 → 58.00 ms | 35.39 → 32.21 MiB |
| 128 | 281.63 → 200.58 ms | 117.10 → 101.32 MiB |

Incremental p95 was 26.8–36.0 ms across the three new runs. Process private bytes
in that later phase varied substantially (about 32–60 MiB), so this is not proof
of universally lower retained heap use. The 128-code cold/full completion is still
slow enough to require attention. Reports are `score-before-validation.json`,
current `validation.json`, and `score-comparison.json` in the measurement directory.
All three architectures pass the original decoder differential and all 18
full-engine trace configurations after these changes.

The subsequent bucket-index optimization stores text hashes and vector positions
instead of a second owned text string for every unpruned candidate. Hash matches
still compare complete UTF-16 text, so collisions do not merge distinct candidates.
Indices remain valid when the state vector reallocates; sorting discards the index
and later additions rebuild it. This reduces duplicate string allocation without
changing pruning, rank choice or probability aggregation.

Three-run full-decode medians on the same workload:

| Raw codes | Scoring-cache version → compact index | Process private after |
| --- | --- | --- |
| 32 | 3.64 → 3.54 ms | 3.74 MiB |
| 64 | 58.00 → 53.83 ms | 32.68 MiB |
| 128 | 200.58 → 174.78 ms | 101.18 MiB |

Incremental p95 is 26.9–30.7 ms in these three runs. Retained private memory is
not substantially reduced; long-input memory remains a separate concern. See
`index-comparison.json` and `index-after-validation.json` in the measurement
directory. The earlier 200.58 ms measurement survives in `run-7ujy9qbo` raw files;
`score-after-summary.json` reconstructs and checks its medians against the saved
comparison. It is a metric summary, not the superseded original validation manifest.

## Hidden real TSF key and commit measurements

Run `python3 tests/sentence_tsf_measure_test.py` after building all three TSF
hosts and validating the ARM64X package. It uses the supplied real table, checks
every committed string against the frozen original full engine, and measures
the ARM64X DLL in ARM64/x64 hosts and the x86 DLL in a hard-linked package layout.
The automatic-commit option is off. Input is three repeated 32/64/128-code
prefixes, with either no pause or 30 ms of apartment pumping between keys.
Each timed key includes two previews, dispatch and key-up callbacks.

All 54 completed compositions match the original. Three-run median Space-key
times (milliseconds) in actual TSF edit sessions:

| Host | 32 codes, paced / burst | 64 codes, paced / burst | 128 codes, paced / burst |
| --- | --- | --- | --- |
| ARM64 | 1.32 / 17.30 | 4.74 / 49.10 | 18.89 / 132.26 |
| x64 (ARM64X) | 1.77 / 10.71 | 10.27 / 66.25 | 26.85 / 167.19 |
| x86 | 1.79 / 11.05 | 10.23 / 117.22 | 30.31 / 310.21 |

For paced input, median per-trial encoding-key p95 spans about 0.75–2.04 ms,
depending on host and length. Encoding stays responsive while decoding catches
up; immediate Space after an unpaced long burst can still wait noticeably,
especially in x86. These are three fixed workloads, not general latency bounds.

Private memory 200 ms after a paced 128-code commit has medians about 45.4 MiB
in ARM64, 18.5 MiB in x64 and 17.6 MiB in x86. Later short compositions can
reduce retained memory again. Three rounds do not establish a leak-free or
bounded-memory guarantee; process overhead and allocator retention are included.
The model remains a shared read-only mapping.

`build/sentence-tsf-measure-all.json` records exact source/model/binary hashes,
medians and the preserved staging directory containing original output and all
raw measurements. These hosts exercise the real TSF path but are hidden test
applications, not installed Word/WeChat or physical keyboard acceptance.

## 2026-09-19 comprehensive latency pass

This pass keeps the beam width, candidate order, exposed scores and early-commit
math unchanged. The exact decoder differential still compares 5,120 snapshots,
and the learning replay compares 163,440 score/prefix results. The production
model remains the same September 19 probability set; Windows now maps its native
TCSKNM02 layout directly instead of expanding it back to TCSKNM01.

The accepted changes are deliberately structural:

- early-commit prefix mass uses a rolling exact-checked hash index and raw-length
  vector instead of tree/set allocation; incomplete-tail evidence computes only
  confidence fields and skips final-ranking `pathIsolation` work;
- beam truncation selects top-K with `nth_element` then sorts only the retained
  states;
- internal sentence boundaries live in a contiguous lattice arena. Public
  `shared_ptr<SentencePathBoundary>` chains are materialized only for published
  candidates/evidence;
- beam states no longer own/copy the entire accumulated UTF-16 sentence. They
  retain a parent boundary, immutable edge view, text length and rolling hash;
  full strings are materialized for published candidates, learning when active,
  and exact collision/tie checks only;
- common BMP CJK/ASCII `pathIsolation` traversal avoids grapheme vectors and
  boundary `shared_ptr` copying;
- the TSF key path compares a lightweight composition raw view instead of making
  two complete candidate snapshots merely to detect a text change;
- candidate rendering reuses per-thread D2D/DWrite/WIC/private-font resources,
  target brush/layer objects and measured space widths across compositions;
- the packaged model is `sentence-ngram-mobile.bin` (TCSKNM02, 224,475,584
  bytes) instead of the 272,600,424-byte expanded TCSKNM01 file. The release
  artifact is still SHA-256 pinned. Custom TCSKNM01 files remain supported with
  the legacy full validation path; TCSKNM02 validates header/unigrams/sparse
  indexes eagerly and bounds-checks variable pages during lookup.

### Windows x64 paired real-model measurement

Five fresh-process rounds were interleaved between `main@3eae011` and the final
optimized tree, using the same prepared real 虎整句 dictionary and the same
September 19 probabilities. Main used its packaged-form TCSKNM01; optimized used
the bit-identical TCSKNM02 layout. Every workload expanded the same number of
states (14 / 88 / 3,508 / 97,668 / 315,668 for 8/16/32/64/128 codes).

| Measurement | main | optimized | change |
| --- | ---: | ---: | ---: |
| Resource open median | 127.71 ms | 8.80 ms | -93.1% |
| 32-code full decode | 12.34 ms | 8.41 ms | -31.9% |
| 64-code full decode | 80.57 ms | 52.55 ms | -34.8% |
| 128-code full decode | 260.48 ms | 168.59 ms | -35.3% |
| 64-key incremental p95 | 52.36 ms | 28.23 ms | -46.1% |
| 128-code process private after decode | 102.24 MiB | 100.18 MiB | -2.0% |
| Incremental phase process private | 51.93 MiB | 47.21 MiB | -9.1% |

Process-private figures include the executable, allocator retention and earlier
phases in the same probe, so they are not a boundary-arena byte count or a leak
proof. The optimized synthetic dense-beam probe exposes the arena separately:
on Windows x64 it retained 4,065 live states in 7,490 state slots (958,720 bytes),
132,160 internal boundary nodes (11,060,400 bytes of boundary capacity), but only
2,167 boundary nodes needed public `shared_ptr` materialization.

A 14,000-query random/observed production-model equivalence check between the
TCSKNM01 and TCSKNM02 native readers had zero score error and identical observed
bigram flags. Fresh WSL processes opened the old layout in about 1.62–1.75 s and
the paged layout in about 12–23 ms; those WSL numbers are diagnostic only and
must not be substituted for the Windows figures above.

The repository's existing ARM64 `sentence_measure_test.py`, now pointed at the
packaged TCSKNM02 model, also completed its three-run standard probe. Current
medians were 0.490 / 0.576 / 5.261 / 32.366 / 98.945 ms for 8/16/32/64/128
codes; the three incremental p95 values were 20.10 / 17.92 / 17.20 ms. These
are current-tree absolute measurements rather than a paired old/new experiment.

Candidate-renderer construction plus layout, repeated on one Windows x64 thread
with the bundled private font, fell from 0.391 to 0.257 ms per instance (~34%).
The actual renderer regression still passes 90 render cases on both x64 and
Win32, including private fonts, DPI round trips, premultiplied alpha and fallback.

Release staging was built for x64, Win32, ARM64 and ARM64X. The ARM64X installer
preflight passed real ARM64+x64 loader checks plus missing/corrupt-model, missing
lexical-prior, wrong-architecture and pre-elevation negative controls.

Release staging was built for x64, Win32 and ARM64. The x64 package manifest
contains `Models\sentence-ngram-mobile.bin` at 224,475,584 bytes with SHA-256
`23216acd8319885aa2431ffbf2231dab4677c5d4abb55a08a404450a15b865ca` and no
expanded `sentence-ngram-v2.bin`; the generated x64+x86 ZIP was 175,868,508
bytes. These measurements are engineering probes, not installed Word/WeChat or
physical-keyboard latency claims.
