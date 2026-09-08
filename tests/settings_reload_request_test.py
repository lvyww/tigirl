"""Verify settings and their explicit reload request are one atomic snapshot."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'build/tests/ARM64/config_store_probe.exe'


def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def run(path, count, operation):
    return subprocess.check_output([str(EXE), win(path), str(count), operation], text=True).strip()


with tempfile.TemporaryDirectory(prefix='settings-reload-', dir=ROOT / 'build') as temporary:
    root = Path(temporary)
    path = root / 'config.txt'
    path.write_text('# retained\n主题 默认\n未知项目 保留\n_reload_fixture seed\n', encoding='utf-8')
    tokens = run(path, 3, 'save-settings').splitlines()
    assert len(tokens) == len(set(tokens)) == 3
    for token in tokens:
        uuid.UUID(token)
    text = path.read_text(encoding='utf-8-sig')
    assert '# retained' in text and '未知项目 保留' in text
    assert f'_native_settings_reload\t{tokens[-1]}' in text
    before = path.read_bytes()
    for operation, expected in [('save-noop', 'noop'), ('save-invalid', 'rejected'), ('deny-save', 'blocked')]:
        assert run(path, 1, operation) == expected
        assert path.read_bytes() == before, operation
        assert not list(root.glob('config.txt.tmp.*')), operation
    missing = root / 'missing.txt'
    assert run(missing, 1, 'save-noop') == 'noop' and not missing.exists()
    jobs = []
    try:
        for operation, count in [('reload-read', 500)] * 2 + [('save-paired', 13)] * 4:
            process = subprocess.Popen([str(EXE), win(path), str(count), operation],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            jobs.append((operation, process))
        snapshots = []
        writes = []
        for operation, process in jobs:
            output, error = process.communicate(timeout=30)
            assert process.returncode == 0, error
            rows = [line.split() for line in output.splitlines()]
            assert all(len(row) == 2 for row in rows)
            (snapshots if operation == 'reload-read' else writes).extend(rows)
    finally:
        for _, process in jobs:
            if process.poll() is None:
                process.kill()
                process.wait()
    assert len(writes) == 52 and len(snapshots) == 1000
    assert len({token for token, _ in writes}) == 52
    for token, _ in writes:
        uuid.UUID(token)
    committed = dict(writes)
    committed[tokens[-1]] = 'seed'
    assert all(committed.get(token) == stamp for token, stamp in snapshots), 'Reader saw mismatched request/settings'
    assert not list(root.glob('config.txt.tmp.*'))

report = dict(status='passed', sequential_unique_requests=3, concurrent_saves=52,
              concurrent_readers=2, paired_snapshots=1000,
              no_op_validation_and_denied_write_preserve_request=True,
              scope='configuration persistence; GUI and TSF verified separately',
              probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
(ROOT / 'build/settings-reload-request-arm64.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
