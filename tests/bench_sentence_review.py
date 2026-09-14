"""Paired synthetic Linux CPU measurements; not a performance acceptance gate."""
import argparse
import json
from pathlib import Path
import shutil
import statistics
import subprocess
import tempfile
from sentence_review_test import ROOT,SOURCES

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--report',type=Path,required=True);p.add_argument('--runs',type=int,default=5);p.add_argument('--cxx',default='g++');a=p.parse_args()
    cxx=shutil.which(a.cxx)
    if not cxx:p.error('compiler unavailable')
    if not 1<=a.runs<=20:p.error('--runs must be between 1 and 20')
    records=[]
    with tempfile.TemporaryDirectory(prefix='tigirl-benchmark-') as tmp:
        work=Path(tmp);binaries={}
        for name,repo in [('old',a.baseline.resolve()),('new',ROOT)]:
            exe=work/name;flags=['-std=c++17','-O2','-pthread','-I',str(repo/'native')]
            if name=='new':flags+=['-DTIGIRL_REVIEW_NEW']
            subprocess.run([cxx,*flags,str(ROOT/'tests/sentence_review_benchmark.cpp'),*(str(repo/s) for s in SOURCES[1:]),'-o',str(exe)],check=True,timeout=300)
            binaries[name]=exe
        for trial in range(a.runs):
            for name in (['old','new'] if trial%2==0 else ['new','old']):
                output=subprocess.check_output([str(binaries[name]),str(work/f'{name}-{trial}')],text=True,timeout=180)
                for line in output.splitlines():records.append(dict(json.loads(line),revision=name,trial=trial))
        summary={}
        for name in binaries:
            summary[name]={}
            for length in [80,128]:
                rows=[r for r in records if r['revision']==name and r.get('length')==length]
                summary[name][str(length)]={key:statistics.median(r[key] for r in rows) for key in ['repeat_ms','append_ms','backspace_ms','repeat_expanded','append_expanded','backspace_expanded']}
            rows=[r for r in records if r['revision']==name and r['test']=='learning_update']
            summary[name]['learning_update_ms']=statistics.median(r['ms_per_update'] for r in rows)
        sums={r['score_sum'] for r in records if r['test']=='learning_update'}
        if len(sums)!=1:raise RuntimeError('learning score mismatch in benchmark')
        report={'runs':a.runs,'within_run_repetitions':20,'synthetic':True,'production_model':False,'uses_ngram':False,'physical_tsf':False,'measurement':'Linux std::clock process CPU; includes synchronous decode/emit but excludes preparation; OS caches not flushed','summary':summary,'raw':records}
        a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
