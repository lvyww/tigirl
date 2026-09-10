# Candidate frame publication

Aligned with TigerClaw commit `d89a242` (2026-09-10): prepare the entire
candidate bitmap offscreen, then publish pixels, size and final caret-relative
position together through `UpdateLayeredWindow`. Only after successful
publication is a hidden window shown, without further moving or resizing it.

Previously `CandidateUI::place` called `SetWindowPos(SWP_SHOWWINDOW)` with the
new dimensions before `paint` rendered and published pixels. That exposed a
window transition before the corresponding image was ready. Cross-monitor
updates also moved the old window before resolving its new DPI.

Now DPI is resolved from the destination monitor under the candidate thread's
PMv2 scope. Positioning is computed on a tentative placement state, which is
committed with hit-test rectangles only after a successful frame publication.
Rendering/publication failure leaves the previously published image intact and
disables stale candidate hit targets; subsequent updates can retry. Explicit
hide and missing-caret states still hide immediately. Unlike upstream, this
change does not add a separate timed rendering-failure retry loop.

Candidate and annotation reveal delays remain real content changes; this fix
removes intermediate window geometry updates, not those configured delays.
No animation, delayed shrinking, or synchronous DWM flush is added.

`tests/candidate_render_test.py` exercises the real rendering and publication
helper for ARM64, x64 and x86, including preparation with unchanged HWND geometry,
rejection of incomplete frames, simultaneous size/position changes, shrinking,
and preservation of intentionally hidden visibility. These checks do not prove
perceived smoothness on physical displays or cross-monitor transitions.
