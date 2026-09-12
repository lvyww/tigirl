# Candidate direction memory (2026-09-12)

This replaces PR #8's stateless work-area sliding policy. Normal placement is
below the input caret; if below does not fit, choose above when it fits, otherwise
use the side with more space and clamp to the work area. The caret gap remains
5 physical pixels; the right reserve remains 2 pixels and bottom reserve zero.
An oversized popup still needs clamping and can overlap the input line.

## Cross-composition preference, not a position lock

`Context::candidatePlacement` survives `CandidateUI::detach`, a normal commit,
cancel and popup recreation. It does not retain candidates, timers, renderer
resources, first-presentation state or the old HWND. A fully fitting above-caret
placement establishes an above preference. The next popup uses it before its
first publication, even when that popup is short enough to fit below the caret.

For unchanged input/coordinate environment, inherit above when
`currentCaret.bottom >= referenceY - tolerance`, provided above still fits.
If above no longer fits but below fits, below wins. When neither side fits the
normal space/clamping fallback runs; an oversized overlap is not remembered as
a successful above placement.

The tolerance is `min(ceil(3 * DPI / 96), floor(caretHeight / 4))` physical pixels.
It is based on the input caret, not the candidate font. A caret shorter than four
pixels has zero tolerance. Three DIP is an initial tuning value, not an empirically
optimal threshold. No setting/UI has been added for it.

Samples inside the deadband do not move the reference. A downward departure
updates the reference without releasing above. An upward departure allows a fresh
choice (it does not force below). Thus repeated 1-2 pixel upward steps eventually
release the preference instead of being swallowed forever as adjacent-frame jitter.
The actual candidate coordinates continue following the valid caret. This policy
reduces side changes; it does not freeze the popup's top while height changes.

## Invalidation and publication

The state is scoped to one live TSF Context. It also compares the monitor handle,
monitor work rectangle, DPI, input owner HWND and its physical bounds. Any change
makes the next placement a fresh decision. Negative monitor coordinates are valid.

`Service::candidatePlacementEpoch()` exposes the existing focus/mode revision.
Actual focus changes, focus loss, deactivation and input-mode changes invalidate
old preferences through this epoch; ordinary commit/cancel, keys and repeated
same-context focus callbacks do not. This deliberately reuses the existing
synchronous lifecycle invalidation rather than adding hooks or a global cache.
Popped/destroyed contexts release their memory along with the Context object.

Missing or malformed caret geometry does not update direction or reference Y.
The existing bounded no-layout grace and hide behavior remain responsible for
visibility: remembered direction never authorizes displaying at stale coordinates.

Placement is calculated on a copy of Context memory. Only a successful current
frame can publish it, and the focus/mode epoch and memory revision must still
match. Failed prepare/publish/show and reentrant hide/detach cannot record the
rejected target. The remembered choice belongs to the accepted final layout,
not an interpolated animation frame. Existing first-candidate atomic publication,
subsequent resize animation, reveal delays and TSF selection are unchanged.

## Validation

`python tests/candidate_placement_test.py --cxx g++` (or `clang++ --sanitize`)
compiles the real production header. Its 202,852 checks include cross-composition
size sequences, deadband boundaries, cumulative upward drift, input-line-height
caps, each environment reset, fallback visibility, failed preview isolation,
eight work areas, four DPI scales and widened integer arithmetic at LONG limits.
A compiled negative control disables inheritance and must fail for the intended
cross-composition assertion; compiler failures are never accepted as success.
The shared retrying scratch-directory cleanup from PR #8 is retained.

`tests/candidate_ui_presentation_test.py` additionally builds
`candidate_direction_ui_probe.cpp`, using the existing real layered-window and
renderer fixture with a controlled clock and mocked TSF owner. It exercises new
HWND first frames after commit/cancel, animation on/off, invalid/no-layout caret,
reveal delay, failed prepare/publish/show, reentrant hide and real owner-window
movement. The original presentation regression and its negative control still run.

`CandidateLayoutHost` checks the real staged Service epoch through real TSF edits,
commit/cancel, same/different context focus, thread focus loss and deactivation.
It complements the direction renderer fixture; it is not a physical typing test.
All of these runners are already in Windows x64/Win32 and Linux core CI (Windows
renderer/TSF cases run on Windows only). The renderer probe's old sliding
assertions now expect above placement and remembered direction on shrink.

QQ/WeChat actual typing, auto-hide taskbars and physical mixed-DPI multi-monitor
drags remain installation-level acceptance work. Synthetic coordinates and
controlled publication tests must not be represented as that acceptance.
