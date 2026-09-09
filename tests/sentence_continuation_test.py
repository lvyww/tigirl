"""Upstream 954c82d regression, through the native Engine and real decoder."""
import hashlib
import json
import subprocess
from pathlib import Path
from windows_process import run_windows

ROOT = Path(__file__).resolve().parents[1]
checks = []
for platform in ['ARM64', 'x64', 'Win32']:
    for name in ['sentence_continuation_probe', 'sentence_session_probe']:
        probe = ROOT / 'build/tests' / platform / (name + '.exe')
        args = [probe]
        if name == 'sentence_continuation_probe':
            fixture = ROOT / 'build' / ('rumination-' + platform + '.tcd')
            args += [subprocess.check_output(['wslpath', '-w', str(fixture)], text=True).strip()]
        result = run_windows(args, capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (platform, name, result.stdout, result.stderr)
        data = json.loads(result.stdout)
        assert data['status'] == 'passed', data
        checks.append(dict(platform=platform, probe=name, result=data,
                           sha256=hashlib.sha256(probe.read_bytes()).hexdigest()))
report = dict(status='passed', upstream='954c82d5ed3823a9007375568740a67d063dfebc',
              checks=checks, foreground_tested=False)
(ROOT / 'build/sentence-continuation-validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
