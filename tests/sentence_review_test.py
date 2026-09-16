"""Compile/run review regressions with immutable synthetic resources, no installed IME.

MSVC Developer PowerShell: --cxx cl. GCC/Clang: optionally --sanitize.
Checks exact results and work/memory counts, not timing thresholds.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
from work_directory import temporary_work_directory

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ['tests/sentence_review_probe.cpp', 'native/SentenceDecoder.cpp',
           'native/SentenceLexicon.cpp', 'native/SentenceNgram.cpp',
           'native/SentenceCharacterRanks.cpp', 'native/SentenceSession.cpp',
           'native/SentenceAutoCommit.cpp', 'native/Dictionary.cpp',
           'native/LexiconSerialize.cpp', 'native/AddWord.cpp', 'native/Text.cpp']

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--cxx', default=os.environ.get('CXX', 'g++'))
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--keep', type=Path)
    p.add_argument('--case', default='all', choices=['all','correctness','ranking','caching','fuzz','learning','journal','mapped','history','cancellation'])
    args = p.parse_args()
    cxx = shutil.which(args.cxx)
    if not cxx: p.error('compiler unavailable')
    msvc = Path(cxx).name.lower() in ('cl', 'cl.exe')
    if args.sanitize and msvc: p.error('use GCC/Clang for ASan/UBSan')
    with temporary_work_directory(prefix='tigirl-review-') as work:
        exe = work / ('review.exe' if msvc else 'review')
        flags = ['/nologo','/std:c++17','/EHsc','/utf-8','/O2',f'/I{ROOT/"native"}'] if msvc else ['-std=c++17','-O2','-pthread','-I',str(ROOT/'native')]
        if args.sanitize: flags += ['-O1','-g0','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        sources = SOURCES + (['native/LexiconOrder.cpp'] if msvc else [])
        output = [f'/Fe:{exe}'] if msvc else ['-o',str(exe)]
        subprocess.run([cxx,*flags,*(str(ROOT/s) for s in sources),*output],cwd=work,check=True,timeout=300)
        if args.keep:
            args.keep.mkdir(parents=True,exist_ok=True);shutil.copy2(exe,args.keep/exe.name)
        env = dict(os.environ)
        if args.sanitize:
            env['ASAN_OPTIONS'] = env.get('ASAN_OPTIONS','') + ':detect_leaks=1:halt_on_error=1'
            env['UBSAN_OPTIONS'] = env.get('UBSAN_OPTIONS','') + ':halt_on_error=1:print_stacktrace=1'
        subprocess.run([str(exe),str(work/'resources'),args.case,str(ROOT/'resources/sentence-lexical-v1.bin')],cwd=work,env=env,check=True,timeout=180)

if __name__ == '__main__': main()
