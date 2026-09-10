# Current folder-based workflow

The normal manager now lists text-table folders and automatically prepares shared
binary caches when selecting/reloading a schema. See [folder workflow](docs/FOLDER_SCHEMAS.md)
for current behavior and validation. The incremental notes below record earlier
implementation stages; their “unfinished” statements are historical.

# Native schema loading (in progress)

The native service reads `当前码表` from the per-user `config.txt` on activation
and when TSF document/thread focus returns. Blank or absent selects `虎码字词`.
The built-in schema still uses the DLL-adjacent `tiger-v2.tcd` and the existing
`user/tiger-words.tcu` journal, preserving installed user data.

Prepared additional schemas currently have this layout under the user root:

```
config.txt
schemas/
  SchemaName/
    tiger-v2.tcd
    user.tcu
```

Set `当前码表 SchemaName` to select one. The table must already be a validated
native v2 binary. This is an interim configuration interface; original text-table
import and schema selection UI remain unfinished.
The default root is `%LOCALAPPDATA%\Tigirl`; isolated tests override it with
`NATIVE_TIGER_USER_ROOT`. Secure-mode activation ignores user schemas.

The short-lived ARM64 native selector provides a validated configuration update:

```
Tigirl.SchemaSelect.exe "<absolute-user-root>" "<absolute-DLL-directory>\tiger-v2.tcd" "SchemaName"
```

Build `tools/SchemaSelect.vcxproj` with Release/ARM64; the executable is currently
`build/tests/ARM64/Tigirl.SchemaSelect.exe`, not yet installed by the package scripts.
It resolves the canonical directory spelling, validates/maps the binary and
replays its journal before publishing configuration, then exits. Unicode command
arguments use the Windows wide-character entry point. Missing or corrupt targets
leave configuration unchanged. Publication keeps the current name and two-entry
case-insensitive recent list in one replacement, sharing the existing configuration
lock with candidate visibility updates. It reads the latest current name under
that lock; a missing current name means the bundled schema. This records recent
choices. Pass `--list` instead of a name to list prepared schemas, or `--recent`
to select the most recent other schema. The latter resolves its target from the
latest configuration under the same lock that publishes its selection; concurrent
callers do not all toggle from an obsolete snapshot. If no recent entry exists,
it uses the next sorted schema (or the first when the current name is absent).
An incomplete directory without `tiger-v2.tcd` is not offered.

The DLL now has the recent-schema shortcut wired to this transaction. Enable
`Ctrl+m切换最近码表 是`; `切换最近码表快捷键` defaults to `Ctrl+VK_M` and supports
the original configurable action gestures. Conflicts with reserved shortcuts are
disabled; an add-word/recent gesture collision disables both actions. No second
schema means normal pass-through processing. Successful switching keeps the full
composition and suppresses further schema/add-word actions until modifier release.
Focus recovery also clears that suppression. Preview only reads catalog/configuration;
target loading and publication happen in the real synchronous TSF edit session.
A failed transaction does not latch the switch and falls back to ordinary chord
processing. Component and configuration tests pass, but real keyboard/TSF shortcut
validation is still pending the full foreground test described below.

Each schema uses an independent journal. The target dictionary and journal are
opened and replayed before the service replaces its active resources. Invalid
names, missing/corrupt tables or journal failures preserve the previously loaded
schema and engine settings. If initial selection fails, the service attempts the
built-in schema and its journal. Errors currently go to debug output; a user-facing
load-error display is still needed. An add-word dialog is closed before replacing
its borrowed store.

Existing contexts move to the selected lexicon when they regain focus. If TSF
still owns their composition, a write edit session refreshes the composition from
its current engine state; a delayed callback does not restore a captured stale
key buffer. Shared binary dictionary views remain read-only. Older contexts can
retain an old mapping until they switch or are destroyed; this is not a private
copy of the full table. Live updates to the same schema binary and immediate
cross-process notification are not implemented yet.

## Evidence and limits

