"""Portable Tab learning regression, including 16 independent journal writers.
Run in MSVC Developer PowerShell with --cxx cl; GCC/Clang support --sanitize.
This exercises source/session/decoder/storage, NOT a live Windows TSF host.
"""
import argparse, json, os, shutil, subprocess, tempfile
from pathlib import Path
from run_core_tests import COMMON_SOURCES
ROOT=Path(__file__).resolve().parents[1]
EXTRA=['native/SentenceDecoder.cpp','native/SentenceLexicon.cpp','native/SentenceCharacterRanks.cpp',
       'native/SentenceNgram.cpp','native/AddWord.cpp']
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--cxx',default=os.environ.get('CXX','g++'));p.add_argument('--sanitize',action='store_true');p.add_argument('--keep',type=Path);args=p.parse_args()
    cxx=shutil.which(args.cxx)
    if not cxx:p.error('compiler unavailable')
    msvc=Path(cxx).name.lower() in ('cl','cl.exe')
    if args.sanitize and msvc:p.error('sanitizers require GCC/Clang')
    with tempfile.TemporaryDirectory(prefix='tab-learning-') as temp:
        work=Path(temp);exe=work/('probe.exe' if msvc else 'probe')
        flags=['/nologo','/std:c++17','/EHsc','/utf-8','/O2',f'/I{ROOT/"native"}'] if msvc else ['-std=c++17','-O2','-pthread','-I',str(ROOT/'native')]
        if args.sanitize:flags+=['-O0','-g0','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        sources=['tests/sentence_learning_probe.cpp']+list(dict.fromkeys(COMMON_SOURCES+EXTRA))
        if msvc:sources+=['native/LexiconOrder.cpp']
        output=[f'/Fe:{exe}'] if msvc else ['-o',str(exe)]
        subprocess.run([cxx,*flags,*(str(ROOT/s) for s in sources),*output],cwd=work,check=True,timeout=240)
        subprocess.run([str(exe),'test',str(work/'unit')],cwd=work,check=True,timeout=90)
        log=work/'.tigirl-learning-v1.log'
        # ASan per-process VA reservations are large; ordinary mode tests all 16
        # simultaneously. Sanitized mode still verifies independent processes.
        processes=[subprocess.Popen([str(exe),'worker',str(log),f'process-{i}','20'],cwd=work) for i in range(16)]
        assert all(p.wait(timeout=60)==0 for p in processes),'journal worker failed'
        count=int(subprocess.check_output([str(exe),'count',str(log)],text=True))
        assert count==320,count
        subprocess.run([str(exe),'worker',str(log),'process-0','20'],check=True)
        replay=int(subprocess.check_output([str(exe),'count',str(log)],text=True));assert replay==320,replay
        print(json.dumps({'test':'multi_process_journal','status':'passed','processes':16,'unique_events':count,'replay_events_added':replay-count,'sanitized':args.sanitize}),flush=True)
        if args.keep:
            args.keep.mkdir(parents=True,exist_ok=True);shutil.copy2(exe,args.keep/exe.name);shutil.copy2(log,args.keep/'cross-language-fixture.log')
if __name__=='__main__':main()
