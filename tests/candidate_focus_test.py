"""Synthetic focus/Win-shortcut regression, including behavioral negative controls."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
from work_directory import temporary_work_directory
from candidate_selection_test import ROOT, SOURCES

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'g++'))
    parser.add_argument('--negative-control', action='store_true')
    args = parser.parse_args()
    compiler = shutil.which(args.cxx)
    if not compiler:
        parser.error(f'Compiler not found: {args.cxx}')
    msvc = Path(compiler).name.lower() in ('cl', 'cl.exe')
    flags = (['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/O2', f'/I{ROOT / "native"}'] if msvc
             else ['-std=c++17', '-O2', '-I', str(ROOT / 'native')])
    with temporary_work_directory(prefix='tigirl-focus-') as work:
        def run(name, engine):
            exe = work / (name + ('.exe' if os.name == 'nt' else ''))
            sources = [ROOT / 'tests/candidate_focus_probe.cpp', engine, *(ROOT / p for p in SOURCES[2:])]
            output = [f'/Fe:{exe}'] if msvc else ['-o', str(exe)]
            subprocess.run([compiler, *flags, *map(str, sources), *output], cwd=work, check=True, timeout=180)
            return subprocess.run([str(exe), str(work / (name + '.tcd'))], cwd=work, capture_output=True, text=True, timeout=60)
        production = ROOT / 'native/Engine.cpp'
        result = run('focus', production)
        print(result.stdout, end='', flush=True)
        if result.returncode:
            raise RuntimeError(result.stderr)
        if args.negative_control:
            source = production.read_text(encoding='utf-8')
            mutations = [
                ('cancel', 'if (key.win) return {};',
                 'if (key.win) {const bool clear=key.vk!=LWin && key.vk!=RWin && composing();if(clear)cancel();return {false,clear,{}};}',
                 'Win shortcut cancelled composition'),
                ('focus', 'void Engine::focusChanged() {',
                 'void Engine::focusChanged() { sentence_.invalidatePending();pageRaw_.clear();page_=0;',
                 'Focus reset ordinary candidate page'),
                ('shift', 'shiftChord_ = key.ctrl || key.alt || key.win;',
                 'shiftChord_ = false;', 'Shell-consumed chord became a Shift toggle')]
            for name, old, new, error in mutations:
                if source.count(old) != 1:
                    raise RuntimeError('Update negative-control mutation: ' + name)
                legacy = work / (name + '.cpp')
                legacy.write_text(source.replace(old, new), encoding='utf-8')
                result = run("negative-" + name, legacy)
                if result.returncode == 0 or error not in result.stderr:
                    raise RuntimeError(f'Negative control did not reject {name}: {result}')
                print('negative control rejected: ' + name, flush=True)

if __name__ == '__main__':
    main()
