"""Actual private TSF adjustment failure, visible-status contract and explicit retry.

Uses explicit TSF callbacks and UI-less candidates; no physical desktop claim.
"""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
PS = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
HOST = BUILD / 'tests/ARM64/tsf_host.exe'
DLL = BUILD / 'ARM64/Release/Tigirl.dll'
PROBE = BUILD / 'tests/ARM64/user_store_probe.exe'
MANIFEST = DLL.parent / 'NativeTiger.Test.manifest'

def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()

def registration():
    script = "(Get-Item 'Registry::HKEY_CLASSES_ROOT\\CLSID\\{D2291A80-84D8-4641-9AB2-BDD1472C846B}\\InprocServer32').GetValue('')"
    return subprocess.check_output([PS, '-NoProfile', '-Command', script], text=True).strip()

before = registration()
shutil.copyfile(ROOT / 'tests/NativeTiger.Test.manifest', MANIFEST)
with tempfile.TemporaryDirectory(prefix='word-save-failure-', dir=BUILD) as temporary:
    root = Path(temporary)
    (root / '.tsf-word-save-test').touch()
    (root / 'config.txt').write_text('最大码长\t4\n', encoding='utf-8-sig')
    result = subprocess.run([str(HOST), win(DLL), '-', win(MANIFEST), '--word-save-failure', win(root)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
    report = json.loads(result.stdout)
    restarted = subprocess.check_output([str(PROBE), win(ROOT / 'data/tiger-v2.tcd'),
                                        win(root / 'user/tiger-words.tcu'), 'dump', 'ab'], text=True)
    assert bytes.fromhex(restarted.splitlines()[0]).decode('utf-16-be') == '疒'
    report['independent_restart_keeps_retry'] = True
assert registration() == before, 'Fixture changed installed registration'
report['registration_unchanged'] = True
report['hashes'] = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                    for path in [HOST, DLL, PROBE]}
(BUILD / 'word-save-failure-arm64.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
