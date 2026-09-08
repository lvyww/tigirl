# Ordinary TigerClaw acceptance audit

Latest user acceptance: after deploying primary `8005a14ee64cefc0` and the
matching x86 supplement, the user confirmed the taskbar 中/英 button test passes.
The preceding package also received basic manual input confirmation in Weixin,
Word and 32-bit Pain. These confirmations do not claim every matrix item below.
Historical package references below retain the scope of their recorded tests.

Audit date: 2026-09-08. The goal and all acceptance bullets in `PLAN.md` remain
unchanged. Component evidence below does not declare the complete goal achieved.
Current ordinary ARM64 staged DLL: `10c54e4d2654cb0752911ad79ec93f9359ea9786531574a2290dfb232d0b611f`.
Current ARM64X staged DLL: `db270be795ece42b6502cf3c8e19551c5d4e2a17061a845d6529c31cbe2f1bcc`.
Both include native compaction/recovery and the targeted stale-selection fix.
Both also fix an old context's deferred termination hiding a new context's
candidate UI. ARM64X package `09d5f4f5445d1891` is now installed, including
its maintenance helper and manager. See `native/USER_STORE_MAINTENANCE.md`
and `build/user-store-maintenance-manager-imported.json` for tested scope.
`build/native-install.json` and `install-arm64.log` record successful installation
and ARM64/x64 installed loader/class-instance checks. An independent registry
and DLL hash read matched that record. Installation was followed by fresh
ARM64/x64 system-registered activation checks in
`build/registered-tsf-validation.json`, with exact DLL/host hashes. Both pass
module/dictionary verification, five explicit key callbacks and deferred-mode
checks. These hidden-host checks do not prove real-application acceptance.

| PLAN requirement | Inspected evidence | What remains unproven |
|---|---|---|
| Native engine, independent TSF composition | `native/Engine.cpp`, `native/tsf/Service.cpp`; private multi-context TSF reports | Current package in real application text controls and OS-delivered key sequences |
| Original formats, precedence, ordering, duplicate/phrase/alias/escape semantics | `build/lexicon-all-directory-parity-arm64.json` (17 cases, all sections, no mismatches); import/order/decode/construct reports | Full importer evidence assembled with final release provenance; practical import/update workflow |
| Shared immutable main and auxiliary tables | `build/dictionary-parity.json` (757,399 records); dictionary hash matches staged/installed package; `build/dictionary-memory-arm64.json` (4 processes, 10,229 multiply shared pages each) | Final-package actual application memory measurements; ARM64X cross-architecture sharing now passes the mixed-host fixtures below |
| Ordinary key operations, paging, bindings, short/uppercase input | `build/key-parity-arm64.json` (35,554 events), `build/settings-key-parity-arm64.json` (41,100 events), selection/uppercase reports | Final assembled engine coverage/provenance and actual keyboard acceptance; a green historical trace is not a current DLL test |
| Mode, modifiers/repeats, punctuation, reverse lookup | Current `build/tsf-private-validation.json`; key/oracle component reports | Physical modifier/focus transitions, particularly the previously intermittent held-Ctrl foreground case; application matrix |
| Candidate layout, font, annotations, caret and mouse | Presentation/theme/font reports; older full TSF/candidate captures | Current-package rendered horizontal/vertical layout, fonts, themes, multiple DPIs, caret/mouse behavior in applications |
| Config/schema/recent shortcut and persisted user edits | Current word-save failure/retry/secure-reactivation report; settings recovery, schema/generation/concurrent journal and live-reader reports | Full settings/menu focus workflow; journal growth/compaction and general recovery; final-package live host lifecycle coverage |
| ARM64 build/install/uninstall, practical apps, architecture coverage, preserve daily IME | Current read-only package check; earlier installation record; uninstall/rollback scripts and preflight evidence | Actual app checks. ARM64X install/rollback/uninstall/reinstall and final ARM64/x64 registered activation pass (`build/deployment-cycle.json`) |
| Original Core oracle | `tools/ReferenceOracle`, existing isolated oracle/parity reports | Final trace coverage/provenance assembled with the release; oracle results do not replace runtime acceptance |
| Multi-process memory and input latency | `build/tsf-memory-active-arm64.json` (4 actual TSF processes; shared pages, about 4.1–4.4 MB private memory after 20k commits) | Report belongs to an earlier DLL; workload excludes physical keys/rendering and does not pump the change watcher during the measured loop |

