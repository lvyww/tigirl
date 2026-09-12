"""Production CandidateUI direction/first-frame/lifetime tests on real Windows.

Run in an MSVC developer shell. No profile registration or physical input.
The existing harness substitutes only its clock, publication failures and owner.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

from candidate_ui_presentation_test import COMMON, LIBS
from work_directory import temporary_work_directory

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='cl')
    parser.add_argument('--negative-control', action='store_true')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Real Windows and MSVC required')
    compiler = shutil.which(args.cxx)
    if not compiler:
        parser.error('Run in an MSVC developer shell')
    with temporary_work_directory(prefix='tigirl-ui-placement-') as work:
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
        probe = ROOT / 'tests/candidate_ui_placement_probe.cpp'
        result = run(probe, 'placement')
        print(result.stdout, end='', flush=True)
        if result.returncode:
            raise RuntimeError(f'CandidateUI placement failed: {result.stderr}')
        if args.negative_control:
            source = (ROOT / 'native/tsf/CandidateUI.cpp').read_text(encoding='utf-8')
            gate = '    state_.reset();'
            if source.count(gate) != 1:
                raise RuntimeError('Detach changed; update negative control')
            (work / 'placement-candidate-ui.cpp').write_text(source.replace(
                gate, '    if(state_)state_->placement.reset();\n' + gate), encoding='utf-8')
            base = (ROOT / 'tests/candidate_ui_presentation_probe.cpp').read_text(encoding='utf-8')
            include = '#include "../native/tsf/CandidateUI.cpp"'
            if base.count(include) != 1:
                raise RuntimeError('Presentation include changed')
            (work / 'placement-presentation-base.cpp').write_text(base.replace(
                include, '#include "placement-candidate-ui.cpp"'), encoding='utf-8')
            control_source = work / 'placement-control.cpp'
            control_source.write_text(probe.read_text(encoding='utf-8').replace(
                '#include "candidate_ui_presentation_probe.cpp"',
                '#include "placement-presentation-base.cpp"'), encoding='utf-8')
            control = run(control_source, 'legacy')
            if control.returncode != 1 or 'Popup teardown cleared direction memory' not in control.stderr:
                raise RuntimeError(f'Per-composition reset not detected: {control}')
            print(json.dumps({'negative_control': 'passed', 'composition_reset_rejected': True}), flush=True)


if __name__ == '__main__':
    main()
