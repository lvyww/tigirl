"""Run real TSF UI-less layout regressions on both bundled dictionaries (Windows).

Build the DLL/importer and CandidateLayoutHost.vcxproj for the same architecture
first. The test loads a private DLL copy through its class factory, with a real
Windows TSF manager/text store. It never registers a profile, injects global
input, uses the user's data, or needs a sentence language model.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from bundled_schemas_test import DATA, ROOT, sections, verify_sources


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['x64', 'Win32'], default='x64')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This integration test requires Windows; run it in Windows CI.')
    manifest = verify_sources()
    binaries = ROOT / 'build/tests' / args.platform
    importer = binaries / 'Tigirl.Import.exe'
    host = binaries / 'candidate_layout_host.exe'
    dll = ROOT / 'build' / args.platform / 'Release/Tigirl.dll'
    for path in (importer, host, dll):
        if not path.is_file():
            parser.error(f'Build the matching native binary first: {path}')
    for scheme in manifest['schemes']:
        with tempfile.TemporaryDirectory(prefix='tigirl-layout-') as temp:
            root = Path(temp)
            package = root / 'package'
            package.mkdir()
            output = package / 'tiger-v2.tcd'
            subprocess.run([str(importer), str(DATA / '码表' / scheme),
                            str(DATA / '拼音反查码表'), str(output), 'zh-CN'],
                           check=True, timeout=180, capture_output=True)
            ordinary = sections(output)
            # Freeze a deterministic, non-dynamic row with candidates past page 1.
            rows = [(code, words) for code, words in ordinary[1].items()
                    if re.fullmatch('[a-z]{2,4}', code) and len(words) >= 7
                    and all(word and '{' not in word and '\x1e' not in word for word in words)]
            if not rows:
                raise RuntimeError(f'No seven-candidate regression row in {scheme}')
            code, words = min(rows, key=lambda item: (len(item[0]), item[0]))
            staged_dll = package / 'Tigirl.dll'
            shutil.copy2(dll, staged_dll)
            user_root = root / 'isolated-user'
            user_root.mkdir()
            (user_root / '.candidate-layout-test').touch()
            (user_root / 'config.txt').write_text(
                '默认中文\t是\n每页候选个数\t5\n最大码长\t16\n'
                '最大码长无重自动上屏\t否\n自动启用整句模式\t否\n', encoding='utf-8-sig')
            env = os.environ.copy()
            env.pop('NATIVE_TIGER_TEST_TRACE', None)
            completed = subprocess.run([str(host), str(staged_dll), str(user_root), code, words[5]],
                                       env=env, capture_output=True, text=True, timeout=60)
            if completed.returncode:
                raise RuntimeError(f'{scheme} / {args.platform}: {completed.stdout}\n{completed.stderr}')
            result = json.loads(completed.stdout)
            if result.get('status') != 'passed':
                raise RuntimeError(result)
            print(json.dumps({'scheme': scheme, 'platform': args.platform, 'code': code,
                              'candidate_count': len(words), **result}), flush=True)
    verify_sources()


if __name__ == '__main__':
    main()
