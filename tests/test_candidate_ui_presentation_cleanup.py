"""Runner-only regressions; no MSVC, IME installation or model needed."""
from contextlib import contextmanager, redirect_stderr
import ctypes
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import traceback
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, call, patch

import candidate_ui_presentation_test as runner
import work_directory as cleanup

HERE = Path(__file__).resolve().parent
EXPECTED = "First candidates did not publish final geometry atomically"


class CleanupTests(unittest.TestCase):
    def setUp(self):
        self.directory = Mock()
        self.directory.name = "unused-work-directory"
        self.stderr = io.StringIO()
        self.enterContext(redirect_stderr(self.stderr))
        self.enterContext(patch.object(cleanup.tempfile, "TemporaryDirectory", return_value=self.directory))
        self.sleep = self.enterContext(patch.object(cleanup.time, "sleep"))

    def warning(self):
        lines = self.stderr.getvalue().splitlines()
        self.assertEqual(len(lines), 1)
        report = json.loads(lines[0])
        self.assertEqual(report["phase"], "cleanup")
        self.assertEqual(report["status"], "warning")
        self.assertEqual(report["directory"], self.directory.name)
        self.assertEqual(report["attempts"], 6)
        self.assertTrue(report["test_result_unchanged"])
        return report

    def test_success_has_no_delay_or_warning(self):
        with cleanup.temporary_work_directory(prefix="tigirl-ui-presentation-") as work:
            self.assertEqual(work, Path(self.directory.name))
        self.directory.cleanup.assert_called_once_with()
        self.sleep.assert_not_called()
        self.assertEqual(self.stderr.getvalue(), "")

    def test_each_transient_failure_count_recovers_with_bounded_waits(self):
        self.assertEqual(cleanup.CLEANUP_RETRY_DELAYS, (0.1, 0.2, 0.4, 0.8, 1.6))
        for failures in range(1, 6):
            with self.subTest(failures=failures):
                self.directory.cleanup.reset_mock()
                self.sleep.reset_mock()
                self.directory.cleanup.side_effect = [PermissionError("legacy.exe locked")] * failures + [None]
                with cleanup.temporary_work_directory(prefix="tigirl-ui-presentation-"):
                    pass
                self.assertEqual(self.directory.cleanup.call_count, failures + 1)
                self.assertEqual(self.sleep.call_args_list,
                                 [call(d) for d in cleanup.CLEANUP_RETRY_DELAYS[:failures]])
                self.assertEqual(self.stderr.getvalue(), "")

    def test_exhaustion_reports_last_error_and_does_not_fail_success(self):
        self.directory.cleanup.side_effect = [PermissionError(f"lock-{i}") for i in range(6)]
        with cleanup.temporary_work_directory(prefix="tigirl-ui-presentation-"):
            pass
        self.assertEqual(self.directory.cleanup.call_count, 6)
        self.assertEqual(self.sleep.call_args_list, [call(d) for d in cleanup.CLEANUP_RETRY_DELAYS])
        self.assertIn("lock-5", self.warning()["error"])

    def test_oserror_cleanup_also_reports_without_masking_result(self):
        self.directory.cleanup.side_effect = OSError("cleanup I/O error")
        with cleanup.temporary_work_directory(prefix="tigirl-ui-presentation-"):
            pass
        self.assertEqual(self.directory.cleanup.call_count, 6)
        self.assertIn("OSError", self.warning()["error"])

    def test_body_error_identity_and_traceback_survive_exhausted_cleanup(self):
        self.directory.cleanup.side_effect = PermissionError("legacy.exe locked")
        failure = RuntimeError("functional assertion failed")

        def fail_body():
            raise failure

        try:
            with cleanup.temporary_work_directory(prefix="tigirl-ui-presentation-"):
                fail_body()
        except RuntimeError as error:
            self.assertIs(error, failure)
            self.assertIn("fail_body", [frame.name for frame in traceback.extract_tb(error.__traceback__)])
        else:
            self.fail("Functional error was swallowed")
        self.warning()

    def test_body_oserror_is_not_mistaken_for_cleanup_failure(self):
        failure = PermissionError("cannot write test fixture")
        with self.assertRaises(PermissionError) as caught:
            with cleanup.temporary_work_directory(prefix="tigirl-ui-presentation-"):
                raise failure
        self.assertIs(caught.exception, failure)
        self.directory.cleanup.assert_called_once_with()
        self.assertEqual(self.stderr.getvalue(), "")

    def test_compiler_timeout_exit_and_interrupt_are_not_swallowed(self):
        failures = [subprocess.CalledProcessError(2, ["cl", "/c"]),
                    subprocess.TimeoutExpired(["legacy.exe"], 90),
                    SystemExit(7), KeyboardInterrupt()]
        for failure in failures:
            with self.subTest(error=type(failure).__name__):
                self.directory.cleanup.side_effect = PermissionError("legacy.exe locked")
                with self.assertRaises(type(failure)) as caught:
                    with cleanup.temporary_work_directory(prefix="tigirl-ui-presentation-"):
                        raise failure
                self.assertIs(caught.exception, failure)

    def test_body_failure_also_survives_successful_cleanup_retry(self):
        failure = RuntimeError("functional error")
        self.directory.cleanup.side_effect = [PermissionError("locked"), None]
        with self.assertRaises(RuntimeError) as caught:
            with cleanup.temporary_work_directory(prefix="tigirl-ui-presentation-"):
                raise failure
        self.assertIs(caught.exception, failure)
        self.assertEqual(self.directory.cleanup.call_count, 2)
        self.assertEqual(self.stderr.getvalue(), "")

    def test_unexpected_cleanup_programming_error_is_not_silenced(self):
        self.directory.cleanup.side_effect = RuntimeError("cleanup implementation bug")
        with self.assertRaisesRegex(RuntimeError, "cleanup implementation bug"):
            with cleanup.temporary_work_directory(prefix="tigirl-ui-presentation-"):
                pass
        self.sleep.assert_not_called()


