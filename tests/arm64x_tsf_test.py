"""Same experimental ARM64X DLL, actual ARM64/x64 TSF hosts and isolated journals."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DLL = ROOT / 'build/ARM64X/ARM64EC/Release/SampleIME.dll'
PS = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()
def registered():
    code = "(Get-Item 'Registry::HKEY_CLASSES_ROOT\\CLSID\\{D2291A80-84D8-4641-9AB2-BDD1472C846B}\\InprocServer32').GetValue('')"
    return subprocess.check_output([PS, '-NoProfile', '-Command', code], text=True).strip()

before = registered()
manifest_source = (ROOT / 'tests/NativeTiger.Test.manifest').read_text()
reports = {}
for platform, architecture, machine in [('ARM64', 'arm64', 0xaa64), ('x64', 'amd64', 0x8664)]:
    host = ROOT / 'build/tests' / platform / 'tsf_host.exe'
    binary = host.read_bytes()
    assert struct.unpack_from('<H', binary, struct.unpack_from('<I', binary, 0x3c)[0] + 4)[0] == machine
    manifest = DLL.parent / ('NativeTiger.' + architecture + '.manifest')
    manifest.write_text(manifest_source.replace('processorArchitecture="arm64"', 'processorArchitecture="' + architecture + '"'))
    with tempfile.TemporaryDirectory(prefix='arm64x-tsf-' + platform + '-', dir=ROOT / 'build') as temporary:
        root = Path(temporary)
        (root / '.tsf-word-save-test').touch()
        (root / 'config.txt').write_text('最大码长\t4\n', encoding='utf-8-sig')
        result = subprocess.run([str(host), win(DLL), '-', win(manifest), '--word-save-failure', win(root)],
                                capture_output=True, text=True, timeout=40)
        (ROOT / 'build' / ('arm64x-tsf-' + platform + '.stdout.txt')).write_text(result.stdout)
        (ROOT / 'build' / ('arm64x-tsf-' + platform + '.stderr.txt')).write_text(result.stderr)
        assert result.returncode == 0, (platform, result.returncode, result.stdout, result.stderr)
        report = json.loads(result.stdout)
        probe = ROOT / 'build/tests/ARM64/user_store_probe.exe'
        rows = subprocess.check_output([str(probe), win(DLL.parent / 'tiger-v2.tcd'),
                                        win(root / 'user/tiger-words.tcu'), 'dump', 'ab'], text=True)
        assert bytes.fromhex(rows.splitlines()[0]).decode('utf-16-be') == '疒', platform
        (root / '.tsf-management-test').touch()
        config_before = (root / 'config.txt').read_bytes()
        management = subprocess.run([str(host), win(DLL), '-', win(manifest), '--management-launch', win(root)],
                                    capture_output=True, text=True, timeout=40)
        assert management.returncode == 0, (platform, management.returncode, management.stdout, management.stderr)
        assert (root / 'config.txt').read_bytes() == config_before
        report.update(host_sha256=hashlib.sha256(binary).hexdigest(), persisted_order_verified=True,
                      interrupted_journal_tail_recovered=True, management_actions=json.loads(management.stdout))
        reports[platform] = report
assert registered() == before
report = dict(status='passed', dll_sha256=hashlib.sha256(DLL.read_bytes()).hexdigest(),
              architectures=reports, registration_unchanged=True, physical_input_tested=False,
              real_applications_tested=False, installed=False)
(ROOT / 'build/arm64x-tsf-validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
