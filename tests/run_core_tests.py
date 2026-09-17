"""Compile/run synthetic regressions once per toolchain, without installed IME data.

Windows: run inside a matching MSVC Developer PowerShell with --cxx cl.
Linux: optionally enable --sanitize for ASan/UBSan. All artifacts are temporary.
"""
import argparse
import json
import os
import sys
from pathlib import Path
import shutil
import subprocess
import time

from candidate_selection_test import SOURCES as SELECTION_SOURCES
from candidate_placement_test import run_test as run_placement_test
from work_directory import temporary_work_directory

ROOT = Path(__file__).resolve().parents[1]
COMMON_SOURCES = SELECTION_SOURCES[1:] + [
    'native/CandidatePresentation.cpp', 'native/Settings.cpp', 'native/SelectionKeys.cpp',
    'native/UserStore.cpp', 'native/SentenceNgram.cpp',
]
PROBES = {
    'candidate_selection': ('tests/candidate_selection_probe.cpp', False, ['fixture.tcd']),
    'sentence_session': ('tests/sentence_session_probe.cpp', True, []),
    'candidate_reveal': ('tests/candidate_reveal_probe.cpp', False, []),
    'code_mask': ('tests/code_mask_probe.cpp', False, []),
    'user_store_refresh_cache': ('tests/user_store_refresh_cache_probe.cpp', False, ['fixture.tcd']),
    'sentence_ngram_validation': ('tests/sentence_ngram_validation_probe.cpp', False, []),
}


def main() -> None:
    subprocess.run([sys.executable, str(ROOT / 'tests/review_hardening_policy_test.py')], check=True)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'g++'))
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    compiler = shutil.which(args.cxx)
    if not compiler:
        parser.error(f'Compiler not found: {args.cxx}')
    msvc = Path(compiler).name.lower() in {'cl', 'cl.exe'}
    if msvc and args.sanitize:
        parser.error('--sanitize requires GCC/Clang ASan and UBSan')
    run_placement_test(compiler, args.sanitize)
    if msvc:
        flags = ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/O2', f'/I{ROOT / "native"}']
    else:
        flags = ['-std=c++17', '-O2', '-I', str(ROOT / 'native')]
        if args.sanitize:
            flags += ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    with temporary_work_directory(prefix='tigirl-core-') as work:
        def run(command: list[str]) -> None:
            subprocess.run(command, cwd=work, check=True, timeout=300)
        run([compiler, *flags, '/c' if msvc else '-c',
             *(str(ROOT / p) for p in COMMON_SOURCES)])
        suffix = '.obj' if msvc else '.o'
        objects = [str(work / (Path(p).stem + suffix)) for p in COMMON_SOURCES]
        for name, (source, wide_main, arguments) in PROBES.items():
            exe = work / (name + ('.exe' if os.name == 'nt' else ''))
            entry = ['-Dwmain=main'] if wide_main and not msvc else []
            output = [f'/Fe:{exe}'] if msvc else ['-o', str(exe)]
            run([compiler, *flags, *entry, str(ROOT / source), *objects, *output])
            started = time.monotonic()
            run([str(exe), *(str(work / p) for p in arguments)])
            print(json.dumps({'probe': name, 'status': 'passed',
                              'seconds': round(time.monotonic() - started, 3),
                              'physical_input_tested': False}), flush=True)


if __name__ == '__main__':
    main()
