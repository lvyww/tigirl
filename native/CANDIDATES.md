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
Overlay's IPC anchor cache/Start-menu heuristics. The former above-caret latch is
superseded by the work-area sliding policy below.

Validation includes three architectures, nine themes, DPI roundtrips, mode padding,
minimum widths, sizes 3/17/31.5/200, and simulated negative-coordinate work areas
at 96/120/144/192 DPI. These simulated placement checks do not establish physical
multi-monitor dragging behavior in Word or WeChat; that remains user acceptance.

## Work-area sliding placement (2026-09-12)

The preferred origin remains `caret.left, caret.bottom + 5` in physical pixels.
If the measured popup does not fit below the caret, `placeCandidateWindow` moves
it up only to `work.bottom - height`, clamped at `work.top`. The popup's bottom
therefore touches the work-area bottom (the taskbar's top when docked below), not
the caret's top. The bottom reserve is now zero; the existing 2-pixel right
reserve and left/top clamping are unchanged. An oversized popup starts at the
work-area top/left; positioning alone cannot make oversized content fit.

This is deliberately stateless: growing/shrinking candidates, delayed reveals,
font changes and new word/sentence sessions all use the current measured size.
There is no above-caret flag, monitor latch or reset path. Once a shorter popup
fits below the caret again it returns there, without a flip threshold. This
removes the abrupt above/below switch, not the movement inherent in resizing.
Overlapping the caret/input box near the bottom is intentional under this policy.
The work area still comes from the caret's nearest monitor, including negative
virtual-screen coordinates; it is not the primary screen or the previous popup's
monitor. Existing first-frame publication, resize animations and TSF selection,
commit and reveal semantics are unchanged.

This follows bime's `WinCandidate.xaml.cs` bottom-overflow policy, except that
bime retains a 2-pixel bottom reserve and this policy aligns exactly to the edge.

`python tests/candidate_placement_test.py --cxx g++` runs the production positioning
header against exact-fit/one-pixel-overflow cases, repeated sessions, smooth
height sweeps, work-area changes, oversized popups, eight monitor/work-area
configurations and sizes at 96/120/144/192 DPI. Linux provides only fixed-width
Win32 geometry types; Windows uses the SDK. A negative control compiles the same
probe against the former flip-and-latch algorithm and must reject it. The test is
also included in `tests/run_core_tests.py` for Windows x64/Win32 and Linux
ASan/UBSan CI. The renderer probe's former above-caret assertions now check
bottom alignment and return below the caret after shrinking. These geometry
checks are not physical multi-monitor or chat-application acceptance tests.
