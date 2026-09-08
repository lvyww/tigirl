# Candidate visibility action and configuration writes

The complete dictionary entry `{隐藏候选}` toggles candidate-item visibility.
As in the original, it commits no literal text and leaves repeat/history alone.
Aliases and a random-set choice yielding that token reach the same action.
The pure engine only returns `toggleHiddenCandidates`; previews cannot change
settings. Successful real TSF dispatch applies the change and rebuilds the
candidate UI with the new style. The code line follows the existing show-code
option independently of candidate-item visibility.

`ConfigStore.cpp` serializes cooperating Windows TSF hosts with a stable
`config.txt.lock` sidecar. Each action reads the latest persisted setting under
the lock, toggles it, replaces previous recognized 隐藏候选 rows, and writes a
complete UTF-8-with-BOM file. Other lines/comments/unknown settings are retained;
UTF-16/32 input is decoded through the common bounded reader. A temporary file
in the same directory is flushed before publication. Output remains bounded to
4 MiB, matching the reader. Windows text readers share deletion access so a
cooperating reader does not prevent replacement of the file it is reading.
Text-file opens retry transient missing/sharing/access errors for at most
100 ms. TSF focus reload also takes the existing sidecar shared lock, avoiding
a false missing-file/default-settings observation during replacement. Reading
a genuinely missing initial configuration creates no directory or lock file.

Existing files use ReplaceFileW with a recovery backup and without flags that
ignore ACL-merge errors. This API preserves the original DACL and file attributes
as documented by Microsoft:
https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew

A failed replacement may have moved the original to its backup. The writer
attempts to restore that backup if the target is missing and retains recovery
files on uncertain failure. A failed first publication does not replace a file
created meanwhile. Successful writes remove their temporary/backup artifacts;
the lock file remains. Power-loss recovery and every filesystem-specific failure
mode are not proven by the current tests. Editors that ignore the sidecar are
not serialized with this protocol.

If persistence fails, the current service still toggles its in-memory visibility
and reports the failure through debug output/a beep, matching the original's
immediate UI-toggle behavior. A later native focus reload may restore the disk
value; durable retry/user-facing recovery remains pending. Other hosts refresh
on focus; live configuration notifications remain future work. The native action
does not perform the original Core's unrelated autorun synchronization.

## Evidence

`tests/config_store_test.py`, with Release/ARM64 ConfigStoreProbe:

- nine UTF-8/16/32 cases preserve comments/unknown settings, collapse duplicate
  setting rows and toggle in both directions;
- four independent processes perform 52 toggles while two reader processes
  perform 2,000 complete-file reads; no transition or unrelated setting is lost;
- a reader denying replacement produces an error and preserves the original;
- malformed input and an update exceeding the output-size bound preserve the
  original; successful writes leave no temporary files behind.

Results are in `build/config-store-arm64.json`. DynamicEngineProbe adds six
keyboard/public-selection cases covering literal, nested and aliased actions,
including copied previews and no history/text leakage, on Linux and ARM64.
See `build/dynamic-engine-*.json` for those results.

Build/activation checks do not prove actual application candidate visibility.
The full TSF typing/rendering suite and installation of the new generation remain
pending. This action does not complete the settings UI, schema switching, shared
history, timer behavior, or the ordinary-input goal as a whole.
