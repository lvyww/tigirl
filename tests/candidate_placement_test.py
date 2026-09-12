"""Test production direction memory with synthetic physical screen geometry.

Windows uses SDK geometry types. Linux provides only POD declarations, not a
window/DPI API emulator. A negative control disables above-direction inheritance.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

from work_directory import temporary_work_directory

ROOT = Path(__file__).resolve().parents[1]
CONTROL_FAILURE = 'Cross-composition above preference was lost'


def run_test(cxx: str, sanitize: bool = False, negative_control: bool = True) -> None:
    compiler = shutil.which(cxx)
    if not compiler:
        raise RuntimeError(f'Compiler not found: {cxx}')
    msvc = Path(compiler).name.lower() in {'cl', 'cl.exe'}
    if msvc and sanitize:
        raise ValueError('--sanitize requires GCC/Clang ASan and UBSan')
    flags = ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/O2', '/W4'] if msvc else [
        '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror']
    if sanitize:
        flags += ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    with temporary_work_directory(prefix='tigirl-placement-') as work:
        includes = []
        if os.name != 'nt':
            (work / 'windows.h').write_text(
                '#pragma once\n#include <cstdint>\nusing LONG=std::int32_t;\n'
                'using UINT=unsigned; using HWND=void*; using HMONITOR=void*;\n'
                'struct RECT { LONG left,top,right,bottom; };\n'
                'struct POINT { LONG x,y; };\n', encoding='utf-8')
            includes = ['-I', str(work)]

        def build(header_dir: Path, name: str) -> Path:
            exe = work / (name + ('.exe' if os.name == 'nt' else ''))
            paths = [f'/I{header_dir}'] if msvc else ['-I', str(header_dir)]
            output = [f'/Fe:{exe}'] if msvc else ['-o', str(exe)]
            subprocess.run([compiler, *flags, *paths, *includes,
                            str(ROOT / 'tests/candidate_placement_probe.cpp'), *output],
                           cwd=work, check=True, timeout=120)
            return exe

        exe = build(ROOT / 'native/tsf', 'candidate_placement')
        subprocess.run([str(exe)], cwd=work, check=True, timeout=30)
        if negative_control:
            legacy = work / 'legacy'
            legacy.mkdir()
            production = (ROOT / 'native/tsf/CandidatePlacement.h').read_text(encoding='utf-8')
            gate = 'const bool keepAbove=same && above_ && delta>=-static_cast<Wide>(epsilon);'
            if production.count(gate) != 1:
                raise RuntimeError('Direction gate changed; update negative control')
            (legacy / 'CandidatePlacement.h').write_text(
                production.replace(gate, 'const bool keepAbove=false;'), encoding='utf-8')
            exe = build(legacy, 'candidate_placement_legacy')
            result = subprocess.run([str(exe)], cwd=work, capture_output=True,
                                    text=True, timeout=30)
            if result.returncode != 1 or CONTROL_FAILURE not in result.stderr:
                raise RuntimeError(f'Legacy policy was not rejected for the expected reason: {result}')
            print(json.dumps({'probe': 'candidate_placement_negative_control',
                              'status': 'passed', 'legacy_policy_rejected': True}), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'g++'))
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    run_test(args.cxx, args.sanitize)
