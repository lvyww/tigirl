"""Functional fault injection: prove review regressions reject broken variants.

All variants use the shared scratch-directory helper; never modifies the checkout. Reuses
unmodified object files, rebuilding only the deliberately mutated translation unit.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
from work_directory import temporary_work_directory
from sentence_review_test import ROOT, SOURCES

def replace_once(text, old, new):
    if text.count(old) != 1: raise RuntimeError(f'mutation anchor not unique: {old[:100]}')
    return text.replace(old,new,1)

def whole(text):
    a=text.index('        const bool safeWhole=');b=text.index('\n        const int from=',a)
    return text[:a]+'        const bool safeWhole=true;'+text[b:]

def selectors(text):
    a=text.index('                 std::all_of(previous->raw.begin()+length');b=text.index(' {',a)
    return text[:a]+'                 true)'+text[b:]

def capacity(text):
    a=text.index('        if(values.capacity()>values.size()*4');b=text.index('        // Keep processed positions compact',a)
    return text[:a]+text[b:]

MUTATIONS = [
    ('confidence_top_k','correctness','native/SentenceDecoder.cpp',lambda s:replace_once(s,'for(const auto& c:all)if(required(c.text))','for(const auto& c:result.candidates)if(required(c.text))')),
    ('ancestor_truncation','correctness','native/SentenceDecoder.cpp',lambda s:replace_once(s,'states[consumed].truncated|=bucket.truncated;','/* no ancestor propagation */')),
    ('whole_input_boundary','correctness','native/SentenceDecoder.cpp',whole),
    ('selector_shrink','correctness','native/SentenceDecoder.cpp',selectors),
    ('learning_shrink','correctness','native/SentenceDecoder.cpp',lambda s:replace_once(s,'&& !previous->lattice->learningAffected)', '&& true)')),
    ('locked_cache','caching','native/SentenceDecoder.cpp',lambda s:replace_once(s,'if(cache_ && cache_->locked!=lockedPrefix)','if(cache_ && lockedPrefix)')),
    ('frozen_capacity','caching','native/SentenceDecoder.cpp',capacity),
    ('scored_upgrade','caching','native/SentenceDecoder.cpp',lambda s:replace_once(s,'auto cached=lattice.evaluated.find(length);','auto cached=lattice.evaluated.end();')),
    ('metadata_cache','caching','native/SentenceLexicon.cpp',lambda s:replace_once(s,'constexpr std::size_t budget=1024*1024;','constexpr std::size_t budget=0;')),
    ('history_release','history','native/SentenceDecoder.cpp',lambda s:replace_once(s,'if(floor>states.first && floor-states.first>=64)','if(false)')),
    ('cooperative_cancel','cancellation','native/SentenceDecoder.cpp',lambda s:replace_once(s,'if(cancellation_ && cancellation_->load(std::memory_order_relaxed))','if(false)')),
]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--cxx',default='g++');args=p.parse_args();cxx=shutil.which(args.cxx)
    if not cxx:p.error('compiler unavailable')
    if Path(cxx).name.lower() in ('cl','cl.exe'):p.error('negative-control runner uses GCC/Clang; normal regressions support MSVC')
    flags=['-std=c++17','-O2','-pthread','-I',str(ROOT/'native')]
    with temporary_work_directory(prefix='tigirl-faults-') as work:
        objects={}
        for i,source in enumerate(SOURCES):
            obj=work/f'{i}.o';subprocess.run([cxx,*flags,'-c',str(ROOT/source),'-o',str(obj)],check=True,timeout=180);objects[source]=obj
        for name,case,source,mutate in MUTATIONS:
            changed=work/(name+'.cpp');changed.write_text(mutate((ROOT/source).read_text()))
            obj=work/(name+'.o');exe=work/name
            subprocess.run([cxx,*flags,'-c',str(changed),'-o',str(obj)],check=True,timeout=180)
            subprocess.run([cxx,*flags,*(str(obj if s==source else path) for s,path in objects.items()),'-o',str(exe)],check=True,timeout=60)
            result=subprocess.run([str(exe),str(work/(name+'-data')),case],capture_output=True,text=True,timeout=60)
            if result.returncode==0:raise RuntimeError(f'broken variant was not rejected: {name}')
            if result.returncode<0:raise RuntimeError(f'negative control crashed rather than assertion rejection: {name}: {result.stderr}')
            print(f'{name}: REJECTED ({result.stderr.strip()})',flush=True)
    print(f'{len(MUTATIONS)} functional negative controls rejected')

if __name__=='__main__':main()
