# Candidate presentation and single-thread animation

Aligned with TigerClaw `538096b` (following `d89a242` atomic publication).
The TSF input thread updates its candidate snapshot synchronously, then posts
one private window message. Further updates replace that snapshot while the
message is pending. DirectWrite initialization, layout and rendering happen
when the message is dispatched, outside the ordinary synchronous key callback.
Candidate enumeration and committing text do not wait for this dispatch.

A hidden candidate appears only after its complete offscreen bitmap has been
published through `UpdateLayeredWindow`. First appearance and hiding have no
animation. Only position and size changes of an already visible candidate
animate, using the upstream smoothstep timeline. Text stays at its final font
size and is clipped to the current frame, never scaled.

Animation geometry retains the final layout's fractional pixel extent. Surface
dimensions round up for allocation, but this padding must not enlarge the painted
skin: width-only transitions keep the bottom edge fixed, and the final animated
frame matches the cached still frame pixel for pixel at every DPI.

SSF layouts may opt into a smaller canvas with `min_width` / `min_height`
(positive integers, 1–8192 in authored image coordinates). Independent code and
candidate layouts prepend `pinyin_` / `zhongwen_` to these keys. Absent or invalid
values retain the source image size on that axis. The renderer also enforces both
fixed end slices plus one source unit, and grows for text plus its insets. It never
moves text or squeezes fixed artwork to satisfy these minima. Rain Bamboo / Snow
Lily / Neon Arcade use horizontal widths 1455 / 1575 / 1175 instead of 1774.
Their vertical minima are 1235×1185 / 1230×1065 / 1225×1205 instead of 1254×1254;
the vertical stretch bands remain above the character. Multiple candidate rows
can make the resulting height exceed the configured floor.

