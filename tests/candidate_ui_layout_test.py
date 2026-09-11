"""MSVC: exercise production CandidateUI layout/model separation and notifications.

Uses real Windows layered windows/rendering and an unactivated test owner.
No profile registration or user data. --negative-control restores the old
unconditional model reset in a temporary source copy, never in tracked sources.
"""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
COMMON = [
    'native/Engine.cpp', 'native/Dictionary.cpp', 'native/Lexicon.cpp',
    'native/DynamicText.cpp', 'native/UppercaseText.cpp', 'native/Text.cpp',
    'native/SentenceSession.cpp', 'native/SentenceAutoCommit.cpp',
    'native/LexiconSerialize.cpp', 'native/CandidatePresentation.cpp',
    'native/CandidateTheme.cpp', 'native/tsf/CandidateRenderer.cpp', 'native/tsf/PrivateFonts.cpp',
]
LIBS = ['ole32.lib', 'oleaut32.lib', 'uuid.lib', 'user32.lib', 'gdi32.lib',
        'shcore.lib', 'd2d1.lib', 'dwrite.lib', 'windowscodecs.lib']


@contextmanager
def temporary_work_directory(prefix='tigirl-ui-layout-'):
    directory = tempfile.TemporaryDirectory(prefix=prefix)
    try:
        yield Path(directory.name)
    finally:
        delays = (0.1, 0.2, 0.4, 0.8, 1.6)
        for attempt in range(len(delays) + 1):
            try:
                directory.cleanup()
                break
            except OSError as error:
                if attempt < len(delays):
                    time.sleep(delays[attempt])
                else:
                    print(json.dumps({'phase': 'cleanup', 'status': 'warning',
                                      'directory': directory.name, 'attempts': attempt + 1,
                                      'error': str(error), 'test_result_unchanged': True}),
                          file=sys.stderr, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='cl')
    parser.add_argument('--negative-control', action='store_true')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Real Windows windows and the MSVC environment are required')
    compiler = shutil.which(args.cxx)
    if not compiler:
        parser.error(f'Compiler not found: {args.cxx}')
    with temporary_work_directory() as work:
        flags = ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/O2', '/MT',
                 '/DUNICODE', '/D_UNICODE', f'/I{ROOT / "native"}',
                 f'/I{ROOT / "native/tsf"}', f'/I{ROOT / "tests"}']
        def compile_source(source, name):
            obj = work / (name + '.obj')
            subprocess.run([compiler, *flags, '/c', str(source), f'/Fo:{obj}'],
                           cwd=work, check=True, timeout=180)
            return obj
        common = [compile_source(ROOT / p, f'common-{i}') for i, p in enumerate(COMMON)]
        def run(source, name):
            obj = compile_source(source, name)
            exe = work / (name + '.exe')
            subprocess.run([compiler, '/nologo', str(obj), *(str(p) for p in common),
                            f'/Fe:{exe}', '/link', *LIBS], cwd=work, check=True, timeout=180)
            return subprocess.run([str(exe), str(work / (name + '.tcd'))], cwd=work,
                                  capture_output=True, text=True, timeout=90)
        probe_path = ROOT / 'tests/candidate_ui_layout_probe.cpp'
        result = run(probe_path, 'layout')
        print(result.stdout, end='', flush=True)
        if result.returncode:
            raise RuntimeError(f'Candidate layout regression failed: {result.stderr}')
        if args.negative_control:
            source = (ROOT / 'native/tsf/CandidateUI.cpp').read_text(encoding='utf-8')
            gate = 'if(update==CandidateUpdate::Content)updateContent();'
            if source.count(gate) != 1:
                raise RuntimeError('Update the negative-control model-reset mutation')
            (work / 'legacy-ui.cpp').write_text(source.replace(gate, 'updateContent();'), encoding='utf-8')
            probe = probe_path.read_text(encoding='utf-8')
            include = '#include "../native/tsf/CandidateUI.cpp"'
            if probe.count(include) != 1:
                raise RuntimeError('Update the negative-control source include')
            legacy = work / 'legacy-probe.cpp'
            legacy.write_text(probe.replace(include, '#include "legacy-ui.cpp"'), encoding='utf-8')
            result = run(legacy, 'legacy')
            if result.returncode == 0 or 'Layout reset host selection or paging' not in result.stderr:
                raise RuntimeError(f'Negative control did not reject the original reset: {result}')
            print('{"negative_control":"passed","unconditional_model_reset":"rejected"}', flush=True)


if __name__ == '__main__':
    main()
