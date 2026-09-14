"""Runner-only cleanup/result regressions; no compiler, model or installed IME.

Exercise the actual four review entrypoints with injected subprocess results and
Windows-style deletion errors. Windows additionally uses a real deletion-denying
handle on review.exe. The shared work_directory implementation is not replaced.
"""
from contextlib import ExitStack, redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, call, patch

import bench_sentence_review as benchmark
import compare_sentence_revisions as comparison
import sentence_review_negative as negative
import sentence_review_test as review
import work_directory as cleanup
from test_candidate_ui_presentation_cleanup import locked_file

HERE = Path(__file__).resolve().parent
RUNNERS = {'review': review, 'comparison': comparison,
           'benchmark': benchmark, 'negative': negative}


def execute_fixture(entry, scenario, command, **kwargs):
    """Supply process outcomes, leaving each runner's result checks intact."""
    compiling = command[0] in ('cl', 'g++')
    if compiling:
        assert kwargs.get('check') is True, 'compiler failure must be checked'
        if scenario == 'compile':
            raise subprocess.CalledProcessError(3, command, stderr='compiler sentinel')
        # --keep must still copy a real file before scratch cleanup.
        if entry == 'review':
            output = next(arg[4:] for arg in command if arg.startswith('/Fe:'))
            Path(output).write_bytes(b'probe fixture, not an executable')
        return subprocess.CompletedProcess(command, 0)
    if entry in ('review', 'comparison'):
        assert kwargs.get('check') is True, 'probe failure must be checked'
    if scenario == 'probe':
        raise subprocess.CalledProcessError(5, command, stderr='probe sentinel')
    if scenario == 'timeout':
        raise subprocess.TimeoutExpired(command, 180)
    if scenario == 'body_permission':
        raise PermissionError('probe launch denied')
    if scenario == 'exit':
        raise SystemExit(7)
    if scenario == 'interrupt':
        raise KeyboardInterrupt()
    if entry == 'comparison':
        data = b'snapshot\n'
        if scenario == 'mismatch' and Path(command[0]).parent.name == 'new':
            data = b'different snapshot\n'
        return subprocess.CompletedProcess(command, 0, data, b'')
    if entry == 'negative':
        code = {'control_passed': 0, 'control_crashed': -11}.get(scenario, 1)
        return subprocess.CompletedProcess(command, code, '', 'functional assertion')
    if entry == 'benchmark':
        rows = [dict(test='decode', length=n, repeat_ms=1, append_ms=2,
                     backspace_ms=3, repeat_expanded=0, append_expanded=2,
                     backspace_expanded=0) for n in (80, 128)]
        score = 2 if scenario == 'mismatch' and Path(command[0]).name == 'new' else 1
        rows.append(dict(test='learning_update', ms_per_update=1, score_sum=score))
        return '\n'.join(json.dumps(row) for row in rows) + '\n'
    return subprocess.CompletedProcess(command, 0)


def run_script_fixture(entry, scenario='success', transient=False):
    # This outer directory owns actual fixtures. The runner receives a wrapper
    # that mimics raw TemporaryDirectory.__exit__, so reverting any entrypoint
    # to the old `with TemporaryDirectory` reproduces the original failure.
    with tempfile.TemporaryDirectory(prefix='tigirl-review-cleanup-fixture-') as tmp:
        root = Path(tmp)
        directories = []

        class Directory:
            def __init__(self, *, prefix):
                self.name = str(root / prefix)
                Path(self.name).mkdir()
                error = PermissionError(13, 'Access is denied', str(Path(self.name) / 'review.exe'))
                error.winerror = 5
                self.cleanup = Mock(side_effect=[error, None] if transient else error)
                directories.append(self)

            def __enter__(self):
                return self.name

            def __exit__(self, *exc):
                self.cleanup()

        runner = RUNNERS[entry]
        argv = ['runner', '--cxx', 'cl' if entry == 'review' else 'g++']
        if entry in ('comparison', 'benchmark'):
            argv += ['--baseline', str(review.ROOT), '--report', str(root / 'report.json')]
        if entry == 'benchmark':
            argv += ['--runs', '1']
        if entry == 'review':
            argv += ['--keep', str(root / 'kept')]
        with (patch.object(cleanup.tempfile, 'TemporaryDirectory', side_effect=Directory),
              patch.object(cleanup.time, 'sleep') as sleep,
              patch.object(runner.shutil, 'which', side_effect=lambda name: name),
              patch.object(runner.subprocess, 'run', side_effect=lambda command, **kw:
                           execute_fixture(entry, scenario, command, **kw)),
              patch.object(runner.subprocess, 'check_output', side_effect=lambda command, **kw:
                           execute_fixture(entry, scenario, command, **kw)),
              patch.object(sys, 'argv', argv)):
            runner.main()
        if entry == 'review':
            assert (root / 'kept/review.exe').read_bytes() == b'probe fixture, not an executable'
        if entry in ('comparison', 'benchmark'):
            assert json.loads((root / 'report.json').read_text())
        return directories, sleep.call_args_list


