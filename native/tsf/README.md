# Native TSF adapter

`SampleIME/Server.cpp` creates `Service`, which loads `tiger-v2.tcd` beside the
DLL. The native engine, mapped indexes and user-word overlay run in the host
process. No resident Core or per-key IPC is involved. A context owns its engine
and TSF composition; immutable dictionary storage is reused within a process
and mapping-backed pages are shared across processes.

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
`build/ARM64/Release/SampleIME.dll`. The activation context redirects COM only
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
