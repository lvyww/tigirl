"""ARM64X installer preflight, including a real ARM64-only DLL substitution."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
PACKAGE = BUILD / 'ARM64X/ARM64EC/Release'
PS = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()
def check(script, preflight=True):
    flags = ['-CheckOnly'] if preflight else ['-Elevated']
    return subprocess.run([PS, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', win(script), '-Arm64X', *flags],
                          capture_output=True, text=True, timeout=30)

result = check(ROOT / 'install_arm64.ps1')
assert result.returncode == 0, result.stderr
package = json.loads(result.stdout)
assert package['architecture'] == 'ARM64X'
assert set(package['load_checks']) == {'arm64', 'x64'}
assert all(x['loaded'] and x['class_instance'] for x in package['load_checks'].values())
with tempfile.TemporaryDirectory(prefix='package-arm64x-', dir=BUILD) as temporary:
    root = Path(temporary)
    staged = root / 'build/ARM64X/ARM64EC/Release'
    staged.mkdir(parents=True)
    for name, digest in package['artifacts'].items():
        relative = Path(name.replace('\\', '/'))
        source, target = PACKAGE / relative, staged / relative
        assert hashlib.sha256(source.read_bytes()).hexdigest() == digest.lower()
        target.parent.mkdir(parents=True, exist_ok=True)
        # These immutable data files are never modified by this fixture.
        if relative.suffix.lower() in ['.dll', '.exe']:
            shutil.copyfile(source, target)
        else:
            os.link(source, target)
    script = root / 'install_arm64.ps1'
    shutil.copyfile(ROOT / 'install_arm64.ps1', script)
    shutil.copyfile(ROOT / 'shortcut_arm64.ps1', root / 'shortcut_arm64.ps1')
    result = check(script)
    assert result.returncode == 0, result.stderr
    # AA64 in the PE header alone is insufficient: this is a valid ARM64 DLL.
    shutil.copyfile(BUILD / 'ARM64/Release/SampleIME.dll', staged / 'SampleIME.dll')
    result = check(script)
    assert result.returncode != 0 and 'x64 loader rejected DLL' in result.stderr and '193' in result.stderr, result.stderr
    shutil.copyfile(PACKAGE / 'SampleIME.dll', staged / 'SampleIME.dll')
    verifier = staged / 'verify_load_x64.exe'
    verifier.unlink()
    result = check(script)
    assert result.returncode != 0 and 'Missing build artifact' in result.stderr, result.stderr
    shutil.copyfile(PACKAGE / 'verify_load_x64.exe', verifier)
    bad = bytearray(verifier.read_bytes())
    struct.pack_into('<H', bad, struct.unpack_from('<I', bad, 0x3c)[0] + 4, 0xaa64)
    verifier.write_bytes(bad)
    result = check(script)
    assert result.returncode != 0 and 'not x64' in result.stderr, result.stderr
    # Invalid packages fail before the elevation boundary, without any UAC flow.
    result = check(script, preflight=False)
    assert result.returncode != 0 and 'not x64' in result.stderr, result.stderr
    assert not (root / 'install-arm64.log').exists()

report = dict(status='passed', package=package, real_dual_loader_checks=True,
              arm64_only_substitution_rejected=True, missing_verifier_rejected=True,
              wrong_verifier_architecture_rejected=True, invalid_package_rejected_before_elevation=True,
              installation_performed=False)
(BUILD / 'native-package-arm64x-preflight.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({k: v for k, v in report.items() if k != 'package'}))