Current-reference provenance caveat: `build/reference-source-current.json`
finds 10 changed/missing files among 34 frozen upstream files when compared
with the supplied checkout's current `next/` directory. The fresh settings
report is a match against the verified frozen snapshot, not proof of parity
with these newer reference contents. Source review of the changed engine/state/
protocol files found sentence/smart-mode changes and no ordinary selection or
commit branch change. The frozen snapshot remains the ordinary-input baseline;
this source review does not establish runtime parity with the evolving checkout.

## Architecture finding and next priority

`tests/ArchitectureLoadProbe.vcxproj` builds actual ARM64 and x64 executables.
`python3 tests/architecture_load_test.py` runs both on this ARM64 Windows system:
the ARM64 executable loads the ordinary ARM64-only DLL and creates
ITfTextInputProcessorEx; the x64 executable fails to load that ordinary DLL with
error 193. This historical result does not describe the installed ARM64X DLL. Evidence, PE machine
fields and executable/DLL hashes are in `build/architecture-load-arm64-only.json`.
The runner verifies each probe's PE machine rather than treating
IsWow64Process2's zero process-machine result as an architecture classification.
It deliberately records that TSF activation and real application input were not
tested by this small loader probe. x86 and ARM64EC hosts remain untested.

An isolated ARM64EC/BuildAsX configuration is now implemented. `build_arm64x.ps1`
produces `build/ARM64X/ARM64EC/Release`, including native ARM64 companion tools.
The experimental DLL `8a9fd592e143d7df636c2acef0f510dde33bb8023329fb948e253a31022b734d`
passes both loader probes (`build/architecture-load-both.json`) and actual ARM64/
x64 TSF hosts (`build/arm64x-tsf-validation.json`), including explicit input,
failed adjustments, interrupted-journal-tail recovery, retry persistence and
secure reactivation. This resolves the demonstrated loader failure for that
ARM64X DLL, previously installed as generation `e3b95d4b9183e325`.
The retained ARM64-only generation remains available for rollback.
Simultaneous sharing and propagation are now measured: a single ARM64/x64 pair
reports all 10,229 dictionary pages multiply shared; a four-host mixed active
fixture and eight-stage continuously pumping configuration/schema/user-word
reader fixture pass. Separate ARM64/ARM64EC storage probes also pass mixed-writer
and interrupted-tail tests. See `build/tsf-memory-pair-arm64x.json`,
`build/tsf-memory-active-arm64x.json`, `build/tsf-live-readers-arm64x.json`, and
`build/user-store-mixed-arm64ec.json`. The active timing fixture excludes physical
input/rendering and watcher-loop pumping; the storage concurrency probe is not
simultaneous TSF UI dispatch. Installer integration now passes dual-loader and invalid-package preflight
(`build/native-package-arm64x-preflight.json`), including rejection of an ordinary
ARM64 DLL substituted into the ARM64X package. Both full management-menu fixtures
pass popup selection, child foreground and unassisted return of host foreground
(`build/management-menu-arm64x-ARM64.json`, `build/management-menu-arm64x-x64.json`).
Keys are synthesized, not physical hardware input. Actual deployment, rollback,
uninstall and reinstall now pass; real application input acceptance remains open.
Microsoft documents [ARM64X for in-process COM and plugins](https://learn.microsoft.com/en-us/windows/arm/arm64x-pe)
and [BuildAsX configuration](https://learn.microsoft.com/en-us/windows/arm/arm64x-build).
An ARM64-only success must not stand in for this gate.

## Desktop and deployment gates

Latest diagnostics also show an uncontrolled input: `z` was present before the
first explicit test event, then `A` produced `za`. Foreground repetitions were
stopped. This provides an input-contamination explanation for that run, not proof
of the cause of earlier count/context failures. A controlled run is still needed.

A later idle-gated run now reproduces focus-return failure with no queued-key
trace entries: explicit A/B are unhandled after return, leaving `交ab` rather than
`交ab疒`. See `build/registered-tsf-interactive-ARM64.json`. The fixture now waits
for asynchronous layout restoration and scopes candidate lookup to its process.
Investigate open-mode/modifier state; root cause remains unproven.

Five subsequent ARM64 repeats pass with zero queued-key entries and open mode 1
throughout (`build/focus-mode-repeats-bt6ty75r`). An x64 run still fails visible
candidate restoration after the two-second deadline with `ab` composing, mode 1
and no modifiers held (`build/registered-tsf-interactive-x64.json`). This remains
an unresolved gate; next diagnostics cover text-store layout queries and locks.

Latest x64 tracing identifies an earlier divergence: immediately after A the
text is `a` but the TSF composition count is zero; B starts a separate composition
and shows two candidates. Open mode remains 1. Correcting the test application's
async lock queuing did not resolve this failure. Trace the composition termination
source before attributing this solely to layout or mode changes.

Composition lifecycle tracing produces five passing x64 runs, while disabling
trace reproduces second-context commit failure. Keeping the composition sink
without logging does not resolve it. Diagnostics now buffer output until exit,
but one subsequent passing run is not proof of a fix. See
`build/composition-trace-investigation.json`; timing sensitivity remains open.

An untraced run with a fixed in-memory observation ring reproduces first-key
termination: A handled, composition starts=1/ends=1; B starts composition 2.
`build/key-ring-repeats-3bg37g9x/runs.json` preserves the evidence. No diagnostic
COM queries or per-key output are needed to observe this failure.

The previous installed DLL's interactive gate was **failing intermittently**;
the new package has not yet repeated this foreground gate. One full
four-variant run passed, but repeat tests found context-text disagreement after
focus return and a wrong candidate count (2 instead of 4 for `ab`). See
`build/registered-tsf-interactive.json` and the repeat report. The ARM64 candidate
capture is legible; this does not resolve the runtime failure. Diagnose before
claiming the context/candidate gate complete.

The desktop became interactive for the recorded ARM64/x64 full menu runs. Both
passed with configuration and registration unchanged. The ARM64 input-settings
capture was visually inspected: Chinese text and controls are legible with no
obvious clipping in the captured window. This is one window at one captured DPI,
not complete appearance or physical-input acceptance. Further desktop interaction
is pending the user's response to the reissued availability prompt.

The full TSF runner now accepts `-PrivateBuild -Arm64X` for ARM64 and adds
`-X64Host` for x64, with separate reports/captures and matching manifests.
Its `-ActivationOnly` path passes both architectures with an isolated user root
(`build/tsf-private-arm64x-ARM64-validation.json` and the x64 counterpart).
These runs cover explicit mode/deferred-lock callbacks, not keyboard dispatch.

Continue with the remaining interactive native TSF fixtures, then
real application tests against actual edit controls (not only this test host).
Record application version/architecture and package hash with each result.
The package is now installed with dual-host registered activation verified.
The real rollback/uninstall/reinstall cycle now also passes
(`build/deployment-cycle.json`), with unchanged user-data hashes and independent
daily TigerClaw registration. That historical deployment cycle ended with ARM64X `e3b95d4b9183e325`.
The current deployment is `09d5f4f5445d1891`, as recorded above. Real
application and remaining input/UI acceptance are open.
`PLAN.md` checkpoints 3–6 therefore remain open.
