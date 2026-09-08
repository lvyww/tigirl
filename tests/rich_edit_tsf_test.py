"""OS-injected input in real Windows Rich Edit controls; no custom text store."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PS = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
COMMAND = [PS] if Path('/proc/sys/fs/binfmt_misc/WSLInterop').exists() else ['/init', PS]
parser = argparse.ArgumentParser()
parser.add_argument('--architecture', choices=['ARM64', 'x64', 'Win32'], default='x64')
args = parser.parse_args()


def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def quote(value):
    return "'" + value.replace("'", "''") + "'"


state = json.loads(subprocess.check_output([*COMMAND, '-NoProfile', '-ExecutionPolicy', 'Bypass',
                                          '-File', win(ROOT/'tests/desktop_state.ps1')], text=True))
if state['desktop'] != 'Default' or not state['foreground'] or state['idle_ms'] < 15000:
    raise SystemExit('Rich Edit test not started: require interactive desktop and 15 seconds input inactivity.')
installed = json.loads((ROOT/('build/native-install-x86.json' if args.architecture == 'Win32' else 'build/native-install.json')).read_text(encoding='utf-8-sig'))
key = "Registry::HKEY_CLASSES_ROOT\\CLSID\\{D2291A80-84D8-4641-9AB2-BDD1472C846B}\\InprocServer32"
def registered():
    code = '(Get-Item '+quote(key)+").GetValue('')"
    if args.architecture == 'Win32':
        code = "$b=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::ClassesRoot,[Microsoft.Win32.RegistryView]::Registry32); $b.OpenSubKey('CLSID\\{D2291A80-84D8-4641-9AB2-BDD1472C846B}\\InprocServer32').GetValue('')"
    return subprocess.check_output([*COMMAND, '-NoProfile', '-Command', code], text=True).strip()
assert registered().lower() == installed['dll'].lower()
installed_path = Path(subprocess.check_output(['wslpath', '-u', installed['dll']], text=True).strip())
actual_dll_hash = hashlib.sha256(installed_path.read_bytes()).hexdigest()
assert actual_dll_hash == installed['dll_hash'].lower(), 'Installed DLL differs from installation record'
host = ROOT/'build/tests'/args.architecture/'rich_edit_probe.exe'
binary = host.read_bytes()
assert struct.unpack_from('<H', binary, struct.unpack_from('<I', binary, 60)[0]+4)[0] == {'ARM64':0xaa64,'x64':0x8664,'Win32':0x14c}[args.architecture]
with tempfile.TemporaryDirectory(prefix='rich-edit-tsf-', dir=ROOT/'build') as temporary:
    root = Path(temporary)
    (root/'.rich-edit-test').touch()
    (root/'config.txt').write_text('最大码长\t4\n', encoding='utf-8-sig')
    command = ('$env:NATIVE_TIGER_USER_ROOT='+quote(win(root))+'; & '+quote(win(host))+' '+
               quote(installed['dll'])+'; exit $LASTEXITCODE')
    result = subprocess.run([*COMMAND, '-NoProfile', '-Command', command], capture_output=True, text=True, timeout=30)
report = dict(status='passed' if result.returncode == 0 else 'failed', architecture=args.architecture,
              dll=installed['dll'], recorded_dll_sha256=installed['dll_hash'],
              verified_dll_sha256=actual_dll_hash,
              dll_unchanged=hashlib.sha256(installed_path.read_bytes()).hexdigest() == actual_dll_hash,
              source_sha256=hashlib.sha256((ROOT/'tests/rich_edit_probe.cpp').read_bytes()).hexdigest(),
              host_sha256=hashlib.sha256(binary).hexdigest(), desktop_precondition=state,
              stdout=result.stdout, stderr=result.stderr, registration_unchanged=registered().lower() == installed['dll'].lower())
if result.returncode == 0:
    report['result'] = json.loads(result.stdout)
if not report['registration_unchanged'] or not report['dll_unchanged']:
    report['status'] = 'failed'
(ROOT/'build'/f'rich-edit-tsf-{args.architecture}.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report))
assert result.returncode == 0 and report['registration_unchanged'] and report['dll_unchanged'], result.stderr
