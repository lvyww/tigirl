"""Independent old-fresh vs new-incremental/locked candidate and score snapshots.

Confidence-pool/ancestor-truncation corrections are deliberately checked by
sentence_review_test.py instead of demanding parity with the buggy old evidence.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
from sentence_review_test import SOURCES, ROOT

def fixture(path):
    tokens = sorted([0,2,3,ord('甲'),ord('乙'),ord('国'),ord('中'),ord('丙'),ord('丁'),0x20000])
    pair = lambda a,b:(a<<21)|b
    sections = [([(a,.1 if a==0 else .2) for a in tokens],False),
                ([(pair(a,b),.0 if (a+b)%5==0 else .03) for a in tokens for b in tokens],True),
                ([(a,.7) for a in tokens],False),
                ([((a<<42)|pair(b,c),.0 if (a+b+c)%3==0 else .013) for a in tokens for b in tokens for c in tokens],True),
                ([(pair(a,b),.8) for a in tokens for b in tokens],True)]
    with path.open('wb') as f:
        f.write(b'TCSKNM01'+struct.pack('<I',1))
        for rows,wide in sections:
            f.write(struct.pack('<Q' if wide else '<I',len(rows)))
            for key,value in rows:f.write(struct.pack('<Qf' if wide else '<If',key,value))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--cxx',default='g++');p.add_argument('--report',type=Path,required=True);args=p.parse_args()
    cxx=shutil.which(args.cxx)
    if not cxx:p.error('compiler unavailable')
    baseline=args.baseline.resolve();msvc=Path(cxx).name.lower() in ('cl','cl.exe')
    sources=SOURCES[1:]+(['native/LexiconOrder.cpp'] if msvc else [])
    with tempfile.TemporaryDirectory(prefix='tigirl-revisions-') as tmp:
        work=Path(tmp);model=work/'synthetic.bin';fixture(model);outputs={};manifest={}
        for name,repo in [('old',baseline),('new',ROOT)]:
            folder=work/name;folder.mkdir();exe=folder/('probe.exe' if msvc else 'probe')
            flags=['/nologo','/std:c++17','/EHsc','/utf-8','/O2',f'/I{repo/"native"}'] if msvc else ['-std=c++17','-O2','-pthread','-I',str(repo/'native')]
            output=[f'/Fe:{exe}'] if msvc else ['-o',str(exe)]
            subprocess.run([cxx,*flags,str(ROOT/'tests/sentence_revision_probe.cpp'),*(str(repo/s) for s in sources),*output],cwd=folder,check=True,timeout=300)
            outputs[name]={}
            for variant,modelarg in [('none','-'),('mapped',str(model))]:
                result=subprocess.run([str(exe),str(folder/variant),'fresh' if name=='old' else 'incremental',modelarg],cwd=folder,capture_output=True,check=True,timeout=180)
                outputs[name][variant]=result.stdout
            manifest[name]={s:hashlib.sha256((repo/s).read_bytes()).hexdigest() for s in sources+['native/SentenceDecoder.h','native/SentenceLearning.h','native/SentenceLexicon.h']}
        counts={}
        for variant in outputs['old']:
            a=outputs['old'][variant].splitlines();b=outputs['new'][variant].splitlines()
            if a!=b:
                args.report.parent.mkdir(parents=True,exist_ok=True)
                args.report.with_suffix('.old.txt').write_bytes(outputs['old'][variant]);args.report.with_suffix('.new.txt').write_bytes(outputs['new'][variant])
                for i,(x,y) in enumerate(zip(a,b)):
                    if x!=y:raise RuntimeError(f'{variant} snapshot mismatch at {i}; raw records saved')
                raise RuntimeError('snapshot count mismatch')
            counts[variant]=len(a)
        report={'status':'passed','snapshots':counts,'total':sum(counts.values()),'fields':'candidate text/order, exact hexadecimal scores, rank, eligibility, segmented code, raw/text/learning boundaries, learning mode/flag','confidence_policy_compared_to_old':False,'sources':manifest,'synthetic_model_sha256':hashlib.sha256(model.read_bytes()).hexdigest(),'production_model':False,'physical_tsf':False}
        args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'status':'passed','snapshots':counts,'total':sum(counts.values())}))

if __name__=='__main__':main()
