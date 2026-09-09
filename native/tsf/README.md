# Native TSF adapter

`SampleIME/Server.cpp` creates `Service`, which loads `tiger-v2.tcd` beside the
DLL. The native engine, mapped indexes and user-word overlay run in the host
process. No resident Core or per-key IPC is involved. A context owns its engine
and TSF composition; immutable dictionary storage is reused within a process
and mapping-backed pages are shared across processes.

Windows file caches use absolute lexical paths, not `canonical()`: under
AppContainer, canonicalization can fail with access denied even when the same
Program Files dictionary can be opened and mapped. File opens still enforce
the original ACLs. `tests/AppContainerReadProbe.vcxproj` builds a read-only
regression probe accepting an existing AppContainer PID, a dictionary path,
and a sentence-model path. It impersonates that token to validate mapping,
cache reuse and missing-file rejection; it does not test foreground UWP input.
The service and manager resolve LocalAppData with
`KF_FLAG_NO_PACKAGE_REDIRECTION | KF_FLAG_DONT_VERIFY`, so packaged hosts use the same NativeTiger
root as desktop hosts. Installation grants AppContainer Modify access and a
low integrity label only to that data tree, including existing children.
External scheme sources and Program Files executables are not made writable.
New files inherit this policy. AppContainer configuration replacement retains
the inherited directory ACL when Windows cannot merge the old file's ACL;
desktop writers retain the original strict replacement behavior.

Theme menu actions write the shared configuration directly, without launching
a desktop manager from the restricted host. The existing data watcher applies
the change to other instances. The mode item declares both button and menu
capabilities and accepts right-click callbacks without a rectangle.
`AppContainerConfigProbe.vcxproj` tests shared known-folder resolution,
bidirectional config visibility, actual menu theme selection, repeated atomic
replacement and preservation of unrelated settings in a marked test directory.
The lookup test runs after impersonation with a null token argument, matching
the service call. Without DONT_VERIFY the shared child can be accessible while
the parent LocalAppData existence probe fails and returns an empty user root.
Taskbar popups use the current TSF context window as their owner. Foreground
behavior still requires a real UWP acceptance test. Creating
`menu-trace.enabled` in NativeTiger enables `menu-trace.log` with menu callback
stages and HRESULTs (no composed text); remove the marker to disable it.

2026-09-09 acceptance: the user confirmed the UWP/Start-menu fixes passed
foreground testing after installation, including the reported skin and menu
issues. ARM64, x64 and Win32 input regressions, restricted-token configuration
and theme-menu tests, and the desktop popup/focus fixture also passed.

Sentence continuation follows TigerClaw commit
`954c82d5ed3823a9007375568740a67d063dfebc`: empty-code automatic commit must retain
decoder-approved segmented duplicate-single paths, so `xrxbj`/`xryxbj` can keep
`反刍` as the first choice after `反` is committed. The decoder remains responsible
for the duplicate-single toggle, optimal-code eligibility and word-rank rules;
the session still rejects whole-input non-first edges on implicit continuation.
`tests/sentence_continuation_test.py` covers both prefix codes and both settings
on ARM64, x64 and Win32, using the real native decoder and Engine. This focused
regression tracks the upstream fix without replacing the older frozen oracle
with unrelated upstream changes.

Key preview copies the engine. Actual consumed keys request a synchronous TSF
write edit session, replace the composition or insert committed text, and publish
the new engine state. Unconsumed keys may still end an existing composition;
their observed event signature prevents duplicate dispatch. Enter's internal
history marker is suppressed when the physical Enter is passed to the application.
User operations are persisted only from actual dispatch, never from previews.

Composition termination clears engine state. Selection changes outside the
composition request an asynchronous edit session guarded by a context revision.
Deferred sessions retain the COM service. Candidate UI objects are detached from
the service before teardown so a host-retained UI element cannot call a dead
service. Focus changes refresh user data and reset transient modifier state.

