"""Coded-row merge compared with the original complete snapshot builder.
All inputs carry codes; uncoded inference is tested separately when implemented.
"""
import hashlib,json,random,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';EXE=BUILD/'tests/ARM64/lexicon_rows_probe.exe'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
cases=[dict(text='乙\tAB\t2\n甲\tab\t9\n丙\tAb\t2\n甲\tab\t1\n'),dict(text='显示甲=>输出\tab\t3\n显示乙=>输出\tAB\t2\n显示甲=>输出\tab\t1\n'),dict(text='甲\ti\t1\n乙\tI\t2\n丙\tİ\t3\n丁\tı\t4\n')]
for group in [['σ','ς','Σ'],['s','ſ','S'],['µ','μ','Μ'],['ß','ẞ'],['𐐀','𐐨'],['k','K','K']]:
 cases.append(dict(text='\n'.join(f'词{i}\t{code}\t{len(group)-i}' for i,code in enumerate(group))))
rng=random.Random(20260909)
for _ in range(96):
 rows=[]
 for i in range(rng.randrange(20,100)):
  code=rng.choice(['a','AB','ab','abc','AbC','x','σ','ς','Σ','I','İ','ı','𐐀','𐐨'])
  word=rng.choice(['甲','乙','丙','显示=>甲','显示=>乙','甲=>甲','英文A','英文a','\\s甲','词\\n条'])
  freq=rng.choice([-2147483648,-2,0,0,1,7,2147483647])
  rows.append(f'{word}\t{code}\t{freq}')
 cases.append(dict(text='\n'.join(rows)))
# Large duplicate groups exercise exact identity without quadratic list scans.
cases.append(dict(text='\n'.join(f'词{i%2000}\t'+('Ab' if i%2 else 'aB')+f'\t{i%11}' for i in range(20000))))
trace=BUILD/'lexicon-merge.jsonl';trace.write_text(''.join(json.dumps(c)+'\n' for c in cases))
hexfile=BUILD/'lexicon-merge.hex';hexfile.write_text(''.join('0 '+''.join(f'{u[0]:04x}' for u in struct.iter_unpack('<H',c['text'].encode('utf-16-le')))+'\n' for c in cases))
oracle=BUILD/'lexicon-merge-oracle.jsonl'
with tempfile.TemporaryDirectory(prefix='lexicon-merge-oracle-',dir=BUILD) as tmp,oracle.open('w',encoding='utf-8') as out:
 (Path(tmp)/'.native-tiger-staging').touch()
 subprocess.run(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'mergerows',win(tmp),win(trace)],stdout=out,check=True)
expected=[json.loads(x) for x in oracle.read_text(encoding='utf-8-sig').splitlines()]
actual=[json.loads(x) for x in subprocess.check_output([str(EXE),win(hexfile),'--merge'],text=True).splitlines()]
assert len(actual)==len(expected)==len(cases)
failures=[dict(index=i,text=cases[i]['text'][:300],expected=a,actual=b) for i,(a,b) in enumerate(zip(expected,actual)) if a!=b]
report=dict(cases=len(cases),input_rows=sum(len(c['text'].splitlines()) for c in cases),mismatches=len(failures),failures=failures[:4],probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
(BUILD/'lexicon-merge-parity-arm64.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False));assert not failures
