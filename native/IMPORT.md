# Original table import (in progress)

`LexiconImport.h/.cpp` implements the original `ParseMbFile` row layer for an
import-time utility. It consumes decoded UTF-16 text and returns coded and uncoded
rows, retaining encounter order and signed 32-bit frequencies. It is not loaded
by the TSF DLL and is not yet a complete table-import command.

`mergeCodedLexiconRows` now performs stable descending frequency sorting,
invariant normalization and ordinal case-insensitive code grouping. Packed
candidate entries deduplicate by exact identity, retaining the first occurrence
at the highest frequency; different aliases with the same commit remain distinct.
Both candidates within a code and the first-insertion order of code groups match
the original. Auxiliary-map construction must receive that insertion order;
binary serialization will perform its own ordinal key sorting later. A per-group
identity set prevents quadratic scans for large duplicate groups. These temporary
import allocations do not become per-host TSF dictionary storage.

`tests/lexicon_merge_parity.py` compares 106 all-coded input fixtures (25,517 rows)
against the original complete `BuildLexiconSnapshot`, including the map's native
enumeration order. Tied/extreme frequencies, duplicate aliases, Unicode code
identities and a 20,000-row duplicate group pass with zero differences. Report:
`build/lexicon-merge-parity-arm64.json`. This intentionally does not yet exercise
uncoded-word inference; the separate pipeline test below now covers that stage.

`buildImportedLexicon` builds the initial construction lookup, infers uncoded
rows and merges them at their original frequencies, then rebuilds construction
and full-code maps. Dedicated `构词.txt` coded rows override main-table choices;
the first eligible row wins regardless of frequency, and only single text elements
qualify. Fallback choices exclude one-unit codes and use the original leading-letter
priority and ordinal tie break. Dedicated-file uncoded rows join ordinary uncoded
rows after the ordinary rows. The caller must exclude its coded rows from the main
input. Full codes select the longest code for each commit text and keep the first
encountered code on equal length.

Phrase construction shares the existing add-word routine through a lookup callback,
preserving punctuation removal, grapheme boundaries, ignored unknown characters,
ASCII-letter fallback and the two/three/four-plus-word rules. The mapped-dictionary
add-word API still uses that same routine and passes its 633 original cases.

`tests/construct_import_parity.py` passes 212 cases against the original snapshot
builder, comparing main entries in insertion order and construction/full maps by
key. This includes absent/empty/overriding construction files, short explicit codes,
aliases, Unicode graphemes and the full row volume from all five real main-table
files. Test-supplied concatenation order is not proof of the pending filesystem
precedence stage. Totals across fixtures: 177,572 main entries, 106,092 construction
entries and 240,745 full-code entries; zero differences. Evidence:
`build/construct-import-parity-arm64.json`.

The reference output uses hexadecimal UTF-16 code units. The original can create
unpaired surrogates when taking one-unit prefixes from supplementary-letter codes;
ordinary .NET JSON string serialization replaced those with U+FFFD and initially
produced four false mismatches. The test now compares original code units verbatim
instead of normalizing either implementation's output. Construction maps precede
adjustment replay in the original, while full-code maps are rebuilt afterward.

`applyImportedAdjustments` now replays `{添加}`, `{删除}`, `{置顶}` and `{前移}`
in file order. Payloads split on tabs, ignore empty fields and retain literal
spaces/comment markers. Matching uses exact commit-text identity, preserving an
existing display alias on moves. Add moves the first matching candidate to the
end; top moves it to the front; advance moves it one place; delete removes all
matching aliases. Empty exact-code entries remain present. The caller still parses
ordinary non-directive rows in the adjustment file as part of the regular file
sequence; the test fixture explicitly does so before replay.

`ImportedLexicon.indexedMain` and `quickSymbols` now contain the main dictionary's
derived metadata: sorted ordinal record keys, unique/nonterminal/automatic-symbol/
exact-source flags, and the four enabled-short-symbol bits. Records reference source
entry indices rather than copying their candidate arrays. This also retains empty
source keys and case-equivalent prefix spellings, as in the original exporter.
Prefix construction excludes empty candidate lists, while short-symbol inventory
still observes their source keys; those distinctions are covered by tests.

