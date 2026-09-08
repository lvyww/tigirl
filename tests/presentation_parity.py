"""Compare candidate text/visibility with the unchanged original Overlay formatter."""
import json, subprocess, tempfile, shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; BUILD=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
cases=[]; rows=[]
contents=[('ab',['交','疒','𤕫'],['jiāo 基本','nè 基本','扩B']),('ab',[],[]),('',[],[]),('a\r\nb',['甲\t乙','x\ry\nz'],['注\r\n释','']),('abcdefghijkl',['词']*10,['']*10)]
for bits in range(16):
 for code,items,annotations in contents:
  cases.append(dict(CandidateVisible=True,InputCode=code,Candidates=items,CandidateAnnotations=annotations,VerticalCandidates=bool(bits&1),ShowCandidateIndex=bool(bits&2),ShowInputCodeInCandidateWindow=bool(bits&4),HideCandidateItems=bool(bits&8)))
  fields=[str(bits),code.encode().hex()]
  for item,annotation in zip(items,annotations): fields += [item.encode().hex(),annotation.encode().hex()]
  rows.append('\t'.join(fields))
trace=BUILD/'presentation-cases.jsonl';trace.write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in cases),encoding='utf-8')
tsv=BUILD/'presentation-cases.tsv';tsv.write_text('\n'.join(rows)+'\n')
with tempfile.TemporaryDirectory(prefix='presentation-oracle-',dir=BUILD) as temp:
 shutil.copytree(ROOT/'data/staging',temp,dirs_exist_ok=True)
 output=subprocess.check_output(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'presentation',win(temp),win(trace)],text=True,encoding='utf-8-sig')
expected=[json.loads(x) for x in output.splitlines()]
for exe in [BUILD/'presentation_probe',BUILD/'tests/ARM64/presentation_probe.exe']:
 if not exe.exists():continue
 actual=[bytes.fromhex(x).decode() for x in subprocess.check_output([str(exe),win(tsv) if exe.suffix=='.exe' else str(tsv)],text=True).splitlines()]
 assert len(actual)==len(expected)==len(cases)
 failures=[dict(index=i,case=cases[i],expected=a,actual=b) for i,(a,b) in enumerate(zip(expected,actual)) if a!=b]
 result=dict(cases=len(cases),mismatches=len(failures),failures=failures)
 suffix='arm64' if exe.suffix=='.exe' else 'linux';(BUILD/f'presentation-parity-{suffix}.json').write_text(json.dumps(result,ensure_ascii=False,indent=2))
 print(json.dumps(dict(platform=suffix,cases=len(cases),mismatches=len(failures),first=failures[:2]),ensure_ascii=False))
 assert not failures
