"""Compare construction lookup, uncoded inference and full-code map to Core."""
import hashlib,json,random,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';EXE=BUILD/'tests/ARM64/construct_import_probe.exe'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
base='甲\tab\t10\n甲\taaaa\t5\n乙\tcd\t3\n丙\tef\t2\n丁\tgh\t1\n'
phrases=['甲乙','甲乙丙','甲乙丙丁','甲😀乙','甲，乙','A甲b','ABCD','甲','甲乙=>甲丙','😀','é','👩🏽\u200d💻','未知']
cases=[]
for construction in [None,'','甲\tx\t1\n甲\tzzzz\t999\n','甲\txy\t1\n甲\tzzzz\t999\n乙\top\n甲乙\t100\n','显示=>甲\tyz\n甲乙\tzzzz\n甲乙丙\t500\n']:
 cases.append(dict(text=base+'\n'.join(p+'\t100' for p in phrases),construction=construction))
for codes in [['oa','za','aa',';a'],['ab','abcd','aa'],['éa','oa','za'],['𐐀a',';a'],['Ⅳa','za'],['aa','bb']]:
 cases.append(dict(text='\n'.join(f'甲\t{code}\t{len(codes)-i}' for i,code in enumerate(codes))+'\n乙\tcd\n甲乙\t100\n',construction=None))
rng=random.Random(20260910)
characters=['甲','乙','丙','丁','A','a','é','👩🏽\u200d💻','🇨🇳','𤕫']
for i in range(200):
 rows=[]
 for _ in range(50):
  char=rng.choice(characters);code=rng.choice(['a','ab','aaaa','ba','oa','ob','za','zb',';a','éa','𐐀a','Ⅳa','cd','ef','gh'])
  if rng.random()<.15:char='显示=>'+char
  rows.append(f'{char}\t{code}\t{rng.randrange(-5,20)}')
 for _ in range(25):
  word=''.join(rng.choice(characters+['，','😀','?']) for _ in range(rng.randrange(1,7)))
  rows.append(f'{word}\t{rng.randrange(-5,20)}')
 construction=None if i%3==0 else '\n'.join(f'{rng.choice(characters)}\t{rng.choice(["x","xy","zz","ab",";q"])}\t{rng.randrange(100)}' for _ in range(8))
 if construction is not None:construction+='\n甲乙\t200\n甲乙丙\t201\n'
 cases.append(dict(text='\n'.join(rows),construction=construction))
adjust_base='别名甲=>甲\tab\t10\n另一甲=>甲\tAB\t9\n乙\tab\t8\n丙\tab\t7\n甲\tcd\t4\n乙\tEF\t3\n'
payloads=['ab\t甲','AB\t新显示=>甲','ab\t乙','ab\t丙','ab\t丁','new\t丁','cd\t甲','missing\t甲','ab 甲','\tab\t甲','ab\t\t甲','ab\t甲\t忽略','ab\t#字面','ab\t\\s甲','ab\t',' \t甲']
for action in ['添加','删除','置顶','前移']:
 for payload in payloads:
  cases.append(dict(text=adjust_base,construction=None,adjustments='{'+action+'}'+payload+'\n'))
for _ in range(80):
 adjustments=[]
 for _ in range(50):
  adjustments.append('{'+rng.choice(['添加','删除','置顶','前移','未知'])+'}'+rng.choice(payloads))
 adjustments+=['丁\tab\t99','甲乙\t20','# adjustment comment']
 cases.append(dict(text=adjust_base,construction=rng.choice([None,'甲\txy\n']),adjustments='\n'.join(adjustments)))
cases.append(dict(text='甲\tσ\n乙\tς\n',construction=None,adjustments='{删除}Σ\t甲\n{添加}ς\t丙\n{置顶}σ\t乙\n'))
for text,adjustment in [
 ('甲\t;\n乙\t;a\n','{删除};a\t乙'),
 ('甲\ta\n乙\tz\n丙\taz\n','{删除}az\t丙'),
 ('甲\ta\n乙\tz\n',''),
 ('甲\t;\n乙\t/\n丙\t[\n丁\tza\n戊\ta\n',''),
 ('甲\tσ\t10\n乙\tςa\t9\n',''),
 ('甲\tab\n','{删除}ab\t甲'),
 ('甲\tab\n','{添加};\t乙\n{添加};a\t丙\n{删除};\t乙')]:
 cases.append(dict(text=text,construction=None,adjustments=adjustment))
# Exercise the full real row volume; file precedence is a separate pending stage.
real_files=sorted(p for p in (ROOT/'data/staging/码表/虎码字词').iterdir() if p.suffix=='.txt' or p.name.endswith('.dict.yaml'))
real_parts=[]
for path in real_files:
 text=path.read_text(encoding='utf-8-sig')
 if path.name.endswith('.dict.yaml'):
  lines=text.split('\n');boundary=next(i for i,line in enumerate(lines) if line.strip()=='...')
  text='\n'.join(lines[boundary+1:])
 real_parts.append(text)
cases.append(dict(text='\n'.join(real_parts),construction=None))
def hextext(s):return ''.join(f'{u[0]:04x}' for u in struct.iter_unpack('<H',s.encode('utf-16-le')))
trace=BUILD/'construct-import.jsonl';trace.write_text(''.join(json.dumps(c)+'\n' for c in cases))
hexfile=BUILD/'construct-import.hex';hexfile.write_text(''.join(str(int(c['construction'] is not None))+' '+hextext(c['text'])+' '+hextext(c['construction'] or '')+(' '+hextext(c['adjustments']) if 'adjustments' in c else '')+'\n' for c in cases))
oracle=BUILD/'construct-import-oracle.jsonl'
with tempfile.TemporaryDirectory(prefix='construct-import-oracle-',dir=BUILD) as tmp,oracle.open('w',encoding='utf-8') as out:
 (Path(tmp)/'.native-tiger-staging').touch()
 subprocess.run(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'constructrows',win(tmp),win(trace)],stdout=out,check=True)
expected=[json.loads(x) for x in oracle.read_text(encoding='utf-8-sig').splitlines()]
actual=[json.loads(x) for x in subprocess.check_output([str(EXE),win(hexfile)],text=True).splitlines()]
def hexvalues(value):
 if isinstance(value,str):return ''.join(f'{u[0]:04x}' for u in struct.iter_unpack('<H',value.encode('utf-16-le',errors='surrogatepass')))
 if isinstance(value,list):return [hexvalues(x) for x in value]
 if isinstance(value,dict):return {k:hexvalues(v) for k,v in value.items()}
 return value
actual=[hexvalues(row) for row in actual]
assert len(actual)==len(expected)==len(cases)
failures=[dict(index=i,case=cases[i],expected=a,actual=b) for i,(a,b) in enumerate(zip(expected,actual)) if a!=b]
report=dict(adjustment_cases=sum('adjustments' in c for c in cases),real_files=[str(p.relative_to(ROOT)) for p in real_files],string_representation='verbatim UTF-16 code units in hex',cases=len(cases),mismatches=len(failures),failures=failures[:3],indexed_entries=sum(len(x['indexed']) for x in expected),main_entries=sum(len(x['main']) for x in expected),construct_entries=sum(len(x['construct']) for x in expected),full_entries=sum(len(x['full']) for x in expected),probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
(BUILD/'construct-import-parity-arm64.json').write_text(json.dumps(report,ensure_ascii=True,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!="failures"},ensure_ascii=True));assert not failures
