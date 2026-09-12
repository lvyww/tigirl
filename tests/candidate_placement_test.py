"""Test the production candidate-placement header with synthetic screen geometry.

Windows uses the real SDK RECT/POINT. On Linux only these POD declarations are
supplied by a temporary windows.h; no window, DPI API or rendering is emulated.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

from work_directory import temporary_work_directory

ROOT = Path(__file__).resolve().parents[1]
# Previous flip-and-latch policy, adapted to the new stateless call signature.
# The same probe must reject this policy, not merely accept the new one.
LEGACY = r'''#pragma once
#include <algorithm>
#include <windows.h>
namespace tiger::tsf {
inline POINT placeCandidateWindow(const RECT& caret,const RECT& work,int width,int height) {
    static bool above=false;
    constexpr int gap=5;
    if(!above && caret.bottom+gap+height>work.bottom)
        above=caret.top-gap-height>=work.top || caret.top-work.top>work.bottom-caret.bottom;
    return {std::clamp(caret.left,work.left,(std::max)(work.left,work.right-width-2)),
        std::clamp(above?caret.top-gap-height:caret.bottom+gap,work.top,(std::max)(work.top,work.bottom-height-2))};
}
}
'''


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
            (legacy / 'CandidatePlacement.h').write_text(LEGACY, encoding='utf-8')
            exe = build(legacy, 'candidate_placement_legacy')
            result = subprocess.run([str(exe)], cwd=work, capture_output=True,
                                    text=True, timeout=30)
            expected = 'Overflow must slide to the work-area bottom, not flip above the caret'
            if result.returncode != 1 or expected not in result.stderr:
                raise RuntimeError(f'Legacy policy was not rejected for the expected reason: {result}')
            print(json.dumps({'probe': 'candidate_placement_negative_control',
                              'status': 'passed', 'legacy_policy_rejected': True}), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'g++'))
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    run_test(args.cxx, args.sanitize)
