# DirectWrite / Direct2D candidate migration

Goal: move candidate drawing to DirectWrite + Direct2D with WPF-like text
quality and per-monitor DPI behavior, preserving native in-process TSF input.

Implemented:

- DirectWrite natural text layouts in DIPs, with no GDI text measurement or
  white-on-black text-mask stage. Ellipsis, mixed-script fallback and private
  bundled font collections are supported. Physical surface dimensions round
  up after DPI scaling; text font sizes retain fractional DIP precision.
- Direct2D renders text, selection backgrounds, translucent themes and
  asymmetric rounded outlines into a reusable WIC premultiplied BGRA target.
  Layered-window presentation still uses UpdateLayeredWindow. The WIC target
  uses software Direct2D rendering (no GPU requirement), not GDI glyph drawing.
  Grayscale antialiasing avoids RGB fringes on translucent pixels.
- Candidate HWND is created under scoped PMv2 awareness. Host thread awareness
  is restored; process defaults are never modified. Host virtualized caret
  coordinates are converted before entering the scoped candidate context.
  DPI changes trigger layout, surface resize and hit-region regeneration.
  A monitor change moves the window to the caret monitor before measuring DPI.
- Layout failures hide the candidate and log an error rather than propagating
  a drawing failure into key handling. Rendering target failures discard the
  target for recreation on the next paint.

Evidence:

- `python3 tests/candidate_render_test.py`: actual ARM64/x64/x86 probes pass
  90 rendering cases each: nine themes, two orientations, 96/120/144/192 DPI
  and a return to 96. Checks include bundled family selection, fallback,
  premultiplied alpha, bounding rectangles and deterministic DPI roundtrip.
- Three actual hidden host awareness modes per architecture (unaware,
  system-aware, PMv2) verify coordinate conversion and scope restoration.
  These do not simulate a physical move between differently scaled monitors.
- `build/directwrite-render-validation.json` retains source/font/probe hashes.
  PNGs are in `build/directwrite-render-{ARM64,x64,Win32}`.
- `tests/wpf_candidate_reference.ps1` renders a WPF visual with the same font,
  size and sample strings to PNG at four DPIs. Reference images are in
  `build/wpf-render-reference`. This is a visual comparison, not a live-window
  or pixel-identical layout test. Default vertical 144 DPI and cyberpunk
  horizontal 192 DPI Direct2D images were visually inspected.

Still required before declaring the migration complete:

- Current DLL in actual foreground input: typing, paging and mouse selection,
  candidates above/below the caret, window motion and scrolling.
- Real cross-monitor DPI transitions and system scaling changes, including
  host applications with differing DPI awareness.
- User confirmation of the actual displayed text quality.

Installed generation is now `ccafa500dd2fe896` (ARM64X), with matching x86
supplement `x86-ccafa500dd2fe896-c470c2e8c82ca421`. Both installers finished
successfully. Installed DLL hashes match the tested build. ARM64/x64 system
registered TSF checks pass (`build/registered-tsf-validation.json`); 32-bit
COM activation passes (`build/directwrite-x86-com.json`). Foreground availability
was requested and remains pending. Existing applications must be fully restarted
to load the new DLL. These background checks do not prove visual acceptance.

Hidden TSF fixture isolation was corrected after the initial failures. After
system TIP deactivation, the profile manager may enable another TIP. Removing
its mode button alone does not remove its compartment event sinks. The manual
fixture now selects an already-loaded English keyboard profile FORPROCESS and
asserts that the active profile is that keyboard layout before creating its
manually driven TIP. No ENABLEPROFILE or FORSESSION flags are used. All three
staged activation variants pass without changing production mode logic or
weakening mode/text/lock assertions (`build/directwrite-tsf-background.json`).
This closes the observed hidden-fixture failure, not physical application gates.

Foreground validation of the earlier DirectWrite generation `0e1d68726621238b` (before the Ctrl+Space fix):

- ARM64, x64 and Win32 standard Rich Edit hosts each pass 27 OS-injected key
  taps: commit, cancel, focus return, numeric selection, backspace, newline
  and replacement of selected text. Installed DLL identity is verified.
- ARM64/x64 system-registered TSF foreground fixtures pass both UI-less and
  native candidate UI modes, including layout recovery and selection through
  a direct WM_LBUTTONDOWN message. This is not physical mouse injection.
- Composited candidate screenshots from both architectures were inspected:
  text, selection and border are visible with no apparent clipping.
- Combined evidence: `build/directwrite-frontend-validation.json`. Screenshots:
  `build/registered-candidate-{ARM64,x64}.png`. No third-party application or
  real cross-monitor transition is covered by these fixtures. Foreground
  keyboard/mouse access was released to the user after the tests.

The Ctrl+Space fix is now deployed. Current installed checks are recorded in
`build/ctrl-space-installed-validation.json`; foreground chord acceptance is
pending. Previous foreground screenshots/results describe the earlier DLL.

Current deployment has subsequently advanced to `5aa4bd69945cfb9c` for folder-based schema management.
See `build/folder-installed-validation.json`; earlier foreground evidence retains
its original DLL identity and is not a fresh foreground check of this package.
