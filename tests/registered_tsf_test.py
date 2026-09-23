"""Activate the installed TIP through system registration, without a private manifest."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PS = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
PS_COMMAND = [PS] if Path('/proc/sys/fs/binfmt_misc/WSLInterop').exists() else ['/init', PS]
parser = argparse.ArgumentParser()
parser.add_argument('--interactive', action='store_true')
parser.add_argument('--trace', action='store_true')
parser.add_argument('--queued', action='store_true')
parser.add_argument('--architecture', choices=['ARM64', 'x64'])
args = parser.parse_args()
record = json.loads((ROOT/'build/native-install.json').read_text(encoding='utf-8-sig'))


def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def quote(value):
    return "'" + value.replace("'", "''") + "'"


def registered():
    code = "(Get-Item 'Registry::HKEY_CLASSES_ROOT\\CLSID\\{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}\\InprocServer32').GetValue('')"
    return subprocess.check_output([*PS_COMMAND, '-NoProfile', '-Command', code], text=True).strip()


assert registered().lower() == record['dll'].lower()
dll = Path(subprocess.check_output(['wslpath', '-u', record['dll']], text=True).strip())
assert hashlib.sha256(dll.read_bytes()).hexdigest() == record['dll_hash'].lower()
desktop_precondition = None
if args.interactive:
    desktop_precondition = json.loads(subprocess.check_output(
        [*PS_COMMAND, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', win(ROOT/'tests/desktop_state.ps1')], text=True))
    (ROOT/'build/registered-tsf-desktop-precondition.json').write_text(json.dumps(desktop_precondition, indent=2)+'\n')
    if (desktop_precondition['desktop'] != 'Default' or not desktop_precondition['foreground'] or
            desktop_precondition['idle_ms'] < 15000):
        raise SystemExit('Interactive test not started: require Default desktop, foreground window and 15 seconds without keyboard/mouse input.')
results = {}
report_name = 'registered-tsf-interactive' if args.interactive else 'registered-tsf-validation'
if args.architecture:
    report_name += '-' + args.architecture
if args.queued:
    report_name += '-queued'
report_path = ROOT/'build'/(report_name + '.json')
report_path.write_text(json.dumps(dict(status='running', dll=record['dll'], architectures=results), indent=2)+'\n')
for architecture, machine in [('ARM64', 0xaa64), ('x64', 0x8664)]:
    if args.architecture and args.architecture != architecture:
        continue
    host = ROOT/'build/tests'/architecture/'tsf_host.exe'
    binary = host.read_bytes()
    assert struct.unpack_from('<H', binary, struct.unpack_from('<I', binary, 0x3c)[0]+4)[0] == machine
    for variant in (['ui-less', 'native-ui'] if args.interactive else ['activation']):
        with tempfile.TemporaryDirectory(prefix='registered-tsf-', dir=ROOT/'build') as temporary:
            user = Path(temporary)
            (user/'config.txt').write_text('最大码长\t4\n', encoding='utf-8-sig')
            capture = ROOT/'build'/f'registered-candidate-{architecture}.bmp'
            arguments = ' --registered-activation-only' if variant == 'activation' else (' ' + quote(win(capture)) if variant == 'native-ui' else '')
            command = ('$env:NATIVE_TIGER_USER_ROOT=' + quote(win(user)) + '; & ' + quote(win(host)) +
                       ' ' + quote(record['dll']) + arguments + '; exit $LASTEXITCODE')
            if args.queued:
                command = "$env:NATIVE_TIGER_TEST_QUEUED_KEYS='1'; " + command
            if args.trace:
                command = "$env:NATIVE_TIGER_TEST_TRACE='1'; " + command
            run = subprocess.run([*PS_COMMAND, '-NoProfile', '-Command', command], capture_output=True, text=True, timeout=45)
            (ROOT/'build'/f'registered-{architecture}-{variant}.stdout.txt').write_text(run.stdout)
            (ROOT/'build'/f'registered-{architecture}-{variant}.stderr.txt').write_text(run.stderr)
            if run.returncode:
                report_path.write_text(json.dumps(dict(status='failed', dll=record['dll'],
                    dll_sha256=record['dll_hash'], architectures=results, failed_architecture=architecture,
                    failed_variant=variant, error=run.stderr, host_sha256=hashlib.sha256(binary).hexdigest()), indent=2)+'\n')
            assert run.returncode == 0, (architecture, variant, run.stdout, run.stderr)
            result = json.loads(run.stdout)
            assert not result['private_activation'] and result['module_verified']
            if variant == 'activation':
                assert result['dictionary_verified']
            results[architecture + '-' + variant] = dict(result=result, host_sha256=hashlib.sha256(binary).hexdigest())
assert registered().lower() == record['dll'].lower()
report = dict(status='passed', dll=record['dll'], dll_sha256=record['dll_hash'],
              architectures=results, registration_unchanged=True, physical_input_tested=False,
              real_applications_tested=False, queued_messages=args.queued, desktop_precondition=desktop_precondition)
report_path.write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report))
