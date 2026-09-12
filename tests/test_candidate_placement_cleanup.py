"""Runner-only placement/core cleanup and exit-status regressions (no compiler).

The production run_test/main result checks still execute. Only subprocess work
and deletion failures are injected; Windows also tests real deletion-denying
handles at the placement runner boundary, without installing the IME.
"""
from contextlib import ExitStack, redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, call, patch

import candidate_placement_test as placement
import run_core_tests as core
import work_directory as cleanup
from test_candidate_ui_presentation_cleanup import locked_file

HERE = Path(__file__).resolve().parent
EXPECTED = 'Overflow must slide to the work-area bottom, not flip above the caret'


def execute_probe(command, scenario, **kwargs):
    """Return the same kind of result/exception as subprocess.run for each phase."""
    if command[0] == 'cl':
        output = next((arg for arg in command if arg.startswith('/Fe:')), '')
        if 'candidate_placement_legacy' in output:
            phase = 'legacy_compile'
        elif 'candidate_placement' in output:
            phase = 'placement_compile'
        else:
            phase = 'core_compile' if '/c' in command else 'core_link'
    else:
        name = Path(command[0]).name
        phase = ('legacy_run' if name == 'candidate_placement_legacy.exe' else
                 'placement_run' if name == 'candidate_placement.exe' else 'core_run')
    if scenario == phase:
        raise subprocess.CalledProcessError(3, command)
    if scenario == 'timeout' and phase == 'legacy_run':
        raise subprocess.TimeoutExpired(command, 30)
    if phase == 'placement_run':
        if scenario == 'body_permission':
            raise PermissionError('probe launch denied')
        if scenario == 'exit':
            raise SystemExit(7)
        if scenario == 'interrupt':
            raise KeyboardInterrupt()
    if phase == 'legacy_run':
        code = {'control_passed': 0, 'control_crashed': 3}.get(scenario, 1)
        return subprocess.CompletedProcess(command, code, '',
                                           'wrong reason' if scenario == 'wrong_control' else EXPECTED)
    return subprocess.CompletedProcess(command, 0)


def run_script_fixture(entry, scenario, transient=False):
    # The outer fixture owns its real files; the runner receives directories
    # whose cleanup can fail. __exit__ deliberately mimics TemporaryDirectory,
    # so reverting to a raw `with TemporaryDirectory` reproduces the old error.
    with tempfile.TemporaryDirectory(prefix='tigirl-placement-fixture-') as tmp:
        root = Path(tmp)
        directories = []

        class Directory:
            def __init__(self, *, prefix):
                self.name = str(root / (prefix + str(len(directories))))
                Path(self.name).mkdir()
                locked = PermissionError(13, 'Access is denied',
                                         str(Path(self.name) / 'candidate_placement_legacy.exe'))
                locked.winerror = 5
                self.cleanup = Mock(side_effect=[locked, None] if transient else locked)
                directories.append(self)

            def __enter__(self):
                return self.name

            def __exit__(self, *exc):
                self.cleanup()

        windows = SimpleNamespace(name='nt', environ=os.environ)
        with (patch.object(placement, 'os', windows), patch.object(core, 'os', windows),
              patch.object(placement.shutil, 'which', return_value='cl'),
              patch.object(cleanup.tempfile, 'TemporaryDirectory', side_effect=Directory),
              patch.object(cleanup.time, 'sleep') as sleep,
              patch.object(placement.subprocess, 'run',
                           side_effect=lambda command, **kwargs: execute_probe(command, scenario, **kwargs)),
              patch.object(sys, 'argv', ['runner', '--cxx', 'cl'])):
            if entry == 'placement':
                placement.run_test('cl', negative_control=scenario != 'no_control')
            else:
                # Keep the actual nested placement run, not a stub: a cleanup
                # warning must not prevent the remaining core probes from running.
                core.main()
        return directories, sleep.call_args_list


