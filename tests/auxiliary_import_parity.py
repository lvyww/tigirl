"""Decoded auxiliary-file content compared with original loader methods."""
import hashlib,json,random,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';EXE=BUILD/'tests/ARM64/auxiliary_import_probe.exe'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
cases=[dict(pinyin='',comments='',splits='')]
lines=['甲 注释','甲 第二条','甲 第二条','甲=>乙 值=>别名','\\s \\t','# comment','甲 #literal','甲 \\#literal','甲 一 二','A First','a Second','甲 Bime20231222BIME','甲 \\\\n','甲 \\n','甲','甲\t\t拆分','\u3000甲 值\u3000','甲\u00a0乙 value']
for line in lines:cases.append(dict(pinyin=line,comments=line,splits=line))
cases.append(dict(pinyin='ni 你 妮 10\n你\tNI\t9\n你\n显示=>你\tni\t3\n',comments='\n'.join(lines),splits='\n'.join(lines)))
rng=random.Random(20260911)
for _ in range(160):
 annotation=[];pinyin=[]
 for _ in range(40):
  key=rng.choice(['甲','乙','a','A','σ','ς','词=>词','\\s','Bime20231222BIME','😀'])
  value=rng.choice(['注释','拆分','另一个','same','=>','甲=>乙','\\s','\\n','\\t','\\\\n','#字面'])
  annotation.append(key+rng.choice([' ','\t','  ','\t\t'])+value+rng.choice(['',' 多余','\t忽略']))
  pinyin.append(rng.choice(['你','妮','他','显示=>你','A','a'])+'\t'+rng.choice(['ni','NI','ta','TA','nǐ','σ','ς'])+'\t'+str(rng.choice([-1,0,1,100,2147483647])))
 cases.append(dict(pinyin='\n'.join(pinyin),comments='\n'.join(annotation),splits='\n'.join(annotation)))
real_root=ROOT/'data/staging/码表/虎码字词'
comment_files=sorted(real_root.glob('*.注释'));split_files=sorted(real_root.glob('*.拆分'))
pyfiles=sorted((ROOT/'data/staging/拼音反查码表').glob('*.txt'))
def combine(files):return '\n'.join(p.read_text(encoding='utf-8-sig') for p in files)
cases.append(dict(pinyin=combine(pyfiles),comments=combine(comment_files),splits=combine(split_files)))
trace=BUILD/'auxiliary-import.jsonl';trace.write_text(''.join(json.dumps(c)+'\n' for c in cases))
def hextext(s):return ''.join(f'{u[0]:04x}' for u in struct.iter_unpack('<H',s.encode('utf-16-le')))
hexfile=BUILD/'auxiliary-import.hex';hexfile.write_text(''.join(' '.join(hextext(c[k]) for k in ['pinyin','comments','splits'])+'\n' for c in cases))
oracle=BUILD/'auxiliary-import-oracle.jsonl'
with tempfile.TemporaryDirectory(prefix='auxiliary-import-oracle-',dir=BUILD) as tmp,oracle.open('w',encoding='utf-8') as out:
 (Path(tmp)/'.native-tiger-staging').touch()
 subprocess.run(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'auxiliaryrows',win(tmp),win(trace)],stdout=out,check=True)
expected=[json.loads(x) for x in oracle.read_text(encoding='utf-8-sig').splitlines()]
actual=[json.loads(x) for x in subprocess.check_output([str(EXE),win(hexfile)],text=True).splitlines()]
assert len(actual)==len(expected)==len(cases)
failures=[dict(index=i,case=cases[i],expected=a,actual=b) for i,(a,b) in enumerate(zip(expected,actual)) if a!=b]
report=dict(cases=len(cases),mismatches=len(failures),failures=failures[:3],pinyin_groups=sum(len(x['pinyin']) for x in expected),pinyin_candidates=sum(len(e[1]) for x in expected for e in x['pinyin']),comment_entries=sum(len(x['comments']) for x in expected),split_entries=sum(len(x['splits']) for x in expected),real_files=[str(p.relative_to(ROOT)) for p in pyfiles+comment_files+split_files],probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
(BUILD/'auxiliary-import-parity-arm64.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='failures'}));assert not failures