# Called in a fresh Python process to check the runner's real exit status, not
# just a helper's return value. Only compiler/probe execution and cleanup fail
# artificially; main() still performs its original result/control checks.
def run_script_fixture(scenario):
    with tempfile.TemporaryDirectory(prefix="tigirl-runner-fixture-") as temp:
        root = Path(temp)
        work = root / "work"
        work.mkdir()
        (root / "native/tsf").mkdir(parents=True)
        (root / "tests").mkdir()
        (root / "native/tsf/CandidateUI.cpp").write_text(
            "!firstCandidateFrame && IsWindowVisible(window_)", encoding="utf-8")
        (root / "tests/candidate_ui_presentation_probe.cpp").write_text(
            '#include "../native/tsf/CandidateUI.cpp"', encoding="utf-8")
        directory = Mock()
        directory.name = str(work)
        directory.cleanup.side_effect = PermissionError("legacy.exe locked")

        def execute(command, **kwargs):
            is_legacy = any("legacy" in arg for arg in command)
            phase = ("legacy_" if is_legacy else "")
            phase += "compile" if "/c" in command else "link" if "/link" in command else "run"
            if scenario == phase:
                raise subprocess.CalledProcessError(2, command, stderr="compiler/linker sentinel")
            if scenario == "timeout" and phase == "legacy_run":
                raise subprocess.TimeoutExpired(command, 90)
            if phase == "run":
                return subprocess.CompletedProcess(command, 3 if scenario == "functional" else 0,
                                                   '{"probe":"executed"}\n', "functional sentinel")
            if phase == "legacy_run":
                return subprocess.CompletedProcess(command, 0 if scenario == "control_passed" else 1,
                                                   "", "wrong failure" if scenario == "wrong_control" else EXPECTED)
            return subprocess.CompletedProcess(command, 0)

        with (patch.object(runner, "ROOT", root), patch.object(runner, "COMMON", []),
              patch.object(runner, "os", SimpleNamespace(name="nt")),
              patch.object(runner.shutil, "which", return_value="cl"),
              patch.object(cleanup.tempfile, "TemporaryDirectory", return_value=directory),
              patch.object(cleanup.time, "sleep"), patch.object(runner.subprocess, "run", side_effect=execute),
              patch.object(sys, "argv", ["runner", "--negative-control"])):
            runner.main()


