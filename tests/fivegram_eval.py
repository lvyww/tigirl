"""Frozen shape-code evaluation: legacy isolation control vs unified fivegram.

Models/fixtures are explicit local inputs and are never modified. No TSF install.
"""
import argparse, concurrent.futures, csv, hashlib, json, subprocess, time
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('probe','model','prior','fixture','cases','reference','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--jobs',type=int,default=4)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    cases=a.cases.read_text(encoding='utf-8-sig').splitlines(keepends=True)
    if cases and cases[0].startswith('id\t'):cases=cases[1:]
    def worker(i):
        root=a.output/str(i);root.mkdir(exist_ok=True)
        shard=root/'cases.tsv';shard.write_text(''.join(cases[i::a.jobs]),encoding='utf-8')
        output=root/'output.tsv'
        with (root/'run.log').open('w') as log:
            subprocess.run([str(a.probe.resolve()),'eval',str(a.model.resolve()),str(a.prior.resolve()),str(a.fixture.resolve()),str(shard.resolve()),str(output.resolve()),str(root.resolve())],stdout=log,stderr=subprocess.STDOUT,check=True)
        print('completed shard',i,flush=True)
        return list(csv.DictReader(output.open(encoding='utf-8'),delimiter='\t'))
    started=time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:rows=[r for part in pool.map(worker,range(a.jobs)) for r in part]
    assert len(rows)==len(cases) and len({r['id'] for r in rows})==len(cases)
    ref={r['id']:r for r in csv.DictReader(a.reference.open(encoding='utf-8-sig'),delimiter='\t')}
    differences=[r for r in rows if r['control']!=ref[r['id']]['fivegram']]
    changes=[r for r in rows if r['unified']!=r['control']]
    def stats(group):
        return {'rows':len(group),'control_correct':sum(r['control']==r['target'] for r in group),'unified_correct':sum(r['unified']==r['target'] for r in group),
                'rescued':sum(r['control']!=r['target']==r['unified'] for r in group),'regressed':sum(r['control']==r['target']!=r['unified'] for r in group),
                **{f'{kind}_top{limit}':sum(0<int(r[kind+'_rank'])<=limit for r in group) for kind in ('control','unified') for limit in (5,20)}}
    report={'seconds':time.monotonic()-started,'combined':stats(rows),'groups':{s:stats([r for r in rows if r['source']==s]) for s in sorted({r['source'] for r in rows})},'control_reference_differences':len(differences),'changed_top1':len(changes),'physical_input_tested':False,
            'files':{k:{'bytes':v.stat().st_size,'sha256':hashlib.file_digest(v.open('rb'),'sha256').hexdigest()} for k,v in vars(a).items() if isinstance(v,Path) and v.is_file()}}
    for name,items in [('all',rows),('changes',changes),('control-differences',differences)]:
        with (a.output/(name+'.tsv')).open('w',encoding='utf-8',newline='') as out:
            writer=csv.DictWriter(out,fieldnames=rows[0].keys(),delimiter='\t');writer.writeheader();writer.writerows(items)
    (a.output/'summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False),flush=True)
    if differences:raise SystemExit('Control differs from frozen TigerClaw; inspect control-differences.tsv')
if __name__=='__main__':main()