class RunnerExitTests(unittest.TestCase):
    def test_shared_helper_is_used_by_both_entrypoints(self):
        self.assertIs(placement.temporary_work_directory, cleanup.temporary_work_directory)
        self.assertIs(core.temporary_work_directory, cleanup.temporary_work_directory)

    def test_transient_locks_retry_in_both_entrypoints(self):
        for entry, count in [('placement', 1), ('core', 2)]:
            with self.subTest(entry=entry), redirect_stderr(io.StringIO()) as stderr, redirect_stdout(io.StringIO()):
                directories, sleeps = run_script_fixture(entry, 'success', transient=True)
                self.assertEqual(len(directories), count)
                self.assertTrue(all(d.cleanup.call_count == 2 for d in directories))
                self.assertEqual(sleeps, [call(0.1)] * count)
                self.assertEqual(stderr.getvalue(), '')

    def test_persistent_locks_preserve_real_exit_codes_under_werror(self):
        scenarios = {
            'success': (0, None), 'placement_compile': (1, 'CalledProcessError'),
            'placement_run': (1, 'CalledProcessError'), 'legacy_compile': (1, 'CalledProcessError'),
            'timeout': (1, 'TimeoutExpired'), 'body_permission': (1, 'probe launch denied'),
            'control_passed': (1, 'Legacy policy was not rejected'),
            'wrong_control': (1, 'Legacy policy was not rejected'),
            'control_crashed': (1, 'Legacy policy was not rejected'),
            'exit': (7, None), 'interrupt': (None, 'KeyboardInterrupt'),
        }
        code = ('import sys; from test_candidate_placement_cleanup import run_script_fixture; '
                'run_script_fixture(sys.argv[1], sys.argv[2])')
        for entry in ['placement', 'core']:
            cases = dict(scenarios)
            if entry == 'core':
                cases.update({phase: (1, 'CalledProcessError')
                              for phase in ['core_compile', 'core_link', 'core_run']})
            else:
                cases['no_control'] = (0, None)
            for scenario, (exit_code, error) in cases.items():
                with self.subTest(entry=entry, scenario=scenario):
                    result = subprocess.run([sys.executable, '-Werror', '-c', code, entry, scenario],
                                            cwd=HERE, capture_output=True, text=True, timeout=30)
                    if exit_code is None:
                        self.assertNotEqual(result.returncode, 0, result.stderr)
                    else:
                        self.assertEqual(result.returncode, exit_code, result.stderr)
                    reports = [json.loads(line) for line in result.stderr.splitlines()
                               if line.startswith('{"phase":')]
                    count = 2 if entry == 'core' and (scenario == 'success' or scenario.startswith('core_')) else 1
                    self.assertEqual(len(reports), count, result.stderr)
                    for report in reports:
                        self.assertEqual(report['status'], 'warning')
                        self.assertEqual(report['attempts'], 6)
                        self.assertTrue(report['test_result_unchanged'])
                        self.assertIn('candidate_placement_legacy.exe', report['error'])
                    if error:
                        self.assertIn(error, result.stderr)
                    if exit_code == 0:
                        self.assertNotIn('Traceback', result.stderr)
                        if scenario != 'no_control':
                            self.assertIn('"legacy_policy_rejected": true', result.stdout)
                        if entry == 'core':
                            for name in core.PROBES:
                                self.assertIn(f'"probe": "{name}"', result.stdout)


@unittest.skipUnless(os.name == 'nt', 'Windows file sharing semantics required')
class WindowsPlacementLockTests(unittest.TestCase):
    def test_real_legacy_file_lock_through_placement_runner(self):
        for transient in [True, False]:
            with self.subTest(transient=transient):
                directories = []
                real_directory = tempfile.TemporaryDirectory
                release = None

                def directory(**kwargs):
                    result = real_directory(**kwargs)
                    directories.append(result)
                    return result

                with ExitStack() as handles:
                    def execute(command, **kwargs):
                        nonlocal release
                        if Path(command[0]).name == 'candidate_placement_legacy.exe':
                            file = Path(command[0])
                            file.write_bytes(b'lock fixture, not an executable')
                            release = handles.enter_context(locked_file(file))
                        return execute_probe(command, 'success', **kwargs)

                    try:
                        with (patch.object(cleanup.tempfile, 'TemporaryDirectory', side_effect=directory),
                              patch.object(placement.shutil, 'which', return_value='cl'),
                              patch.object(placement.subprocess, 'run', side_effect=execute),
                              patch.object(cleanup.time, 'sleep',
                                           side_effect=lambda _: release() if transient else None) as sleep,
                              redirect_stderr(io.StringIO()) as stderr, redirect_stdout(io.StringIO())):
                            placement.run_test('cl')
                        self.assertIsNotNone(release, 'Legacy-file lock was not exercised')
                        self.assertEqual(len(directories), 1)
                        work = Path(directories[0].name)
                        if transient:
                            sleep.assert_called_once_with(0.1)
                            self.assertFalse(work.exists())
                            self.assertEqual(stderr.getvalue(), '')
                        else:
                            self.assertEqual(sleep.call_args_list, [call(d) for d in cleanup.CLEANUP_RETRY_DELAYS])
                            report = json.loads(stderr.getvalue())
                            self.assertEqual(report['directory'], str(work))
                            self.assertEqual(report['attempts'], 6)
                            self.assertTrue(report['test_result_unchanged'])
                            self.assertTrue((work / 'candidate_placement_legacy.exe').exists())
                    finally:
                        handles.close()
                        for item in directories:
                            item.cleanup()


if __name__ == '__main__':
    unittest.main()
