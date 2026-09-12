"""Cross-composition direction memory: production geometry plus Windows UI.

Linux: --cxx clang++ --sanitize. MSVC developer shell: --cxx cl --ui.
Uses the shared bounded cleanup; real test/control failures still propagate.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
from work_directory import temporary_work_directory

ROOT = Path(__file__).resolve().parents[1]


def run_test(cxx: str, sanitize: bool = False, ui: bool = False) -> None:
    compiler = shutil.which(cxx)
    if not compiler:
        raise RuntimeError(f'Compiler not found: {cxx}')
    msvc = Path(compiler).name.lower() in {'cl', 'cl.exe'}
    if sanitize and msvc:
        raise ValueError('--sanitize requires GCC/Clang')
    if ui and (os.name != 'nt' or not msvc):
        raise ValueError('--ui requires Windows with the MSVC developer environment')
    flags = ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/O2', '/W4'] if msvc else [
        '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror']
    if sanitize:
        flags += ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    with temporary_work_directory(prefix='tigirl-orientation-') as work:
        sdk = []
        if os.name != 'nt':
            (work / 'windows.h').write_text(
                '#pragma once\n#include <cstdint>\nusing LONG=std::int32_t;\n'
                'struct RECT { LONG left,top,right,bottom; };\n'
                'struct POINT { LONG x,y; };\n', encoding='utf-8')
            sdk = ['-I', str(work)]

        def build(include: Path, name: str) -> Path:
            exe = work / (name + ('.exe' if os.name == 'nt' else ''))
            dirs = [f'/I{include}', f'/I{ROOT / "native/tsf"}'] if msvc else [
                '-I', str(include), '-I', str(ROOT / 'native/tsf')]
            output = [f'/Fe:{exe}'] if msvc else ['-o', str(exe)]
            subprocess.run([compiler, *flags, *dirs, *sdk,
                            str(ROOT / 'tests/candidate_orientation_probe.cpp'), *output],
                           cwd=work, check=True, timeout=120)
            return exe

        exe = build(ROOT / 'native/tsf', 'orientation')
        subprocess.run([str(exe)], cwd=work, check=True, timeout=30)
        # No compile errors/crashes accepted as a negative-control pass.
        text = (ROOT / 'native/tsf/CandidateOrientation.h').read_text(encoding='utf-8')
        gate = 'if(!(above_ && fitsAbove))'
        if text.count(gate) != 1:
            raise RuntimeError('Update the orientation negative-control mutation')
        legacy = work / 'legacy'
        legacy.mkdir()
        (legacy / 'CandidateOrientation.h').write_text(text.replace(gate, 'if(true)'), encoding='utf-8')
        exe = build(legacy, 'orientation_legacy')
        control = subprocess.run([str(exe)], cwd=work, capture_output=True, text=True, timeout=30)
        expected = 'Above preference lost across short compositions'
        if control.returncode != 1 or expected not in control.stderr:
            raise RuntimeError(f'Orientation negative control failed: {control}')
        print(json.dumps({'probe': 'orientation_negative_control', 'status': 'passed',
                          'stateless_policy_rejected': True}), flush=True)
        if ui:
            # Reuse the production renderer/Engine link list, not a parallel model.
            from candidate_ui_presentation_test import COMMON, LIBS
            ui_flags = [*flags, '/MT', '/DUNICODE', '/D_UNICODE',
                        f'/I{ROOT / "native"}', f'/I{ROOT / "native/tsf"}', f'/I{ROOT / "tests"}']
            objects = []
            for index, source in enumerate([*COMMON, 'tests/candidate_orientation_ui_probe.cpp']):
                obj = work / f'ui-{index}.obj'
                subprocess.run([compiler, *ui_flags, '/c', str(ROOT / source), f'/Fo:{obj}'],
                               cwd=work, check=True, timeout=180)
                objects.append(str(obj))
            exe = work / 'orientation_ui.exe'
            subprocess.run([compiler, '/nologo', *objects, f'/Fe:{exe}', '/link', *LIBS],
                           cwd=work, check=True, timeout=180)
            subprocess.run([str(exe), str(work / 'fixture.tcd')], cwd=work, check=True, timeout=90)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'g++'))
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--ui', action='store_true')
    args = parser.parse_args()
    run_test(args.cxx, args.sanitize, args.ui)
