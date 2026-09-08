# Uppercase numeric input

Uppercase mode now converts valid `S` amounts when committed with Space,
Enter or punctuation. `S123.45` produces `壹佰贰拾叁元肆角伍分`.
The code grammar and output follow the original Core, including its empty
output for `S0`, `壹分` for `S0.005`, and `数字格式错误!` for decimal overflow.
Invalid amount syntax remains literal uppercase input.

`UppercaseText.cpp` uses decimal digit strings instead of floating point for
amounts. It reproduces decimal's 96-bit coefficient and scale limit 28 with
nearest-even parsing, followed by the original custom currency format's
rounding to two places. Units extend through 穰. Numeric-prefix recognition
uses invariant ASCII float syntax, matching the strings that uppercase typing
can produce, including exponents and NaN/Infinity. The helpers do not depend
on COM, a Core process, locale-sensitive numeric parsing or IPC.

Unshifted period/comma stays in the buffer only when the original numeric-prefix
predicate succeeds. For example, after `S1` a period stays in composition;
after bare `S` it commits `S.`. After a comma, the prefix no longer parses as
a Float, so the next separator may commit. The original predicate accepts
`D` followed by a number, but does not accept `Ds` followed by a number.
Those quirks are preserved; this is not a new unrestricted numeric editor.

## Verification

Build ReferenceOracle and Linux/ARM64 UppercaseProbe and EngineProbe, then run:

```
g++ -std=c++17 -Wall -Wextra -Werror -O2 -Inative native/UppercaseText.cpp native/Text.cpp tests/uppercase_probe.cpp -o build/uppercase_probe
python3 tests/currency_parity.py
python3 tests/currency_key_parity.py
```

- 1,853 direct conversion/prefix cases match the original on both platforms,
  including 96-bit overflow, long fractional input, midpoint rounding,
  zero/group boundaries and seeded random decimal strings.
- 4,574 key events match original result and candidate snapshots on both
  platforms. They cover commit/cancel/editing, punctuation and mode toggles.
- Existing 35,554-event ordinary-input regression remains unchanged on both
  platforms. Reports are `build/currency-parity-*.json`,
  `build/currency-key-parity-*.json`, `build/currency-default-regression-*.json`.
  They record executable hashes.

The oracle invokes the untouched original currency and prefix methods and
runs only in disposable filesystem/registry staging. No timer is scheduled
by these tests. Neither a finite corpus nor component tests prove full runtime
parity; currency has not yet been checked by typing into a real application.

## Remaining uppercase behavior

`parseManualTimer` now implements `Ds`/`DS` recognition and delay calculation.
It distinguishes non-timer text, a consumed timer command with no scheduled
delay (zero/unparseable minutes), and positive minutes converted to milliseconds,
including a valid zero-millisecond result after truncation. Commas, the regex's
single-final-LF behavior, floating-point overflow/underflow and INT_MAX clamping
match the tested original regex/.NET arithmetic.

`tests/manual_timer_parity.py` passes 2,150 cases on Linux and ARM64. The oracle
uses the untouched original TimerRegex and .NET parsing/arithmetic copied from
ScheduleManualTimer; it intentionally does not invoke the scheduling method or
create reminders. Reports: `build/manual-timer-parity-*.json`. This is parser
and arithmetic evidence, not end-to-end timer verification.

Timer commits are now connected to the engine and TSF. The pure engine returns
an explicit `manualTimerMs` action; only actual TSF apply schedules it, outside
secure mode. Space, Enter and punctuation consume matching timer codes, while
CapsLock, Escape and Tab retain their original distinct behavior. Ten native
engine cases on each architecture check delay/output, copied preview isolation
and no scheduling on key release (`build/manual-timer-engine-*.json`).

The earlier `tsf/ManualTimer` implementation uses a message-only window with distinct timer generations,
so queued messages from a replaced timer cannot fire the new reminder. Zero-ms
delays are posted rather than invoked synchronously. The default notification
opens `时间差不多咯！` / `计时器` on the default desktop in a separate thread;
that thread holds a loader reference released by FreeLibraryAndExitThread.
The notification therefore does not wait on the host input thread. An initial
MB_DEFAULT_DESKTOP_ONLY test could not observe/close a popup; explicit desktop
selection in the worker now passes actual caption/text/close checks.

The ARM64 scheduler probe passes replacement, invalid-delay preservation,
cancellation, one-shot firing and closing/releasing the scheduler during its
callback; it also displays and dismisses the actual default reminder. Report:
`build/manual-timer-ui-arm64.json`. Rebuild `tests/ManualTimerUIProbe.vcxproj`
and run its executable to reproduce this short popup test.

Timer commands now use `ReminderLaunch` and the separate `timer_reminder.exe`.
The helper loads no dictionary and processes no input; it exits after replacement
or dismissal. The TSF adapter reserves a ticket in `user/timer.txt`, starts the
helper with unique startup events, waits for readiness, publishes the accepted
ticket/deadline atomically, then releases the helper. Failed startup leaves the
previous accepted timer intact. The helper can also observe parent exit after
publication without depending on another message from that process. Persistent
sequence/epoch checks prevent old requests reviving after all helpers exit.
TSF deactivation no longer cancels these reminders. `ManualTimer` remains the
internal scheduler used by live data refresh, and its historical probes remain
separate from the production reminder path.

`build/reminder-launch-arm64.json` verifies the native launch API, parent-exit
survival, quoted paths and failure preservation. `build/reminder-process-arm64.json`
records the helper/ledger/popup tests at its own executable hash.
`build/timer-host-exit-arm64.json` verifies two independent real TSF service hosts
submit commands, deactivate and exit before the actual reminder: the newer
command replaces the older helper, and the final popup still appears and closes.
Those hosts use explicit TSF key callbacks, not physical keyboard injection.
Real-application coverage, restrictive host job objects, unavailable ledger paths
and locked-desktop behavior remain unresolved.

## Full TSF timer expiry check

`tests/run_timer_tsf.ps1` activates the current private DLL with disposable user
data and submits `Ds1` through ITfKeystrokeMgr preview/dispatch callbacks with
verified OS and TSF focus. This is not physical keyboard injection. Each UI mode exercises 44
events across two contexts and waits for the actual one-minute reminder. It
checks that preview starts no helper, actual submission starts exactly one,
the literal command leaves no committed text, the popup has the expected text,
its button closes it, and the helper process exits after dismissal. Both modes
pass; timing and executable hashes are in
`build/tsf-private-timer-validation.json`. The run takes approximately two minutes.

The reminder test now discovers its actual Button control instead of assuming
IDOK, and waits for window destruction. The previous short-popup test had only
observed text and posted a close request; it did not prove that the request
closed the window. Its strengthened version now passes that check too.
