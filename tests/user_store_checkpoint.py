"""Validate checkpoint replay and subsequent edits in isolated disposable journals."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--windows', action='store_true', help='Run the actual ARM64 Windows probe')
args = parser.parse_args()
probe = ROOT / ('build/tests/ARM64/user_store_replay_probe.exe' if args.windows else 'build/user_store_replay_probe')
dictionary = ROOT / 'data/tiger-v2.tcd'


def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def quote(value):
    return "'" + value.replace("'", "''") + "'"


results = []
with tempfile.TemporaryDirectory(prefix='checkpoint-', dir=ROOT / 'build') as temporary:
    unrelated = Path(temporary)/'unrelated.tcu.compact-fixture.old'
    unrelated.write_bytes(b'Unrelated recovery artifact; preserve exactly.')
    for count in (0, 100000):
        journal = Path(temporary) / f'{count}.tcu'
        if args.windows:
            ps = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
            command = [ps] if Path('/proc/sys/fs/binfmt_misc/WSLInterop').exists() else ['/init', ps]
            command += ['-NoProfile', '-Command', '& ' + ' '.join(quote(win(path)) for path in
                         (probe, dictionary, journal)) + f' {count}; exit $LASTEXITCODE']
        else:
            command = [str(probe), str(dictionary), str(journal), str(count)]
        results.append(json.loads(subprocess.check_output(command, text=True, timeout=60)))
    assert unrelated.read_bytes() == b'Unrelated recovery artifact; preserve exactly.'
report = {'platform': 'Windows ARM64' if args.windows else 'Linux', 'results': results,
          'explicit_native_publication_tested': args.windows,
          'automatic_compaction_enabled': False, 'partial_failure_recovery_tested': False,
          'hashes': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                     for path in (probe, dictionary, ROOT / 'native/UserStore.cpp',
                                  ROOT / 'tests/user_store_replay_probe.cpp')}}
output = ROOT / 'build' / ('user-store-checkpoint-arm64.json' if args.windows else 'user-store-checkpoint-linux.json')
output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