- `tests/schema_recent_test.py`: prepared catalog, single-schema no-op, canonical
  MRU selection, missing-current/MRU fallback, 16 simultaneous native selector
  commands and corrupt-target preservation pass. `build/schema-recent-arm64.json`.
- `tests/schema_shortcut_probe.cpp`: Linux and ARM64 checks for preview isolation,
  preserving composition, auto-repeat, held-modifier rollover, release/focus
  rearming, unavailable pass-through and shortcut conflicts pass. Reports:
  `build/schema-shortcut-{linux,arm64}.json`. Linux ordinary key differential
  regression passes 35,554 events; ARM64 add-word regression passes 20,460 events.
- `tests/schema_config_test.py`: five ordered selections, Unicode trimming/case
  deduplication, 124 concurrent schema selections plus 34 visibility toggles,
  2,000 coherent reader snapshots, denied replacement and seven invalid names.
  `tests/config_store_test.py` also passes after sharing the mutation path.
- `tests/schema_select_test.py`: three successful native selections, canonical
  Unicode directory spelling and four rejected selections (missing/path traversal,
  bad binary, bad journal) without configuration changes. Reports:
  `build/schema-config-arm64.json` and `build/schema-selector-arm64.json`.
- `tests/switch_composition_parity.py`: 11,040 original-engine comparisons per
  architecture for composition refresh. This is component evidence.
- `tests/run_schema_tsf.ps1 -ActivationOnly`: private activation of the actual
  ARM64 DLL confirms its mapped view names the alternate schema file, rather
  than the bundled dictionary. Report:
  `build/tsf-private-schema-activation-validation.json`. No key events are sent.
- `tests/run_schema_tsf.ps1`: fixture for UI-less and native UI input across two
  documents, distinct per-schema user words, switching back, and missing/invalid
  target recovery. It now also checks Ctrl+M preview without config writes,
  preservation of active composition, switching in both directions and suppression
  of held-modifier repeat/add-word rollover. The last full attempt failed before event 0 because the Windows
  foreground window was unavailable. Input and composition integration remain
  unverified until this full test passes. Its failed report is retained separately.

Both runners use disposable directories and private COM activation; they do not
register the new DLL or modify the daily TigerClaw runtime.

Named schemas now support immutable generation updates via `lexicon_import --update`
(see `native/IMPORT.md`). `current.txt` holds `generation<TAB><32 lowercase hex digits>`;
the selected binary is `generations/<id>/tiger-v2.tcd`. A missing descriptor retains
the legacy `tiger-v2.tcd` layout; an invalid descriptor is an error. Journals remain
at schema-level `user.tcu`, and old binaries are retained. TSF settings/focus reload
checks dictionary identity even for an unchanged schema name. Two CLI update cycles,
selector resolution and failure rollback pass; actual same-schema TSF input and
concurrent live-reader updates still require dedicated validation.

Live-reader update validation now passes: `schema_generation_concurrency.py` keeps
four actual ARM64 processes' original mappings alive while six concurrent importer
processes publish generations. Across 1,118 reads all original candidates remain
unchanged, every freshly resolved mapping contains a whole expected generation,
and all readers finish on the same selected version. Six published files and the
legacy file remain intact. This small-dictionary stress test does not measure full
TSF host memory or input latency. Evidence:
`build/schema-generation-concurrency-arm64.json`.

`tests/run_schema_tsf.ps1 -ActivationOnly -Generation` now verifies the actual TSF
DLL maps the descriptor-selected generation, with the legacy binary still present.
The host checks the mapped Windows filename independently of the resolver. The
fixture's initial mixed path separators caused a false mismatch; the expected path
now uses native path components. Successful report:
`build/tsf-private-schema-activation-generation-validation.json`. No keyboard events
are sent. Foreground inspection still returns HWND 0, so active-composition update
validation remains open. The runner uses disposable data and preserves registration.


Version recovery is available through the native selector:

