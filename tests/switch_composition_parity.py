"""Original schema-refresh parity, same dictionary with target length/mixed changes."""
import hashlib,itertools,json,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
texts=['','a','ab','ab=','ab==','abcd','abcdef','abcdefghijklmnop','abcdefghijklmnopq','zzzzzz','`ni','`zhong','Hello','Dq1']
texts+=[''.join(pair) for pair in itertools.product('abcdefghijklmnopqrstuvwxyz',repeat=2)]
cases=[dict(text=text,beforeMixed=before,afterMixed=after,maximum=maximum) for text,before,after,maximum in itertools.product(texts,[False,True],[False,True],[1,2,4,16])]
trace=BUILD/'switch-composition.jsonl';trace.write_text(''.join(json.dumps(c)+'\n' for c in cases))
tsv=BUILD/'switch-composition.tsv';tsv.write_text(''.join(f"{int(c['beforeMixed'])} {int(c['afterMixed'])} {c['maximum']} {c['text']}\n" for c in cases))
oracle=BUILD/'switch-composition-oracle.jsonl'
with tempfile.TemporaryDirectory(prefix='switch-composition-oracle-',dir=BUILD) as tmp,oracle.open('w',encoding='utf-8') as out:
 shutil.copytree(ROOT/'data/staging',tmp,dirs_exist_ok=True)
 subprocess.run(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'switchcomposition',win(tmp),win(trace)],stdout=out,check=True)
expected=[json.loads(x) for x in oracle.read_text(encoding='utf-8-sig').splitlines()]
failed=False
for arch,exe in [('linux',BUILD/'switch_composition_probe'),('arm64',BUILD/'tests/ARM64/switch_composition_probe.exe')]:
 actual=[json.loads(x) for x in subprocess.check_output([str(exe)]+[win(p) if arch=='arm64' else str(p) for p in [ROOT/'data/tiger-v2.tcd',tsv]],text=True).splitlines()]
 assert len(actual)==len(expected)==len(cases)
 failures=[dict(case=cases[i],expected=a,actual=b) for i,(a,b) in enumerate(zip(expected,actual)) if a!=b]
 report=dict(cases=len(cases),mismatches=len(failures),failures=failures[:10],probe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),scope='Same lexicon; original schema composition refresh under changed maximum and mixed settings')
 (BUILD/f'switch-composition-parity-{arch}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
 print(arch,len(cases),'cases',len(failures),'mismatches',failures[:2],flush=True)
 failed|=bool(failures)
assert not failed
