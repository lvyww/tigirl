"""Exercise native Windows directory invalidation in separate ARM64 processes."""
import hashlib
import json
from pathlib import Path
import selectors
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'build/tests/ARM64/directory_changes_probe.exe'


def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def receive(process, expected):
    with selectors.DefaultSelector() as selector:
        selector.register(process.stdout, selectors.EVENT_READ)
        assert selector.select(10), f'No response from watcher {process.pid}'
        actual = process.stdout.readline().strip()
    status = process.poll()
    assert actual == expected, (expected, actual, status,
                                process.stderr.read() if status is not None else '')


def command(process, text):
    process.stdin.write(text + '\n')
    process.stdin.flush()
    receive(process, text)


with tempfile.TemporaryDirectory(prefix='directory-changes-', dir=ROOT / 'build') as temporary:
    base = Path(temporary)
    roots = [base / '输入设置', base / '隔离设置']
    for root in roots:
        root.mkdir()
        (root / '.directory-changes-test').touch()
    readers = []
    try:
        for root in [roots[0], roots[0], roots[1]]:
            process = subprocess.Popen([str(EXE), win(root), 'watch'],
                                       stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, text=True, bufsize=1)
            readers.append(process)
            receive(process, 'ready')
        for process in readers:
            command(process, 'idle')
        for stage in range(10):
            subprocess.run([str(EXE), win(roots[0]), str(stage)], check=True,
                           capture_output=True, text=True, timeout=10)
            for process in readers[:2]:
                command(process, 'changed')
                command(process, 'quiet')
                # A later duplicate is legal even after a quiet interval; it
                # invalidates the snapshot again without implying a new write.
            command(readers[2], 'idle')
        for process in readers:
            process.stdin.write('close\n')
            process.stdin.flush()
        for process in readers:
            assert process.wait(timeout=10) == 0, process.stderr.read()
        relocation = json.loads(subprocess.check_output(
            [str(EXE), win(roots[0]), 'root-rename'], text=True, timeout=10))
        assert relocation['watched_rename_error'] == 0, relocation
        assert relocation['closed_rename_succeeded'], relocation
    finally:
        for process in readers:
            if process.poll() is None:
                process.stdin.close()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()

report = dict(status='passed', simultaneous_watchers=3, shared_root_watchers=2,
              mutation_stages=10, cross_process_notifications_verified=20,
              unrelated_root_unchanged=True, lifecycle_and_handle_checks=True,
              root_relocation=relocation,
              integrated_with_tsf=False,
              probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
(ROOT / 'build/directory-changes-arm64.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