The expanded `construct_import_parity.py` passes 364 original snapshot comparisons,
including 152 fixtures with adjustment-file content or metadata boundaries and the
full real table volume. Totals: 178,567 main entries, 106,610 construction entries,
241,687 full-code entries and 187,734 indexed records. Candidate order, raw UTF-16
units, all four record flags and quick-symbol bits have zero differences. Report:
`build/construct-import-parity-arm64.json`. Filesystem precedence remains outside
this explicitly supplied row-order test.

The comparison found that Windows `CompareStringOrdinal` differed from the
original .NET ICU behavior for final sigma and micro sign. `OrdinalCase.h` now
uses simple ICU uppercase while preserving U+0131 and U+017F, following the
[.NET 10 ordinal-casing implementation](https://github.com/dotnet/runtime/blob/v10.0.0/src/native/libs/System.Globalization.Native/pal_common.c).
Schema catalog, MRU metadata, service loading and the selector share this rule;
the selector additionally verifies `ς方案` resolves to a prepared `Σ方案` directory
and that equivalent spellings do not occupy both recent slots.

The parser covers plain text and `.dict.yaml` body gating, all three standard
line endings, Unicode edge trimming, full/inline comments, odd/even backslash
escaping of `#`, and skipping adjustment directives. A space-separated likely
code token selects code-first parsing, potentially yielding multiple candidates;
the presence of a tab instead selects the original word-first rules. Word-first
rows support both code/frequency orders and preserve uncoded rows for later
phrase-code inference. Integer parsing retains the original overflow fallback.
Entry escapes and display/commit aliases reuse the already validated add-word
decoder; code-first normalization uses its invariant Unicode casing.

`tests/lexicon_rows_parity.py` invokes the unmodified original private
`ParseMbFile` method through the offline oracle. Fixtures are written only to a
disposable marked staging root. The ARM64 native parser matches 1,234 cases:
synthetic format/boundary cases plus all five main-table files in the reference
staging schema. Together these produce 253,280 coded and 198 uncoded rows, with
zero differences in codes, packed text, frequencies or row order. Evidence:
`build/lexicon-rows-parity-arm64.json`; build project:
`tests/LexiconRowsProbe.vcxproj`. All 34 original source hashes remain unchanged.

Auxiliary content import now includes pinyin, comments and splits. Pinyin uses
the coded-row parser and stable merge, ignoring uncoded entries. Annotation rows
use only the first two space/tab-separated fields and the original escape decoder,
without display-alias interpretation or inline-comment stripping. Comments append
with one separating space (including duplicates); splits replace earlier values.
Keys remain ordinal case-sensitive. Callers supply source-file encounter order.
`auxiliary_import_parity.py` passes 181 original-loader comparisons, including all
four actual auxiliary files: 39,654 pinyin groups / 68,205 candidates, 105,948 comment
entries and 105,054 split entries across fixtures, with zero differences. Extracting
the shared escape decoder also retains all 633 add-word cases; current report
hashes match their ARM64 probes. The rebuilt DLL passes alternate-schema private
activation (zero keyboard events), with installed registration unchanged.

`LexiconSerialize.h/.cpp` serializes all six imported sections into v2 bytes,
sorting records by ordinal UTF-16 keys and interning strings across sections.
Candidate order and raw UTF-16 units are preserved. Main record source indices
avoid copying source candidate strings; serialization allocations are confined
to the import utility. Size arithmetic and v2 count limits are checked, and invalid
keys, flags, source indices and duplicate keys are rejected. This API returns
bytes; it does not yet publish a generation or modify an existing dictionary.

`tests/lexicon_serialize_test.py` verifies the ARM64 serializer against the existing
41,897,112-byte original export, plus independently generated boundary and minimal
fixtures. All three outputs are byte-identical; the native mapped reader also checks
every record and candidate after writing. Inputs are deliberately reversed before
serialization to check sorting. Fixtures cover empty exact keys' candidate lists,
synthetic prefixes, duplicate/shared strings, empty values, embedded NULs and
unpaired surrogate units. Five invalid-input categories are rejected per fixture.
Evidence: `build/lexicon-serialize-arm64.json`. This isolates binary-format fidelity;
the complete original-files-to-binary pipeline is still awaiting integration.

`LexiconDecode.h/.cpp` now provides the raw-byte decoding layer, matching original
`DetectTextEncoding` followed by .NET 10 `StreamReader` decoding. Both the UTF-8
detector's positive result and its `Encoding.Default` fallback decode as UTF-8 in
this reference runtime. BOMs select UTF-16 LE/BE or UTF-32 BE, with replacement
fallback for malformed/truncated sequences. The original UTF-16 LE preamble takes
precedence for `FF FE 00 00`; those last two bytes remain a NUL rather than selecting
UTF-32 LE. Repeated BOMs beyond the leading preamble remain content.

`tests/lexicon_decode_parity.py` passes 2,292 ARM64 comparisons, including all 256
individual byte values, malformed UTF-8/scalars/surrogates, random BOM-prefixed
inputs, 1 KiB/4 KiB reader boundaries and all nine real main/auxiliary files.
The oracle calls the unchanged original detector and .NET file reader on disposable
staging files; output comparisons preserve exact UTF-16 units. Zero differences;
evidence: `build/lexicon-decode-parity-arm64.json`. All 34 reference source hashes
remain unchanged. `readLexiconText` now reads actual binary files through this
decoder, supports native Unicode filesystem paths and reports missing/non-regular
files and failed reads rather than treating them as empty tables. The updated
decoder test repeats all 2,292 cases through actual files under a Chinese filename,
and verifies missing-path and directory rejection. It does not yet test mid-read
device failure or a source being modified concurrently.

`LexiconFile.h/.cpp` connects file reading to `parseLexiconRows`, deriving YAML
body gating from the original case-insensitive `.dict.yaml` suffix rule. Callers
still supply the file sequence; directory enumeration and culture-sensitive
precedence have not yet been connected.

The expanded `lexicon_rows_parity.py` also writes all 1,234 parser fixtures to
Chinese filenames, cycling UTF-8, UTF-8 BOM, UTF-16 LE/BE and UTF-32 BE, then invokes
the file parser with mixed-case `.DiCt.YaMl` extensions. Both supplied-text and
actual-file paths match the original parser: 253,280 coded and 198 uncoded rows,
zero differences. Report `build/lexicon-rows-parity-arm64.json` records the separate
file-case count and the current ARM64 executable hash. These fixtures cover file
reading plus parsing, not the remaining directory precedence or full binary import.

`LexiconOrder.h/.cpp` now implements Windows top-level file enumeration and main
table ordering. It concatenates TXT files followed by `.dict.yaml` files in native
enumeration order, promotes names matching the directory's schema name with the
shared ordinal-ignore-case rule, then uses stable ICU culture collation. Directories
and unrelated extensions are excluded; missing directories report an error. The
caller supplies its current culture explicitly (empty means invariant).

`tests/lexicon_order_parity.py` compares actual Windows directory results with the
unchanged original `GetOrderedLexiconFiles`. Forty cases pass across invariant,
en-US, zh-CN, zh-TW, sv-SE, tr-TR, de-DE and ja-JP, including real staging filenames,
empty directories, schema-name priority, mixed-case extensions, composed/decomposed
accents, soft hyphens and other collation ties, CJK and supplementary characters.
Report: `build/lexicon-order-parity-arm64.json`. All 34 reference hashes remain
unchanged. This verifies ordered paths, not their integration into complete snapshot
construction; construction/sentence-file exclusions and auxiliary enumeration still
need connecting to the directory importer.

`LexiconDirectory.h/.cpp` now connects ordered directory paths, actual file decoding
and parsing to main snapshot construction. Dedicated `构词.txt` rows are excluded
from ordinary main input and supplied separately for construction and inference.
`补充语料.txt` is excluded from ordinary input. Non-directive rows in `用户调整.txt`
participate at that file's actual sorted position, while adjustment directives
replay after the main/construction build. Full-code and main-index metadata are
then derived through the existing pipeline.

`tests/lexicon_directory_parity.py` passes 17 complete main-snapshot comparisons
against the original `BuildLexiconSnapshot`, including the actual tiger schema
directory and synthetic multi-file directories under four cultures. Tests combine
schema-name priority, TXT/YAML ties, three file encodings, construction override/
absence/empty-file behavior, uncoded inference, adjustment ordinary rows and
directives, plus sentence-file and nested-directory exclusion. Main group/candidate
order, construction/full maps, indexed records and quick-symbol flags all agree:
171,205 main and 172,136 indexed entries across cases. Evidence:
`build/lexicon-directory-parity-arm64.json`. Auxiliary pinyin/comment/split fields
are still empty in this main-directory API and require integration separately.

`importLexiconDirectory` now fills all six sections. The caller supplies the main
schema directory, pinyin directory and current culture. Auxiliary files retain
native Windows enumeration order instead of main-table culture sorting. Pinyin
accepts top-level TXT coded rows, then performs the original stable frequency merge;
uncoded entries and YAML are ignored. Comments concatenate and splits overwrite in
encounter order. Missing pinyin directories yield an empty pinyin map. The shared
`enumerateLexiconFiles` also preserves the existing main-order behavior.

`lexicon_directory_parity.py --all` passes 17 comparisons against complete original
snapshots, including real main and auxiliary files and synthetic multi-file
annotations/splits/pinyin. All six map contents, candidate/insertion order and main
metadata match exactly. Special `.txt`, `.注释` and `.拆分` filenames are included;
suffix-only files must not be lost by relying on `filesystem::path::extension`.
Evidence: `build/lexicon-all-directory-parity-arm64.json`. Main-directory ordering
has a separate 40-case regression after extracting shared enumeration. The optional
missing-pinyin branch is not separately covered by this integration suite yet.

`tools/lexicon_import.cpp` and `tools/LexiconImport.vcxproj` now provide a short-lived
native ARM64 import command. It connects directory loading, six-section snapshot
construction, v2 serialization and immutable publication. All directory/output
arguments are absolute Windows paths; culture is explicit (empty for invariant).
For example, in Windows PowerShell after building the project:

```powershell
& .\build\tests\ARM64\Tigirl.Import.exe `
  'C:\Users\yc\Desktop\ime\data\staging\码表\虎码字词' `
  'C:\Users\yc\Desktop\ime\data\staging\拼音反查码表' `
  'C:\Users\yc\Desktop\ime\build\imported\tiger-v2.tcd' 'zh-CN'
```

The output must not already exist. `LexiconPublish` serializes before writing,
creates a GUID-named temporary file beside the destination, writes and flushes it,
then validates it using the production mapped reader. `MoveFileExW` without the
replace flag publishes the new generation; exceptions clean owned temporary files.
The command does not select a schema, change registration or overwrite a generation.

`tests/lexicon_publish_test.py` passes the full real-files-to-binary path: output is
byte-identical to the 41,897,112-byte original export (SHA-256
`7fe93ae1f18b51f4b0db6f0d5468ee8ff3741c699e4701a15b94fe3b569acc43`).
Four concurrent publishers targeting one path yield exactly one success; a preexisting
destination remains unchanged and all owned temporary files are removed. Empty main
input is rejected; missing optional pinyin succeeds with an empty pinyin section.
Evidence: `build/lexicon-publish-arm64.json`. These tests cover normal publication
and name conflicts, not injected disk-write/flush faults or power-loss recovery.

The importer also supports named schema creation:

```text
lexicon_import --schema <absolute-source-schema-dir> <absolute-pinyin-dir> <absolute-user-root> <new-schema-name> <culture>
```

It publishes to `<user-root>/schemas/<name>/tiger-v2.tcd`, making the result directly
discoverable by `schema_select --list` and selectable with the existing selector.
It shares schema-name validation with the runtime and rejects the reserved builtin
name and nonempty existing ordinal-case-equivalent directories. A stable `.import.lock` in
the schema root serializes named imports, including Unicode-equivalent names which
Windows itself may treat as distinct directory names. Import does not change the
current selection; journal storage remains at the existing per-schema location.

The expanded publish test verifies named import followed by catalog listing and
actual selector validation/configuration publication in a disposable user root.
Four rejected names preserve the selected configuration and binary, and concurrent
`Σ方案`/`ς方案` imports produce one selectable scheme (also selectable as `σ方案`).
The complete real-file binary and immutable publication regressions still pass.
Report: `build/lexicon-publish-arm64.json`. The creation mode refuses existing
nonempty schemes; the separate update mode is described below. An empty ordinary directory left after failed publication can
now be reused on retry, using its canonical spelling even for Unicode aliases.
Nonempty directories, files and reparse points are rejected without modifying their
contents. Tests recover an empty `Σ恢复` directory through `ς恢复`, then successfully
select it as `σ恢复`; user-journal and interrupted-temporary-file fixtures remain
byte-identical after rejected retries, with selected configuration unchanged.
This is recovery from an empty leftover directory; nonempty crash artifacts still
require an explicit recovery workflow. No disk-fault or process-kill injection is
claimed by these fixture-based tests.

`lexicon_import --update` accepts the same arguments as `--schema`, validates an
existing named scheme, and publishes a fresh GUID generation under
`schemas/<name>/generations/<32-lowercase-hex>/tiger-v2.tcd`. It validates/replays the
existing `user.tcu` against the new mapping before publishing `current.txt` with
`generation<TAB><id>`. Descriptor writes use the configuration store's stable lock,
flush and atomic replacement. Prior binaries are retained. Failed journal validation
leaves the old descriptor selected and may retain an unselected new generation.

Catalog, selector and TSF resolve this descriptor through `schemaDictionaryPath`;
without it they continue using the legacy direct binary. Invalid descriptors fail
rather than silently loading the legacy file. On settings/focus reload the TSF
also checks same-name dictionary identity and prepares a new store if it changed.
There is no immediate cross-process update notification yet.

`tests/schema_generation_test.py` passes two successive native updates with old-file
retention, actual selector loading, corrupt-journal rollback and four malformed/
missing-generation descriptor failures. Report: `build/schema-generation-arm64.json`.
Named publication and selector regressions pass. Rebuilt ARM64 DLL private activation
passes with zero key events and unchanged registration. Actual TSF composition during
same-name generation updates and live-reader/concurrent-update stress remain unverified.

The subsequent live-reader check now passes with four ARM64 readers, six concurrent
updates and 1,118 reads (`build/schema-generation-concurrency-arm64.json`). Old
mappings remain stable while every newly resolved mapping is a complete expected
generation. Actual DLL activation from a generation descriptor also passes
(`build/tsf-private-schema-activation-generation-validation.json`), including mapped
filename verification while the legacy binary still exists. This does not prove
composition refresh during active typing; the Windows foreground window remains
unavailable for that test. See `SCHEMAS.md` for exact scope and fixture corrections.

Remaining work includes a packaged importer/settings UI and further schema generation
update lifecycle validation/recovery. The existing selector can now consume natively imported named
schemes directly. This completes neither
the packaged user flow nor the full ordinary-input acceptance scope.


Retained-version listing and recovery now use `schema_select --versions` and
`schema_select --restore` (full syntax and evidence in `SCHEMAS.md`). Restore supports
validated generation IDs or `legacy` and repairs malformed descriptor encoding
without changing current-schema selection/MRU. Actual active-input restoration and
GUI integration remain unverified/unfinished.
