"""Headless TSF activation of staged DLLs with private COM manifests."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import shutil
import argparse
from tsf_architectures import ROOT, variants

PS = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
COMMAND = [PS] if Path('/proc/sys/fs/binfmt_misc/WSLInterop').exists() else ['/init', PS]
parser = argparse.ArgumentParser()
parser.add_argument('--builtin-override', action='store_true')
parser.add_argument('--selection-race', action='store_true')
parser.add_argument('--architecture', choices=['ARM64', 'x64'])
parser.add_argument('--arm64x-only', action='store_true')
args = parser.parse_args()


def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def quote(value):
    return "'" + value.replace("'", "''") + "'"


def registered():
    code = "(Get-Item 'Registry::HKEY_CLASSES_ROOT\\CLSID\\{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}\\InprocServer32').GetValue('')"
    return subprocess.check_output([*COMMAND, '-NoProfile', '-Command', code], text=True).strip()


before = registered()
output = ROOT / ('build/staged-tsf-selection-race.json' if args.selection_race else 'build/staged-tsf-builtin-override.json' if args.builtin_override else 'build/staged-tsf-activation.json')
report = {'status': 'running', 'checks': [], 'physical_input_tested': False,
          'real_applications_tested': False, 'installed': False}
if args.selection_race:
    report['fixture_sha256'] = hashlib.sha256((ROOT/'tests/selection_race_fixture.h').read_bytes()).hexdigest()
output.write_text(json.dumps(report, indent=2)+'\n')
try:
    for arm64x in ((True,) if args.arm64x_only else (False, True)):
        dll, hosts = variants(arm64x)
        for architecture, host, manifest in hosts:
            if args.architecture and architecture != args.architecture:
                continue
            with tempfile.TemporaryDirectory(prefix='staged-activation-', dir=ROOT/'build') as temporary:
                user = Path(temporary)
                if args.builtin_override:
                    (user/'.builtin-override-tsf-test').touch()
                    schema=user/'schemas/虎码字词'
                    generation=schema/'generations'/('a'*32);generation.mkdir(parents=True)
                    shutil.copyfile(dll.parent/'tiger-v2.tcd',generation/'tiger-v2.tcd')
                    (schema/'current.txt').write_text('generation\t'+'a'*32+'\n')
                (user/'config.txt').write_text('最大码长\t4\n', encoding='utf-8-sig')
                command = ('$env:NATIVE_TIGER_USER_ROOT=' + quote(win(user)) + '; & ' +
                           ' '.join(quote(value) for value in (win(host), win(dll), '-', win(manifest), '--selection-race' if args.selection_race else '--activation-only')) +
                           '; exit $LASTEXITCODE')
                run = subprocess.run([*COMMAND, '-NoProfile', '-Command', command], capture_output=True, text=True, timeout=45)
                check = {'architecture': architecture, 'dll': str(dll.relative_to(ROOT)),
                         'dll_sha256': hashlib.sha256(dll.read_bytes()).hexdigest(),
                         'host_sha256': hashlib.sha256(host.read_bytes()).hexdigest(),
                         'returncode': run.returncode, 'stdout': run.stdout, 'stderr': run.stderr}
                report['checks'].append(check)
                assert run.returncode == 0, check
                check['result'] = json.loads(run.stdout)
                assert check['result']['private_activation'] and check['result']['module_verified']
                assert check['result']['dictionary_verified']
    report['status'] = 'passed'
except Exception as error:
    report['status'] = 'failed'
    report['error'] = str(error)
    raise
finally:
    report['registration_unchanged'] = registered() == before
    if not report['registration_unchanged']:
        report['status'] = 'failed'
    output.write_text(json.dumps(report, indent=2)+'\n')
assert report['registration_unchanged']
print(json.dumps(report))
