# First candidate presentation

A visible encoding placeholder is not a previously presented candidate list.
`CandidateUI::hasPresentedCandidates_` belongs to the native UI display session,
not to the decoder generation, HWND visibility, WM_PAINT, or cached bitmap.

After the existing `CandidateReveal` delay policy produces nonempty items, the
first candidate frame cancels any running geometry transition and publishes the
final bitmap, position and size together. The latch is set only after a successful
publication/show of the current frame with actual rendered candidate rows.
Preparation, rendering, publication or show failure leaves it unlatched so the
existing bounded retry also publishes directly. No additional delay is introduced.

Subsequent updates use the existing transition, including delayed annotation
expansion. Empty/pending decode results do not clear the latch. Explicit Show(FALSE),
detach, composition end, window destruction and actual layout-timeout hiding do.
A short TS_E_NOLAYOUT grace period freezes the old frame without clearing the latch;
layout recovery still animates as before. CandidateReveal's clocks and latching,
FrameTransition's interpolation/duration, and the worker thread are unchanged.

## Windows regression test

From either an x64 or x86 MSVC Developer PowerShell:

```powershell
python tests/candidate_ui_presentation_test.py --negative-control
```

The test compiles the production CandidateUI.cpp into an isolated executable.
It uses real layered windows, DirectWrite/Direct2D rendering, GDI backing surfaces,
UpdateLayeredWindow and the production Engine. It supplies a link-time unactivated
TSF-owner double, a controlled clock, manually delivered timer messages, and
one-shot failures at backing-surface allocation, publication and window show.
No test hooks, extra threads or delays are compiled into the production DLL.
The private friend grants test assertions access without adding a public API.
No IME is registered/installed, no user data is used, and no model/fonts are downloaded.

Both horizontal and vertical layouts cover:

- Encoding placeholder to first candidates: one complete final-sized publication;
  an existing code-only geometry transition is cancelled.
- Next candidate update: intermediate geometry remains animated.
- First backing-surface/publication/show failure: retry remains a first presentation.
- Explicit hide and a new composition: first-presentation rules run again.
- Empty results do not reset the session; candidate/annotation delays retain their
  existing deadlines; the later annotation expansion animates.
- Short layout loss and recovery preserve animation; actual timeout resets the
  latch; an old layout timer cannot hide a recovered frame.
- Finalize/Abort during animation detach immediately; queued timer/refresh messages
  cannot publish again. The mock owner checks Engine output, not COM document edits.
- Reentrant hide during publication/show and external window destruction cannot
  latch or revive an obsolete frame.

The negative control changes only the animation condition in a temporary source
copy back to visibility-only. It must fail the first-candidate final-geometry
assertion (a compiler failure does not count). Tracked sources are never mutated.

This is an actual window/rendering regression, not physical-keyboard acceptance,
real QQ validation, a full TSF document-transaction test, or full-model sentence
quality evaluation. Those remain separate checks.

## Test-runner cleanup

Windows may temporarily deny deletion of an executable after its process exits.
The runner explicitly cleans up its TemporaryDirectory, with at most six passes
and waits of 0.1, 0.2, 0.4, 0.8 and 1.6 seconds between failures (3.1 seconds of
retry waits total). Successful cleanup has no added wait. It retains the standard
handling of read-only files and partially removed directories.

If every pass raises an OSError, stderr gets a separate JSON cleanup warning with
the residual directory, attempts and last error. The directory may need manual
removal once the lock is released. This warning does not change the functional
result or exit code, even under `-Werror`. Compilation, linking, functional probe,
negative-control failures, timeouts and interrupts still propagate; compilation
failure is never accepted as a successful negative control.

```sh
python tests/test_candidate_ui_presentation_cleanup.py -v
```

These runner regressions execute on Windows and Linux without a compiler. They
inject cleanup failures and check real child-process exit codes. Windows also
holds a real file handle without delete sharing to test transient and persistent
locks, without background threads or timing-dependent sleeps. The native window
regressions above remain separate and still run in both Windows CI jobs.
