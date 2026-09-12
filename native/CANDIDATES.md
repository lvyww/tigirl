# Candidate presentation

The pending native build connects six additional original config keys:

| Key | Default |
| --- | --- |
| 竖排候选 | 是 |
| 显示候选序号 | 是 |
| 候选窗显示编码 | 否 |
| 隐藏候选 | 否 |
| 字体 | #霞鹜文楷 GB 屏幕阅读版 |
| 字体大小 | 17, clamped to 3–200 |

`CandidatePresentation` follows the original Overlay's text/visibility rules.
Candidate indexes use `1 `, annotations use `〔…〕`, and CR/LF/tab content is
escaped for display. No-candidate input still shows the code unless hidden
candidate mode suppresses it. Hidden-candidate mode optionally keeps only the
code. UI-less candidate data remains available independently of native display.

`CandidateUI` measures actual GDI text and renders horizontal or vertical item
rectangles. The same rectangles are used for mouse hit testing. Code and item
rows are optional, and font size scales with window DPI. Horizontal content wraps
at monitor width; an individually oversized item is ellipsized. Vertical row
height uses the reference's 1.5 font-size scale, bounded by actual font metrics.
These are functional native layout rules, not proven pixel-identical WPF output.

Font handling now supports installed families and bundled `#` families using
process-private GDI resources from the version's `字体` directory. The exact
reference font, resource reuse and final-release behavior pass native ARM64
checks; see `tsf/FONTS.md`. Missing/rejected families still use GDI fallback.
Theme palettes and per-pixel composition are now implemented (`THEMES.md`); delayed
candidate/annotation expansion, very large-font overflow and complete DPI changes
still need work. Finite numeric font sizes are supported; malformed/non-finite
values fall back to 17 rather than entering invalid geometry.

`tests/presentation_parity.py` compares 80 text/visibility combinations with three
unchanged original Overlay source files copied into the offline oracle; hashes
are in `tools/ReferenceOracle/overlay-sha256.json`. This proves text formatting and
visibility decisions, not screen rendering or hit-test accuracy. Both Linux and
ARM64 pass. The TSF host's native mouse test now locates the rendered highlight
instead of assuming a fixed font/row/header size; execution against the pending
installed DLL is still required. Previous installed popup screenshots do not
validate this new layout.

## Delayed reveal

The appearance settings accept TigerClaw's original keys:

| Key | Default | Range |
| --- | --- | --- |
| 延时显示候选(毫秒) | 0 | 0–60000 ms |
| 延时展开注释和拆分(毫秒) | 0 | 0–60000 ms |

Blank/invalid config values mean zero; signed integer values clamp to the range
(int32 overflow is invalid). The settings dialog accepts blank as zero and rejects
out-of-range or non-integer edits without saving.

Both clocks start with the candidate display session, not with every keystroke.
Once revealed, content stays expanded through further typing, paging and selection.
Annotations cannot appear before candidates; their delay is measured from the same
start, not added to the candidate delay. Pinyin reverse-lookup annotations bypass
the annotation delay. As in TigerClaw, an absent candidate/annotation list latches
that part as expanded. A positive candidate delay overrides 隐藏候选 after the wait;
with zero delay, 隐藏候选 continues to hide candidates permanently. Before reveal,
encoding appears only when 候选窗显示编码 is enabled.

The UI owns a Win32 timer on its nonactivating candidate window, including when
that window is temporarily hidden awaiting reveal. Commit/cancel/focus teardown
removes the window and timer. Missing caret geometry prevents display; a timer
never reuses stale geometry to show the window. TSF UI-less candidate enumeration,
selection and commit remain available during the visual delay.

Validation: `python3 tests/candidate_reveal_test.py` covers clock boundaries,
independent deadlines, latching, reset, hidden/code-only display, pinyin and parsing.
`python3 tests/input_settings_test.py` exercises save/reopen/cancel, eight invalid
delay edits and native settings layouts at 96/144/192 DPI on an inactive desktop.

## Candidate mouse controls

Right-button down opens the same schema/theme/management menu as the language-bar
button. The candidate HWND owns this popup; unlike the taskbar entry point, this
path does not create and activate a temporary owner window. Cancelling the menu
therefore leaves the application's composition and focus in place.

A vertical mouse-wheel delta of +120 increases font size by 0.5; negative deltas
shrink it. No Ctrl modifier is required. Fractional deltas are proportional; the
saved value is rounded to two decimal places and clamped to 3–200. The stable
configuration lock protects read/modify/write against other TSF instances, and
only 字体大小 is changed, without an input-settings reload request. The current
candidate renderer is replaced and relaid out immediately, retaining selection
and reveal-session state. Other instances pick up the saved style through the
existing configuration watcher. Wheel delivery uses normal Windows messages;
no global mouse hook or Core process is introduced.

`tests/candidate_mouse_test.py` drives actual TSF candidate windows in a temporary foreground
host: enlarge/shrink, persisted value, popup contents/cancel, retained focus
and composition, and subsequent Space commit. `tests/candidate_font_wheel_test.py`
checks bounds, fractional deltas, preservation of unrelated fields, and concurrent
writers. Neither test injects global input or switches the input desktop.

## TigerClaw mode layout (2026-09-09)

