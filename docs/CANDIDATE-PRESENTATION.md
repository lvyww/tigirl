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

Settings on the appearance page:

- `候选窗动效`: default `是`.
- `候选窗动效时间(毫秒)`: default 200; range 0–60000. Zero or disabling
  animation publishes final geometry immediately.

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
  saving and reopening animation settings alongside existing settings.

These checks do not establish perceived smoothness on physical displays,
cross-monitor transitions, or behavior inside each real application/UWP host.
