# Candidate content and layout updates

`CandidateUpdate::Content` publishes a new Engine snapshot, selection and default
page table. It is used by input, sentence-result and data/configuration updates.
`CandidateUpdate::Layout` only refreshes the caret and native window geometry. It
does not copy Engine, enumerate candidates, replace host selection/page tables,
or mark the candidate model changed. Context::revision is deliberately not a
candidate version: key-up and other non-content events may advance it.

The initial model is available before BeginUIElement. The following layout pass
must not overwrite selection or pagination that the host set in BeginUIElement.
CandidateReveal deadlines and the existing short TS_E_NOLAYOUT frame-retention
policy are unchanged. Pure layout uses the existing candidate snapshot; timers
continue naturally rather than restarting because the caret moved.

## Notifications

GetUpdatedFlags exposes pending model changes. First publication advertises all
fields. Content updates compare count, selection, page table and current page;
strings are conservatively marked because off-page candidates can change too.
No full off-page candidate enumeration or dynamic-text evaluation is added.

notifyUpdated skips empty notifications and concurrent/reentrant delivery. The
pending flags are acknowledged only after UpdateUIElement succeeds for the same
model revision. A failed notification, or new content produced inside a callback,
remains pending for the next update. A layout recovery may retry that *existing*
notification but does not rebuild the model or reset host state. UI and manager
references are retained across the callback; detached elements cannot notify.

Late layout edit sessions validate activation, focus, composition and context
membership before touching the UI. This change does not alter the document-write
transaction or candidate ordering rules.

## Tests

Windows MSVC Developer PowerShell (matching x64 or x86):

```powershell
python tests/candidate_ui_layout_test.py --negative-control
```

This compiles production CandidateUI/Engine/rendering code and exercises both
UI-less and real layered-window paths. The owner and counting UI-element manager
are test doubles. It checks host selection/page tables, actual caret movement,
model-change flags, failed-delivery retry, reentrant content/layout/detach, and
absence of repeated notifications. A temporary negative control restores only
unconditional model replacement and must fail the layout-preservation assertion;
compiler failures are not accepted as a negative-control result.

The existing integration probe also loads the production DLL and imports both
tracked default schemas, then uses real Windows TSF document/context/edit-session
and UI-element services:

```powershell
./build_native.ps1 -Platform x64
# Build tests/CandidateLayoutHost.vcxproj with MSBuild for the same platform.
python tests/candidate_layout_test.py --platform x64
```

A factory-loaded, unregistered input service cannot subscribe to the native
keystroke manager in this test environment. The test-only LayoutThreadManager
supplies that subscription, forwards the document/manager operations, and the
probe calls ITfKeyEventSink explicitly. It does not register a profile, activate a
system input method, inject global input, or change production activation rules.
The JSON output discloses the mocked subscription separately from real TSF edits.
The probe keeps the assertions for layout-only notifications, sixth-candidate
finalization, keyboard page changes, abort and late callbacks. A startup failure
is a test failure, not proof of the original candidate reset.

Both runners clean their own temporary directories with six bounded attempts.
Exhausted filesystem cleanup emits a separate stderr warning; compilation, probe,
negative-control errors and timeouts still fail. No user dictionary is modified.

These tests do not constitute physical-input, QQ, multi-DPI hardware, ARM64 or
full sentence-language-model acceptance. PR #4's first-candidate presentation
change remains a separate change; this PR does not alter the animation decision.