class ReviewRunnerCleanupTests(unittest.TestCase):
    def test_all_four_entrypoints_reuse_shared_helper(self):
        for name, runner in RUNNERS.items():
            with self.subTest(entry=name):
                self.assertIs(runner.temporary_work_directory, cleanup.temporary_work_directory)

    def test_transient_locks_retry_without_warning(self):
        for entry in RUNNERS:
            with self.subTest(entry=entry), redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()) as stderr:
                directories, sleeps = run_script_fixture(entry, transient=True)
                self.assertEqual(len(directories), 1)
                self.assertEqual(directories[0].cleanup.call_count, 2)
                self.assertEqual(sleeps, [call(0.1)])
                self.assertEqual(stderr.getvalue(), '')

    def test_persistent_locks_preserve_entrypoint_exit_codes_under_werror(self):
        common = {'success': (0, None), 'compile': (1, 'CalledProcessError'),
                  'probe': (1, 'CalledProcessError'), 'timeout': (1, 'TimeoutExpired'),
                  'body_permission': (1, 'probe launch denied'), 'exit': (7, None),
                  'interrupt': (None, 'KeyboardInterrupt')}
        extra = {'comparison': {'mismatch': (1, 'snapshot mismatch')},
                 'benchmark': {'mismatch': (1, 'learning score mismatch')},
                 'negative': {'control_passed': (1, 'broken variant was not rejected'),
                              'control_crashed': (1, 'negative control crashed')}}
        code = ('import sys; from test_sentence_review_cleanup import run_script_fixture; '
                'run_script_fixture(sys.argv[1], sys.argv[2])')
        for entry in RUNNERS:
            for scenario, (exit_code, error) in {**common, **extra.get(entry, {})}.items():
                with self.subTest(entry=entry, scenario=scenario):
                    result = subprocess.run([sys.executable, '-Werror', '-c', code, entry, scenario],
                                            cwd=HERE, capture_output=True, text=True, timeout=30)
                    if exit_code is None:
                        self.assertNotEqual(result.returncode, 0, result.stderr)
                    else:
                        self.assertEqual(result.returncode, exit_code, result.stderr)
                    reports = [json.loads(line) for line in result.stderr.splitlines()
                               if line.startswith('{"phase":')]
                    self.assertEqual(len(reports), 1, result.stderr)
                    report = reports[0]
                    self.assertEqual(report['phase'], 'cleanup')
                    self.assertEqual(report['status'], 'warning')
                    self.assertEqual(report['attempts'], 6)
                    self.assertTrue(report['test_result_unchanged'])
                    self.assertIn('review.exe', report['error'])
                    if error:
                        self.assertIn(error, result.stderr)
                    if exit_code == 0:
                        self.assertNotIn('Traceback', result.stderr)
                        if entry == 'negative':
                            self.assertIn('11 functional negative controls rejected', result.stdout)


@unittest.skipUnless(os.name == 'nt', 'Windows file sharing semantics required')
class WindowsReviewLockTests(unittest.TestCase):
    def test_real_review_exe_lock_at_entrypoint_cleanup(self):
        for transient in (True, False):
            with self.subTest(transient=transient):
                directories = []
                real_directory = tempfile.TemporaryDirectory
                release = None

                def directory(**kwargs):
                    item = real_directory(**kwargs)
                    directories.append(item)
                    return item

                with ExitStack() as handles:
                    def execute(command, **kwargs):
                        nonlocal release
                        if command[0] != 'cl':
                            release = handles.enter_context(locked_file(Path(command[0])))
                        return execute_fixture('review', 'success', command, **kwargs)

                    try:
                        with (patch.object(cleanup.tempfile, 'TemporaryDirectory', side_effect=directory),
                              patch.object(cleanup.time, 'sleep', side_effect=lambda _: release() if transient else None) as sleep,
                              patch.object(review.shutil, 'which', return_value='cl'),
                              patch.object(review.subprocess, 'run', side_effect=execute),
                              patch.object(sys, 'argv', ['runner', '--cxx', 'cl']),
                              redirect_stderr(io.StringIO()) as stderr):
                            review.main()
                        self.assertEqual(len(directories), 1)
                        work = Path(directories[0].name)
                        if transient:
                            self.assertEqual(sleep.call_args_list, [call(0.1)])
                            self.assertFalse(work.exists())
                            self.assertEqual(stderr.getvalue(), '')
                        else:
                            self.assertEqual(sleep.call_args_list, [call(d) for d in cleanup.CLEANUP_RETRY_DELAYS])
                            report = json.loads(stderr.getvalue())
                            self.assertEqual(report['directory'], str(work))
                            self.assertEqual(report['attempts'], 6)
                            self.assertTrue(report['test_result_unchanged'])
                            self.assertTrue((work / 'review.exe').exists())
                    finally:
                        handles.close()
                        for item in directories:
                            item.cleanup()


if __name__ == '__main__':
    unittest.main()
