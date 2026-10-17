"""Real Engine/decoder digit-binding regression with an isolated synthetic table."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
from candidate_selection_test import SOURCES as ENGINE_SOURCES
from work_directory import temporary_work_directory

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ['tests/sentence_digit_bindings_probe.cpp', *ENGINE_SOURCES[1:],
           'native/SelectionKeys.cpp', 'native/SentenceDecoder.cpp',
           'native/SentenceLexicon.cpp', 'native/SentenceCharacterRanks.cpp',
           'native/SentenceNgram.cpp', 'native/SentenceFivegram.cpp', 'native/AddWord.cpp']

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'g++'))
    args = parser.parse_args()
    compiler = shutil.which(args.cxx)
    if not compiler:
        parser.error('Compiler not found')
    msvc = Path(compiler).name.lower() in {'cl', 'cl.exe'}
    with temporary_work_directory(prefix='tigirl-digit-bindings-') as work:
        exe = work / ('probe.exe' if msvc else 'probe')
        flags = ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/O2'] if msvc else ['-std=c++17', '-O2', '-pthread']
        output = [f'/Fe:{exe}'] if msvc else ['-o', str(exe)]
        subprocess.run([compiler, *flags, *(str(ROOT / p) for p in SOURCES), *output],
                       cwd=work, check=True, timeout=300)
        subprocess.run([str(exe), str(work / 'fixture.tcd')], cwd=work, check=True, timeout=60)

if __name__ == '__main__':
    main()
