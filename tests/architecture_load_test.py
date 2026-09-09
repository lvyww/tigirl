"""Observe loading and COM construction from real ARM64 and emulated x64 processes.

The arm64-only expectation records a coverage limitation, not general acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import struct

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--dll', type=Path, default=ROOT / 'build/ARM64/Release/Tigirl.dll')
parser.add_argument('--expect', choices=['arm64-only', 'both'], default='arm64-only')
args = parser.parse_args()
dll = args.dll.resolve()
win_dll = subprocess.check_output(['wslpath', '-w', str(dll)], text=True).strip()
results = {}
for platform, machine in [('ARM64', 0xaa64), ('x64', 0x8664)]:
    probe = ROOT / 'build/tests' / platform / 'architecture_load_probe.exe'
    binary = probe.read_bytes()
    pe_machine = struct.unpack_from('<H', binary, struct.unpack_from('<I', binary, 0x3c)[0] + 4)[0]
    assert pe_machine == machine, (platform, pe_machine)
    observed = json.loads(subprocess.check_output([str(probe), win_dll], text=True, timeout=20))
    observed['probe_pe_machine'] = pe_machine
    assert observed['native_machine'] == 0xaa64, observed
    observed['probe_sha256'] = hashlib.sha256(probe.read_bytes()).hexdigest()
    results[platform] = observed
assert results['ARM64']['loaded'] and results['ARM64']['class_instance'], results
if args.expect == 'both':
    assert results['x64']['loaded'] and results['x64']['class_instance'], results
else:
    assert not results['x64']['loaded'] and results['x64']['load_error'] == 193, results
report = dict(expectation=args.expect, dll=str(dll), dll_sha256=hashlib.sha256(dll.read_bytes()).hexdigest(),
              results=results, tsf_activation_tested=False, real_application_input_tested=False,
              registration_performed=False)
(ROOT / 'build' / ('architecture-load-' + args.expect + '.json')).write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
