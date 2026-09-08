"""Check original-core settings-reset semantics in the native ARM64 engine."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
exe = root / 'build/tests/ARM64/configuration_reload_probe.exe'
dictionary = subprocess.check_output(['wslpath', '-w', str(root / 'data/tiger-v2.tcd')], text=True).strip()
report = json.loads(subprocess.check_output([str(exe), dictionary], text=True))
report['probe_sha256'] = hashlib.sha256(exe.read_bytes()).hexdigest()
(root / 'build/configuration-reload-arm64.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
