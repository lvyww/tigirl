"""Verify default model lookup with the x86 installer's hard-linked package layout."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
primary = BUILD / 'ARM64X/ARM64EC/Release'
preflight = json.loads((BUILD / 'native-package-arm64x-preflight.json').read_text())
assert preflight['status'] == 'passed'
artifacts = preflight['package']['artifacts']
with tempfile.TemporaryDirectory(prefix='sentence-x86-package-', dir=BUILD) as tmp:
    stage = Path(tmp)
    for name, expected in artifacts.items():
        relative = Path(name.replace('\\', '/'))
        source = primary / relative
        assert hashlib.sha256(source.read_bytes()).hexdigest() == expected.lower(), name
        if name == 'Tigirl.dll':
            continue
        target = stage / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        os.link(source, target)
        assert os.path.samefile(source, target), name
    shutil.copyfile(BUILD / 'Win32/Release/Tigirl.dll', stage / 'Tigirl.dll')
    env = {k: v for k, v in os.environ.items() if not k.startswith('SENTENCE_')}
    env.update(SENTENCE_X86='1', SENTENCE_BUNDLED_MODEL='1', SENTENCE_JOURNAL='1',
               SENTENCE_X86_PACKAGE_DIR=str(stage))
    subprocess.run([sys.executable, str(ROOT / 'tests/sentence_tsf_test.py')],
                   env=env, cwd=ROOT, check=True)
    result = json.loads((BUILD / 'sentence-tsf-x86-journal-bundled-validation.json').read_text())
    assert len(result['checks']) == 1 and result['checks'][0]['returncode'] == 0
    report = dict(status='passed', primary_generation=preflight['package']['generation'],
                  shared_artifacts=artifacts, hard_link_identity_verified=True,
                  default_model_lookup=True, journal_and_schema_reload=True,
                  installed=False, checks=result['checks'])
    (BUILD / 'sentence-x86-package-validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': 'passed', 'hard_link_identity_verified': True, 'installed': False}))