This section supersedes the former uniform 6-DIP padding and 10-DIP spacing.
Middle-button release cycles horizontal → vertical → code-only → horizontal.
Horizontal and vertical show-code preferences are remembered separately for the
service lifetime, as the reference Overlay remembers them for its lifetime. Each
cycle atomically saves the three public display flags together, without resetting
composition. A positive reveal delay retains the original hide/delay interaction.

Padding (left/top/right/bottom, DIP): vertical 12/8/8/7; horizontal 8/8/8/8;
code-only round(fontSize×0.4)/2/round(fontSize×0.4)/2. Theme border width is outside
this padding. Line heights are ceil(fontSize×1.5) vertically and ceil(fontSize)
horizontally/code-only. Minimum text-control widths are ceil(fontSize×3.76+15)
vertically and ceil(fontSize×2.88+15) horizontally; code-only has no minimum.
Horizontal gaps use actual font widths for two spaces; after input code, append
max(0,7-codeLength) spaces. No horizontal wrapping; the work-area width cap trims
long text and prevents clipped-away candidates from retaining mouse targets.

Placement uses the actual TSF caret and existing PMv2 conversion rather than the
Overlay's IPC anchor cache/Start-menu heuristics. The current cross-composition
direction policy is specified below.

Validation includes three architectures, nine themes, DPI roundtrips, mode padding,
minimum widths, sizes 3/17/31.5/200, and simulated negative-coordinate work areas
at 96/120/144/192 DPI. These simulated placement checks do not establish physical
multi-monitor dragging behavior in Word or WeChat; that remains user acceptance.

## Cross-composition direction memory (2026-09-12)

This supersedes PR #8's stateless work-area-bottom sliding policy. Restore
above/below placement: normally prefer `caret.bottom + 5`, and flip to
`caret.top - 5 - height` when below does not fit. Keep the existing 5 physical-pixel
caret gap, 2-pixel right reserve and zero bottom reserve. If neither side fits,
choose the side with more space and clamp to the work area. Oversized content
still cannot be made fully visible by positioning alone.

After a successful above placement, retain that direction while the physical
caret bottom is within the stable reference's jitter tolerance or moves down.
Tolerance is round(3 DIP in monitor pixels), capped at one quarter of the host
caret height; very short carets can have zero tolerance. Jitter samples do not
replace the stable reference. An accumulated upward move beyond tolerance
re-evaluates normally, rather than forcing below even when it cannot fit.
Downward movement beyond tolerance advances the reference while retaining the
above preference. The actual popup still follows current caret coordinates;
this is direction hysteresis, not frozen coordinates or a fixed popup top.
The above preference is ignored when above cannot fit and below can.

`Context::placement` owns this memory, not CandidateUI. Normal word/sentence
commit, cancellation and popup teardown preserve it across compositions.
New popup objects use the retained direction on their first published frame.
Reveal clocks, candidate contents, selection, first-presentation state and
animation instances retain their existing per-popup/per-composition lifetimes.

Genuine focus/context changes, foreground/thread focus loss, view destruction,
context removal, English-mode transition and service deactivation invalidate
memory. Repeated focus notification for the same context does not. Active view
COM identity is tracked separately so changing views within one context resets
it as well. Monitor, physical work area, DPI, owner HWND and physical owner
rectangle changes cause a fresh placement decision; environment transitions do
not animate through the previous environment's coordinates. Ordinary layout
notifications do not reset direction. Invalid or temporarily unavailable caret
geometry does not mutate memory or allow a timer to resurrect a stale popup.

Layout calculates on a copy. A successfully published current frame accepts its
direction/reference only if the context placement revision has not changed in
a reentrant callback. Failed backing allocation, publication or first show does
not record a new direction. This preserves the previous first-frame and layout
notification fixes rather than treating UI destruction as an environment reset.

Validation entry points:

- `python tests/candidate_placement_test.py --cxx g++` (or `cl` on Windows):
  production geometry header, 81,609 checks, eight work areas, four DPIs, stable
  jitter reference, gradual upward movement, downward inheritance, repeated
  simulated word-size sequences, resets, visibility fallback and LONG extremes.
  The negative control disables above inheritance and must be rejected. This
  runner is included in the existing Windows/Linux core CI and retains the
  shared bounded scratch-directory cleanup introduced for PR #8.
- `python tests/candidate_ui_placement_test.py --negative-control`: real Windows
  layered windows and renderer, controlled clock and TSF owner double. Checks
  new-popup first-frame inheritance, commit/cancel teardown, shrinking, jitter,
  upward movement, delayed reveal, no-layout/invalid geometry, publication
  failures/reentrancy and owner movement. A reset-on-detach mutation must fail.
- Build `tests/CandidatePlacementHost.vcxproj`, then
  `python tests/candidate_layout_test.py --platform x64 --placement` (or Win32):
  staged production DLL with real TSF contexts, edit sessions and both bundled
  dictionaries. Seeds geometry memory to isolate commit/cancel, duplicate focus,
  same-HWND context changes, thread focus, view destruction, pop and deactivation
  lifetimes. It is UI-less; real rendering is covered by the preceding probe.

Windows x64/Win32 CI runs the new window/lifetime tests alongside the existing
layout/presentation and cleanup/failure-propagation suites. These tests do not
establish QQ/WeChat physical-input behavior, taskbar auto-hide or physical
multi-monitor dragging; those remain installation acceptance checks. No profile
registration, user-data changes or global input injection is required.
