# Dynamic dictionary output

`DynamicText.cpp` implements the original Core's whole-entry substitutions:
`{日期}`, `{日期.}`, `{日期-}`, `{日期/}`, `{时分秒}`, `{时分}`, `{星期}`,
`{周}`, and random sets such as `{甲|乙|丙}`. Dates use the local Gregorian
clock; 星期 uses the Windows user's locale and 周 uses fixed Chinese names.
Tokens embedded in ordinary text are literal. Unknown tokens remain literal.

Random sets ignore empty entries, preserve whitespace and duplicate entries,
and select a position uniformly. They do not implement numeric ranges or
recursive parsing. Conversion happens once when resolving a candidate and
again when normalizing the final commit, as in the original key protocol.
Thus a random choice that yields a date token can expand on commit. Display
aliases retain their literal display label while their output is converted.

Each engine owns a small, noncryptographic PRNG. Engine copies retain its state,
so TSF key previews and discarded edit transactions cannot advance the actual
engine's sequence. Candidate display and committed random values can differ;
the original also converts them separately. Exact random sequences are not
claimed identical to .NET Random.

Keyboard commits and public candidate selection both normalize output and update
repeat/history/decimal state. The latter is used by mouse and UI-less selection.
An integration regression initially failed because public selection omitted this
postprocessing; the fix makes repeating a clicked dynamic candidate work.

## Evidence

`tests/dynamic_parity.py` requires both Linux and ARM64 DynamicProbe and
DynamicEngineProbe builds. It runs the untouched original conversion method in
an isolated oracle, compares 20 token cases (allowing before/after clock bounds
and checking random-set membership), and verifies 168 fixed-clock cases and
64 random draws. The engine probe separately exercises 34 keyboard/selection
cases, including aliases, repeat and nested tokens, plus 64 copied-preview cycles.
Results are in `build/dynamic-parity-*.json` and `build/dynamic-engine-*.json`;
reports record probe hashes.

The existing 35,554-event default trace also replays without differences on
Linux and ARM64 (`build/dynamic-key-regression-*.json`). This is a regression
against the previously oracle-verified trace, not a new exhaustive key oracle.

ARM64 DLL build, package preflight, and registration-free TSF activation pass.
Activation proves startup and dictionary mapping, not actual typing or rendering.
The built DLL has not replaced the installed generation. Full application checks
remain pending. Add-word and hide-candidate actions are now connected; see `ADDWORD.md` and
`CONFIG_STORE.md`. Manual timer behavior, and Unicode text-element history remain separate work.

## Build and run

From the workspace, build `tests/DynamicProbe.vcxproj` and
`tests/DynamicEngineProbe.vcxproj` with MSBuild Release/ARM64, then build Linux:

```
g++ -std=c++17 -Wall -Wextra -Werror -O2 -Inative native/DynamicText.cpp native/Text.cpp tests/dynamic_probe.cpp -o build/dynamic_probe
g++ -std=c++17 -Wall -Wextra -Werror -O2 -Inative native/Dictionary.cpp native/Lexicon.cpp native/Text.cpp native/DynamicText.cpp native/UppercaseText.cpp native/Engine.cpp tests/dynamic_engine_probe.cpp -o build/dynamic_engine_probe
python3 tests/dynamic_parity.py
```

The reference oracle must also be built from its current wrapper before running.
