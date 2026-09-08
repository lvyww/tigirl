# Candidate themes

The pending build reads `主题` from the ordinary config. Names and palette values
match the original Overlay resolver: 默认, 通透, 一般通透, 迷雾, 星夜, 纸, 粉,
赛博朋克, 清晨. Unknown names fall back to 默认. Theme selection reloads on focus.

`CandidateTheme` keeps foreground, background, border and selection colors as
ARGB, including the original alpha values, border widths and corner radii.
`CandidateUI` uses a layered window with premultiplied BGRA pixels. GDI draws
anti-aliased white text to a black coverage mask using the selected native font;
the renderer composes the theme and glyph coverage before UpdateLayeredWindow.
This preserves foreground opacity when the background is transparent. Selection
uses the same measured rectangles as mouse hit testing. Corners use four coverage
samples per pixel. This implementation does not claim pixel-identical WPF output.

`tests/theme_parity.py` extracts all nine palette declarations directly from the
read-only original source, checks unknown-name fallback and independently checks
60 alpha-compositing samples. The native probe also checks premultiplied-channel
invariants over all output pixels. Linux and ARM64 pass. A 20-iteration 600x200
surface measurement averages approximately 0.50 ms on ARM64 after an interior
fast path; this excludes GDI mask creation, window updates and full input latency.
Results: `build/theme-parity-linux.json`, `build/theme-parity-arm64.json`.

The layered window hook builds for ARM64 but is not yet validated in an installed
application. Current TSF native-mouse tests assume the default theme and capture
the rendered client surface to locate its highlight. The previous installed
screenshots/test passes do not validate the pending renderer. Configuration UI,
delayed expansion and full application/DPI/large-font coverage remain required.
