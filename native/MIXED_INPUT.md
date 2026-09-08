# Ordinary mixed input

This feature is in progress. `MixedInput.h` implements the original Core's
`FixedLengthMixedInputDecoder` semantics. The engine now reads
`中英文不限长混合输入` (default 否), retains raw input and preferred page choices,
and connects editing, selection, punctuation and language transitions. TSF and
candidate presentation use the resolved surface while snapshots retain raw keys.

The authoritative input is the full raw code. For maximum code length M, a
segment remains active at exactly M characters and becomes a completed prefix
only when the next character arrives. A completed segment uses its preferred
candidate output, otherwise its cached first candidate output, otherwise its
original code (including ASCII case). The last segment remains raw for editing.
Chinese composition concatenates the resolved prefix, selected active output
and punctuation suffix; English/raw composition returns the original keys.

The cache includes empty results, compares ordinary ASCII codes without case,
and resets on lexicon revision or explicit clear. Decoder copies own their
cache. Resolvers are supplied per call so a copied Engine cannot retain a
callback pointing into the original Engine. Integration retains full raw input
and preferred page choices separately, discards choices when backspacing into
their segment, and uses a new revision when lexicon data changes.

Linux and Windows ARM64 each match 1,931 stateful original-decoder cases:
incremental typing and deletion, maximum lengths -1/0/1/2/4/16, preferred and
empty candidates, repeated codes with different case, cache clearing/revisions,
random sequences, copied previews, and a 16,385-character input. Synthetic
resolver call counts are compared too, so equal output alone cannot conceal
extra resolutions or missing invalidation. These are decoder checks, not
keyboard or actual-application parity. The original source snapshot is unchanged.

Build `tests/MixedProbe.vcxproj` in Release/ARM64 and ReferenceOracle in Release.
For Linux:

```
g++ -std=c++17 -Wall -Wextra -Werror -O2 -Inative native/Text.cpp tests/mixed_probe.cpp -o build/mixed_probe
python3 tests/mixed_parity.py
python3 tests/mixed_parity.py --replay --windows
```

Reports: `build/mixed-parity-linux.json`, `build/mixed-parity-arm64.json`.
The oracle initializes only a disposable filesystem/registry sandbox.

`tests/mixed_key_parity.py` compares three settings profiles (default mixed,
two-character codes/one candidate per page, Enter-clear/Tab-pass) against the
original engine. Each architecture passes 39,474 key events, comparing handling,
commit, raw input, resolved surface, mode, page, candidates and annotations.
The corpus includes punctuation, uppercase continuation, CapsLock, language
switching, raw Enter, cross-segment backspace, page selection and word adjustment.
Default non-mixed regression passes 35,554 events per architecture.

Run `python3 tests/mixed_key_parity.py`, then
`python3 tests/mixed_key_parity.py --replay --windows` after rebuilding both
engine probes. Reports are `build/mixed-key-parity-*.json` and
`build/mixed-default-regression-*.json`.

The current DLL passes private TSF host checks in default and mixed modes.
Run `tests/run_mixed_tsf.ps1` in Windows PowerShell after building the DLL and
`tests/TsfHost.vcxproj`. It creates a disposable user-data directory and selects
mixed input with maximum code length two. Both UI-less and native-popup runs
exercise 76 events across two contexts through real TSF edit sessions. Added
checks assert the decoded prefix stays in one composition, backspace reopens
the previous segment, candidate finalization preserves the prefix, Enter outputs
raw keys and Escape removes the whole preedit. Native-popup mode clicks the
mixed candidate; UI-less mode finalizes its second candidate.

`build/tsf-private-mixed-validation.json` records the actual DLL/test hashes,
isolated user root and unchanged registration. The runner restores its process
environment and removes the disposable directory. Default host regression also
passes (36 events per mode). These controlled-host checks do not establish
behavior in arbitrary applications. Hot configuration changes, dynamic candidate
cache behavior, broader custom selection profiles and application checks remain.