```text
schema_select <absolute-user-root> <absolute-bundled-dictionary> --versions <schema>
schema_select <absolute-user-root> <absolute-bundled-dictionary> --restore <schema> <generation-or-legacy>
```

Listing validates each retained binary, excluding corrupt/incomplete files, and is
independent of the current descriptor. `legacy` identifies the original direct
`tiger-v2.tcd`. Restore validates the requested mapping and replays the schema journal
before atomically replacing the dedicated descriptor. It does not change the global
schema selection or MRU. The explicit `generation<TAB>legacy` descriptor is understood
by both selector and TSF. Dedicated descriptor replacement can repair malformed
encoding without first parsing the damaged text; ordinary user configuration still
uses read-modify-write and retains its existing error behavior.

The expanded `schema_generation_test.py` passes two restores from damaged descriptors
(including malformed byte encoding), two update cycles, validated listing and four
failed restores that preserve the descriptor. Corrupt, missing and traversal targets,
and bad user journals, are rejected. ARM64 build and generation private activation
also pass. Evidence: `build/schema-generation-arm64.json`. Restoration during active
TSF composition, GUI recovery and cleanup of unselected files remain pending.


A native Win32 manager is now built by `tools/SchemaManager.vcxproj`:

```text
Tigirl.exe <absolute-user-root> <absolute-bundled-dictionary>
```

Keep `Tigirl.SchemaSelect.exe` and `Tigirl.Import.exe` beside the manager. Its window
provides schema selection, folder pickers, named import/update, retained-version
listing and restore. Import/update uses the Windows user's current locale. Commands
run in a worker thread through explicitly located companion executables, with
Windows argument quoting and captured output; controls disable during work, and a
close request waits for completion. The manager includes incomplete schema directories
so the recovery actions remain reachable when the selected descriptor is damaged.

`tests/schema_manager_test.py` creates the real window hidden in a marked disposable
root, dispatches the Use command through its window procedure and waits for the
actual background selector/completion message. Successful configuration publication,
Unicode/space paths, corrupt-target preservation and missing-companion failure pass.
Unmarked roots cannot invoke this test mode. Report: `build/schema-manager-arm64.json`.
The test does not prove visual layout, interactive folder picking, the remaining
button flows, DPI behavior or closing during work. Those checks, installation/launch
integration and broader settings UI remain pending. No manager was installed or
launched against the daily user root.


The expanded hidden-window manager test now also dispatches Import and Update,
verifies their actual published files/descriptor, then loads the version combo and
uses its selected entry to Restore the original version. Import/update/restore do
not change the global selected schema. A close request sent synchronously while a
Use worker is active waits for completion and preserves the completed selection.
All checks pass in `build/schema-manager-arm64.json`; folder-picker interaction,
visible layout and DPI behavior remain unverified. Background-thread construction
failure now restores enabled controls instead of leaving the window permanently busy;
that exceptional allocation path is corrected by inspection, not fault injection.


Version choices now show local file-write time, an eight-character distinguishing
ID and a current-version marker, sorted newest first. The initial binary is labeled
“最初导入的版本”. Labels and full generation IDs are stored separately; Restore
uses the ID associated with the selected combo index, never parses display text.
Schema changes clear both the displayed choices and their ID bindings. A missing
file timestamp displays “时间未知” while still requiring normal backend validation
before restore. The expanded real-window test makes two updates, restores the
initial binary, then selects the newest formatted entry and verifies the exact
resulting generation ID. Prior GUI/error/close tests still pass. This remains
hidden-window event-flow evidence, not visual/DPI validation.


The manager now enables per-monitor-v2 DPI awareness, scales its font and logical
control rectangles, sizes the client area using the actual nonclient DPI and handles
`WM_DPICHANGED` with the suggested window rectangle. Hidden-window tests exercise
96/144/192 DPI content layouts and check actual control bounds against the client
area plus exact font height, then restore the window's real DPI before button tests.
The first simulation incorrectly used the simulated content DPI for the actual
window frame, producing a smaller-than-requested client area; using the window's
actual frame DPI fixes this. All manager operation tests continue to pass. Evidence:
`build/schema-manager-arm64.json`. Physical monitor transitions, text rendering and
interactive visual inspection remain unverified; this is geometry/event-flow evidence.