class RunnerExitTests(unittest.TestCase):
    def test_cleanup_warning_preserves_actual_main_exit_codes_under_werror(self):
        scenarios = {"success": (0, None), "compile": (1, "CalledProcessError"),
                     "link": (1, "CalledProcessError"), "functional": (1, "functional sentinel"),
                     "legacy_compile": (1, "CalledProcessError"), "legacy_link": (1, "CalledProcessError"),
                     "timeout": (1, "TimeoutExpired"),
                     "control_passed": (1, "Negative control did not detect"),
                     "wrong_control": (1, "Negative control did not detect")}
        code = "import sys; from test_candidate_ui_presentation_cleanup import run_script_fixture; run_script_fixture(sys.argv[1])"
        for scenario, (exit_code, error) in scenarios.items():
            with self.subTest(scenario=scenario):
                result = subprocess.run([sys.executable, "-Werror", "-c", code, scenario],
                                        cwd=HERE, capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, exit_code, result.stderr)
                reports = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{"phase":')]
                self.assertEqual(len(reports), 1, result.stderr)
                self.assertEqual(reports[0]["status"], "warning")
                self.assertEqual(reports[0]["attempts"], 6)
                if error:
                    self.assertIn(error, result.stderr)
                    self.assertNotIn('"negative_control":"passed"', result.stdout)
                else:
                    self.assertIn('"negative_control":"passed"', result.stdout)
                    self.assertNotIn("Traceback", result.stderr)


class RealDirectoryTests(unittest.TestCase):
    def test_readonly_and_nested_files_are_removed(self):
        with cleanup.temporary_work_directory(prefix="tigirl-ui-presentation-") as work:
            nested = work / "nested"
            nested.mkdir()
            file = nested / "legacy.exe"
            file.write_bytes(b"fixture, not an executable")
            file.chmod(stat.S_IREAD)
        self.assertFalse(work.exists())

    def test_already_removed_directory_is_successful(self):
        with cleanup.temporary_work_directory(prefix="tigirl-ui-presentation-") as work:
            work.rmdir()
        self.assertFalse(work.exists())


@contextmanager
def locked_file(path):
    # Test an actual Windows deletion denial, with no thread or timing race.
    from ctypes import wintypes
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    create = kernel.CreateFileW
    create.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                       wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    create.restype = wintypes.HANDLE
    close = kernel.CloseHandle
    close.argtypes = [wintypes.HANDLE]
    close.restype = wintypes.BOOL
    handle = create(str(path), 0x80000000, 0x1 | 0x2, None, 3, 0, None)
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())

    def release():
        nonlocal handle
        if handle is not None:
            if not close(handle):
                raise ctypes.WinError(ctypes.get_last_error())
            handle = None
    try:
        yield release
    finally:
        release()


@unittest.skipUnless(os.name == "nt", "Windows file sharing semantics required")
class WindowsLockTests(unittest.TestCase):
    def test_real_locked_legacy_exe_succeeds_after_handle_is_released(self):
        directory = tempfile.TemporaryDirectory(prefix="tigirl-lock-test-")
        work = Path(directory.name)
        (work / "legacy.exe").write_bytes(b"lock fixture")
        try:
            with locked_file(work / "legacy.exe") as release:
                with (patch.object(cleanup.time, "sleep", side_effect=lambda _: release()) as sleep,
                      redirect_stderr(io.StringIO()) as stderr):
                    cleanup.cleanup_work_directory(directory)
                sleep.assert_called_once_with(0.1)
                self.assertEqual(stderr.getvalue(), "")
                self.assertFalse(work.exists())
        finally:
            directory.cleanup()

    def test_real_persistent_lock_warns_and_retains_residual_path(self):
        directory = tempfile.TemporaryDirectory(prefix="tigirl-lock-test-")
        work = Path(directory.name)
        (work / "legacy.exe").write_bytes(b"lock fixture")
        try:
            with locked_file(work / "legacy.exe"):
                with patch.object(cleanup.time, "sleep") as sleep, redirect_stderr(io.StringIO()) as stderr:
                    cleanup.cleanup_work_directory(directory)
                self.assertEqual(sleep.call_args_list, [call(d) for d in cleanup.CLEANUP_RETRY_DELAYS])
                report = json.loads(stderr.getvalue())
                self.assertEqual(report["directory"], str(work))
                self.assertEqual(report["attempts"], 6)
                self.assertEqual(report["status"], "warning")
                self.assertTrue((work / "legacy.exe").exists())
        finally:
            directory.cleanup()
        self.assertFalse(work.exists())


if __name__ == "__main__":
    unittest.main()