Before drawing SSF slices, the renderer prefilters the background to **1.5 times**
the physical font-scale/DPI resolution, capped at the source dimensions, with WIC
HighQualityCubic. This is profile **E** from the
[desktop sampling review](../design/skin-sampling-review-20261017/index.html).
Unlike D2D 1.0's four-texel bilinear sampler, this
[WIC filter widens its kernel for strong reductions](https://learn.microsoft.com/en-us/windows/win32/api/wincodec/ne-wincodec-wicbitmapinterpolationmode).
Filtering uses premultiplied BGRA (including composited APNG frames), with color
channels clamped to alpha after cubic reconstruction. Each reduced raster then
receives a 3×3 Gaussian unsharp mask (amount **0.30**, RGB detail threshold **1/255**).
It operates on straight RGB, re-premultiplies and clamps each channel to the
unchanged alpha. Only neighborhoods whose nine alpha values are all at least
250/255 qualify; translucent edges and the outer pixel border remain untouched.
This includes the 253/254-alpha interiors common in the skin PNGs. Small reductions
whose intermediate raster reaches source size, native-size and enlarged art keep
the original pixels. DirectWrite text is never part of this filtering.
Bitmap DPI maps the reduced pixels back to authored source coordinates, retaining
the existing slice boundaries, physical-edge snapping, insets and hit rectangles.
This filters the original art before nine-slice stretching; extreme compression
of a stretch band still uses D2D bilinear sampling of the prefiltered image.

An eight-entry, 16 MiB CPU LRU per renderer/image caches the filtered and sharpened
results by APNG frame and pixel dimensions. It survives target recreation during
size animations and is
cleared when the image changes. D2D bitmaps remain render-target local. Results
larger than the cache budget are uploaded without retaining the filtered CPU
copy. No new user setting, source-image rewrite or image-processing thread is
introduced.

`SsfScalingProbe` covers the sharpened near-opaque interiors, unchanged alpha and
translucent neighborhoods, cache eviction, target/DPI recreation, native-size
identity and APNG advancement. The three-pixel stripe fixture permits at most
10% channel-range RMS residual contrast: profile E retains some fine texture
(21/255 worst case at 192 DPI), versus about 84/255 without prefiltering. This
replaces the more smoothing-oriented 12/255 limit used before profile E.
The [production E verification](../design/skin-sampling-review-20261017/production-e/index.html)
records desktop pixel parity with the selected draft and architecture/build results.

A 10 ms `WM_TIMER` request draws at most one frame per dispatch. Elapsed wall
clock time selects the frame, so delayed ticks skip forward rather than
extending the animation. A changed target starts from the currently published
rectangle; an unchanged target does not restart the timeline. A pending input
refresh takes precedence over an old animation frame. Commit, cancel, UI suppression and teardown stop animation and hide immediately.
A transient `TS_E_NOLAYOUT` from the current composition freezes the last
published frame for at most 100 ms and disables its hit targets. A layout
notification within that interval resumes animation from the displayed rectangle.
The deadline does not restart on repeated failures; expiry hides the window.
Other missing-caret failures still hide immediately. This prevents asynchronous
host text layout from turning each append into an unanimated first appearance.

Candidate movement and resizing animation is always enabled with a fixed
70 ms duration. The appearance page no longer exposes animation controls;
legacy `候选窗动效` and `候选窗动效时间(毫秒)` settings are ignored.

The renderer caches unchanged layouts and final pixels, and reuses the GDI
backing surface when its size is unchanged. Individual frames are prepared
fully before pixels and geometry are published together. Destination-monitor
DPI is resolved under PMv2. Placement and candidate hit rectangles are committed
only after successful publication. Pending refreshes disable stale hit targets;
clicks outside the currently visible frame are rejected. Render failures stop
the animation and permit up to three timed retries before awaiting new input.
Revision checks discard results superseded by reentrant updates or teardown.

There is no additional thread or process, no sleep loop and no synchronous
DWM flush. This is cooperative scheduling, not rendering isolation: each frame
still occupies the TSF thread while it is being drawn, and a host that does not
pump messages can delay presentation. Native menu loops may dispatch posted
refreshes. Candidate/annotation reveal delays remain independent content rules.

Validation:

- Build `tests/SsfScalingProbe.vcxproj`, then run
  `python tests/ssf_scaling_test.py --probe build/tests/ARM64/ssf_scaling_probe.exe`:
  synthetic fine stripes, transparent colored pixels, native-size identity,
  APNG advancement, target recreation and DPI/cache-eviction round trips.
  `--skins resources/Skins` additionally captures all five layouts at four DPIs
  and measures cold rendering and repeated animation-frame rendering separately.
- Build `tests/CandidateAnimationRenderProbe.vcxproj`, then run
  `candidate_animation_render_probe.exe resources/Skins data/fonts/LXGWWenKaiGBScreen.ttf`:
  private-font `d` → `ds` layouts, fixed-edge width transitions and identical final
  pixels across five layouts, three font sizes and 96/120/144/192 DPI, using all
  bundled skins and the built-in fallback. Runs offscreen without user input.
  Omit the font path to use Segoe UI in clean CI without downloaded font data.
- `python3 tests/candidate_reveal_test.py`: reveal deadlines, transition
  sampling/retargeting/cancellation and configuration boundaries.
- `python3 tests/candidate_render_test.py`: real DirectWrite rendering and
  layered publication on ARM64, x64 and x86, including clipping without text
  scaling and atomic geometry changes.
- `python3 tests/candidate_mouse_test.py --async`: private TSF activation and
  explicit key callbacks verify coalescing, deferred initial drawing,
  intermediate size frames, input during animation and immediate commit/hide.
  This mode does not validate physical focus or menus.
- `python3 tests/candidate_mouse_test.py`: separate foreground mouse/menu test;
  requires the test host to obtain OS foreground focus.
- `python3 tests/input_settings_test.py`: native controls at 96/144/192 DPI,
  preserving the fixed animation behavior when saving and reopening settings.

These checks do not establish perceived smoothness on physical displays,
cross-monitor transitions, or behavior inside each real application/UWP host.
