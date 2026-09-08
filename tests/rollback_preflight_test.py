"""Validate real rollback snapshots without elevation or registration changes."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
PS = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'


def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def check(snapshot, preflight=True):
    return subprocess.run(
        [PS, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
         win(ROOT / 'rollback_arm64.ps1'), '-Snapshot', win(snapshot),
         '-CheckOnly' if preflight else '-Elevated'],
        capture_output=True, text=True, timeout=30)


snapshot = BUILD / 'previous-install-19466b1647bcaa2b.json'
original = json.loads(snapshot.read_text(encoding='utf-8-sig'))
valid = check(snapshot)
assert valid.returncode == 0, valid.stderr
cases = [dict(original, hash=None), dict(original, hash=''),
         dict(original, hash='invalid'), dict(original, hash='0' * 64)]
missing = dict(original)
del missing['hash']
cases.append(missing)
with tempfile.TemporaryDirectory(prefix='rollback-preflight-', dir=BUILD) as temporary:
    for index, record in enumerate(cases):
        path = Path(temporary) / f'snapshot [{index}].json'
        path.write_text(json.dumps(record), encoding='utf-8')
        expected = 'hash mismatch' if index == 3 else 'requires a SHA256'
        for preflight in (True, False):
            result = check(path, preflight)
            assert result.returncode != 0 and expected in result.stderr, result.stderr
    # A valid path containing wildcard characters must also be read literally.
    path.write_text(json.dumps(original), encoding='utf-8')
    result = check(path)
    assert result.returncode == 0, result.stderr

report = dict(status='passed', validated_dll=original['dll'],
              validated_hash=original['hash'], invalid_hash_cases=len(cases),
              invalid_snapshots_rejected_before_elevation=True,
              literal_snapshot_path=True, rollback_executed=False)
(BUILD / 'rollback-preflight-arm64.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