The candidate object implements `ITfCandidateListUIElementBehavior` for hosts
that suppress the native window. Otherwise it renders a nonactivating popup and
commits mouse selections through a revision-checked edit session. Model refresh
does not depend on layout availability: absent geometry hides the popup while
keeping UI-less candidate data current; a layout notification retries placement.
Windows explicitly permits a missing-layout result from
[GetTextExt](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcontextview-gettextext).
Read/write session constraints are documented under
[RequestEditSession](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcontext-requesteditsession).

`tests/TsfHost.vcxproj` builds a native ARM64 application with real Windows TSF
documents and a small `ITextStoreACP`. It activates the registered profile only
for its process. The two documents have distinct HWND focus associations.
Tests exercise preview/dispatch, preedit replacement, commit/cancel, composition
termination across focus changes, UI-less selection and optional popup/mouse
selection. They require foreground focus and do not establish compatibility
with all applications. The optional second argument saves a native popup BMP.

Custom selection bindings now reload from the independent per-user file on
activation/focus and through the owning thread's directory-change scheduler;
see `../SELECTION.md`. Settings, user journals and schema generations also update
in the focused context without a focus transition. Notifications are polled at
250 ms with five-second reconciliation/reopen fallback. Actual delivery depends
on the host pumping messages and granting TSF edit sessions. Unchanged effective
data preserves paging and candidate UI. Installed validation remains pending.

Ordinary engine settings now load from the independent per-user config; see
`../SETTINGS.md` for the supported keys and unverified runtime coverage.

Remaining scope includes remaining configuration/schema UI, language-bar and input
mode compartment synchronization, configurable candidate layout/fonts, add-word
UI, persistence-error recovery, broader application/lifetime testing and the
remaining ordinary-engine features in `PLAN.md`.

## Private build validation

`tests/run_tsf_host.ps1 -PrivateBuild` uses a test-only activation manifest beside
`build/ARM64/Release/Tigirl.dll`. The activation context redirects COM only
inside the test process. It does not register or install that DLL, and the runner
compares the original registered path before and after. An existing enabled TSF
profile is still required for Windows profile metadata. The manifest is not part
of the installed package. See Microsoft's
[assembly manifest documentation](https://learn.microsoft.com/en-us/windows/win32/sbscs/assembly-manifests)
and [CreateActCtxW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createactctxw).

The fixture verifies that no other SampleIME generation loaded and that the real
service mapped `tiger-v2.tcd` from the tested DLL's directory. Merely preloading a
class factory is not accepted as service activation evidence. The full test still
requires OS foreground focus and does not skip that requirement.

Add `-ActivationOnly` when checking startup without an interactive desktop. This
mode verifies profile/service activation, the module and dictionary, then
teardown; it explicitly reports zero input events and never claims input/window
coverage. It currently passes on ARM64 with registration unchanged. The attempted
full private run stopped before event zero because GetForegroundWindow returned
null. Full composition, layout-recovery, font and layered-window behavior remain
unverified for the pending build until an interactive desktop is available.

## Tab confirmation in sentence input

Ported from TigerClaw `14b611f48555bb0f02b1fdf1e7b02671096bebbc`.
Tab/Shift+Tab highlights without committing. The next code letter fixes the
chosen text and raw-code boundary; the decoder scores only the remaining tail,
with language-model and supplement context reconstructed from the locked text.
With automatic commit enabled, that letter also submits only the uncommitted
selected text, independently of confidence and retained-code floors. Otherwise
Backspace reaching a lock boundary releases it; nested locks unwind one at a
time. Numeric/punctuation rank selectors edit the current segment, and arrow
navigation alone does not arm confirmation. Literal exits retain live raw-code
semantics. Immutable lock snapshots travel with synchronous/asynchronous decode
tickets and exact-path queries; old completions cannot replace the new state.

Run `tests/sentence_tab_lock_test.ps1` in Windows PowerShell to build and run
ARM64/x64/Win32 engine+decoder fixtures, the preceding rumination regression,
and the session-state suite. These tests replay pending decode requests and
inject stale results; they do not constitute physical foreground input testing.
