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
