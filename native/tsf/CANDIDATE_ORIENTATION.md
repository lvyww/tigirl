# Cross-composition candidate orientation (2026-09-12)

This supersedes the active sliding policy described in the earlier Work-area
sliding placement section of `../CANDIDATES.md`. The low-level work-area clamp
and its regressions remain useful, but `CandidateOrientation` now chooses the
side before invoking that clamp. No input model, dictionary, candidate ranking,
commit behavior, reveal clock or animation policy is changed.

## Direction and stable Y

Initially prefer below the caret with the existing 5 physical-pixel gap. If it
does not fit, use above when possible; otherwise choose the side with more room
and clamp. Above positioning is `caret.top - 5 - height`, not taskbar alignment.
The existing right reserve (2 physical pixels) and bottom reserve (zero) remain.
Content taller than the entire work area cannot be made fully visible by moving
it; the clamp retains the top/left fallback.

After a displayed above target, retain that preference while the caret bottom
has not moved clearly upward. The tolerance is rounded 3 DIP in physical pixels,
capped to one quarter of the smaller current/reference caret height. This is an
initial tuning value, not a measured optimum. A 1..3-pixel caret has zero tolerance.
Inside the band the stable reference is not rewritten. Downward motion outside
the band advances the reference but retains above. Upward motion outside the
band releases the preference and evaluates current space; it does not force below.
If the inherited above position cannot fit and below can, visibility wins.

This remembers a side, not a fixed popup top. Changing height can still move the
first row while the popup remains above the caret. The live X/Y placement is not
frozen by the jitter band.

## Ownership, reset and publication

`Context::candidateOrientation` outlives CandidateUI and composition teardown.
Normal commit/cancel and subsequent words can inherit it, without keeping old
windows, model contents, selection or reveal/first-frame state alive. A different
Context starts independently; pop/deactivation eventually destroys that value.

The environment includes the service's existing focus/mode generation, monitor,
DPI, work rectangle, TSF owner HWND, root HWND and both physical window rectangles.
`modeRevision_` already advances on actual context changes, foreground/thread
focus loss, input-mode changes and deactivation, not on normal commits; the new
read-only accessor reuses it. Spurious focus callbacks for the same context do not
advance it. A changed environment invalidates direction on the next valid layout,
including changing away and back between compositions. A window move is detected
when positioning next runs; this adds no global hook or continuous idle polling.

An invalid/missing caret is never interpreted as Y=0. The existing layout grace
and hide rules still apply, without erasing the direction preference or showing
a new frame from stale caret data. If a non-null owner cannot be queried, hide
rather than inherit unknown geometry. The original PMv2 caret conversion remains.

The UI calculates a copy of the remembered state, publishes a complete current
frame, then stores the accepted target direction only if the UI and focus epoch
are still current. Failed publication/show and reentrant hiding do not latch a
new side. Animation samples do not become stable caret reference positions.
A recreated UI reads direction before its first publication, not after flashing
below and moving above.

## Validation

`python tests/candidate_orientation_test.py --cxx g++` executes production geometry
with synthetic environments. Linux supplies only fixed-width RECT/POINT types.
The suite covers repeated short/long lists, cumulative upward drift, downward
reference advancement, threshold boundaries and small-caret caps, independent
contexts, environment invalidation, invalid rectangles, negative monitors,
96/120/144/192 DPI dimensions, oversized windows and extreme signed coordinates.
A compiled negative control removes the above preference and must fail for the
specific short-composition regression, not a compiler error or arbitrary crash.

On Windows, `--cxx cl --ui` additionally compiles production CandidateUI/Engine/
rendering with the existing unactivated TSF owner test double. It covers actual
layered-window teardown/recreation after commit/cancel, first-frame inherited
placement (including animation enabled), jitter, missing/zero layouts, publication
and Show failures, reentrant hide, owner motion, UI-less operation, independent
contexts and delayed code/candidate display. No registration or input injection.
Both Windows CI platforms run this plus existing presentation/layout/core tests;
Linux CI runs the geometry suite under ASan/UBSan. All new runners reuse the shared
bounded cleanup helper. QQ/WeChat physical input, true focus-switch integration
and physical mixed-DPI multi-monitor movement remain acceptance checks, not claims
made by the synthetic geometry or mocked-owner window tests.