`build_arm64.ps1` now also builds the three management tools and copies their exact
executables beside the DLL and dictionary in `build/ARM64/Release`. Installation
includes their hashes in the immutable package generation and validates every DLL/
EXE as ARM64. `Tigirl.exe` supports no-argument startup: dictionary and
companion tools are resolved beside the executable, with the normal
`%LOCALAPPDATA%/Tigirl` user root (or the existing `NATIVE_TIGER_USER_ROOT`
override). Explicit-path launch and marked test modes remain available.

`tests/package_arm64_test.py` passes read-only installer preflight, verifies staged
executables match build outputs, rejects a deliberately substituted x64 manager
and a missing manager in disposable package copies, and exercises default startup
path resolution from the staged package with an isolated user-root override. The
actual hidden window selects the bundled schema. Report:
`build/native-package-preflight.json`; installation was not performed. Existing
manager tests pass after this change. Start-menu/language-bar launch integration,
interactive installation and real visual/foreground checks remain pending.


The installer now creates an all-users Start Menu shortcut at
`虎娘/方案管理.lnk` pointing to the installed immutable generation's manager,
with no arguments. `shortcut_arm64.ps1` provides shared ownership-checked creation,
retargeting and removal. Ownership requires both the manager description marker
and a `Tigirl.exe` target inside the installation's `versions` tree.
Rollback retargets the shortcut when its selected generation has a manager, or
removes only an owned shortcut when returning to an older generation without one.
Unowned same-name shortcuts and unrelated directory contents are preserved.

`tests/shortcut_test.ps1` verifies actual Windows shell-link creation, retargeting,
owned removal, unowned preservation and unrelated-file preservation in a disposable
Programs directory (`build/shortcut-arm64.json`). Installer and rollback CheckOnly
both pass with the shared helper; `build/native-install-preflight.json` includes the
planned Start Menu path. No real Start Menu or registration was modified. A separate
complete uninstall command is still missing; the removal helper alone does not
satisfy the uninstall acceptance requirement. Interactive install/launch and rollback
integration require verification before the package can be called complete.


`uninstall_arm64.ps1` now provides installation-record-checked unregistration:

```powershell
.\uninstall_arm64.ps1 -CheckOnly
# Actual execution requires administrative privileges and removes the native profile:
# .\uninstall_arm64.ps1
```

Preflight requires the exact native profile, a hash-matching installed immutable
DLL and agreement with current COM registration. It rejects another registered
generation instead of unregistering it through an obsolete record. Execution
rechecks registration after elevation, invokes that DLL's unregister entry point,
then checks COM and the specific language-profile registrations. Only an owned
manager shortcut is removed. User data and binary generations are retained; this
is intentionally not yet a complete disk-cleanup/uninstall acceptance claim.

`tests/uninstall_preflight_test.py` passes live read-only preflight and four rejected
record fixtures (foreign profile, mismatched hash, outside path and stale generation).
The before/after registered-profile plan is unchanged. Report:
`build/uninstall-preflight-arm64.json`. Actual unregistration, remaining TSF category
cleanup, reinstallation and binary cleanup have not been exercised. No uninstall
or elevation was performed while developing/testing this script.


Installer and rollback now preflight the manager shortcut before elevation/registration
changes. The shared read-only check rejects an unowned same-name shortcut, a directory
in place of the link, or a file occupying the intended menu folder. It neither creates
missing directories nor rewrites owned links. Shortcut tests verify unchanged hashes
and absent-directory preservation, plus parent-file conflicts; install and rollback
CheckOnly pass. This catches known namespace conflicts earlier, but does not prove
atomic recovery from every later filesystem/registration failure or a concurrent
external shortcut edit. Actual installation remains unexecuted.
