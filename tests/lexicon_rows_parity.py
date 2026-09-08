"""Compare import row parsing to the unmodified original ParseMbFile method."""
import hashlib,itertools,json,random,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';EXE=BUILD/'tests/ARM64/lexicon_rows_probe.exe'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
texts=['',' # comment','ab 甲 乙 100','ab\t甲\t100','甲\tab\t100','甲\t100\tab','甲 100','甲','ab 1','AB 甲','a_ 甲','a` 甲','显示=>提交 ab 9','同=>同 ab','\\s ab','{添加}ab 甲','{删除}ab 甲','{置顶}ab 甲','{前移}ab 甲','甲 ab #注释']
for slash in range(8):texts += ['ab 甲'+'\\'*slash+'#乙 12','甲'+'\\'*slash+'#乙\tab\t3']
for first,sep,second,third in itertools.product(['ab','甲',';','123','İ','ＡＢ'],[' ','\t'],['ab','甲','0','+1','-1','2147483647','2147483648','-2147483648','-2147483649'],['',' 3','\t4',' +2147483648',' 1\0',' \u00a01']):
 texts.append(first+sep+second+third)
for space in ['\t','\v','\f','\u0085','\u00a0','\u1680','\u2000','\u2028','\u2029','\u202f','\u205f','\u3000']:
 texts += [space+'ab 甲 3'+space,'ab '+space+'甲'+space,'甲\tab\t'+space+'3'+space]
rng=random.Random(20260908)
for _ in range(500):
 parts=[rng.choice(['ab','AB','甲','乙','1','-3','2147483648','\\s','\\#','=>','显示=>提交','#tail','Bime20231222BIME']) for _ in range(rng.randrange(1,6))]
 texts.append(rng.choice([' ','\t','  ']).join(parts))
cases=[dict(text=t,yaml=False) for t in texts]
for header in ['', '---\nname: test\n', '# comment\n...\n', ' ... \r\n', '... # comment\n', '---\n...\n']:
 cases.append(dict(text=header+'ab 甲 乙 100\r甲\tab\t3\n{添加}xx 丙\n',yaml=True))
# Multi-line grouping verifies YAML state and encounter order, including repeats.
for ending in ['\n','\r','\r\n']:
 cases.append(dict(text=ending.join(texts[:60]),yaml=False))
real_files=sorted((ROOT/'data/staging/码表/虎码字词').iterdir())
real_files=[p for p in real_files if p.suffix=='.txt' or p.name.endswith('.dict.yaml')]
for p in real_files:cases.append(dict(text=p.read_text(encoding='utf-8-sig'),yaml=p.name.endswith('.dict.yaml')))
trace=BUILD/'lexicon-rows.jsonl';trace.write_text(''.join(json.dumps(c,ensure_ascii=True)+'\n' for c in cases))
def hexcase(case):
 import struct
 units=struct.iter_unpack('<H',case['text'].encode('utf-16-le'))
 return str(int(case['yaml']))+' '+''.join(f'{unit[0]:04x}' for unit in units)+'\n'
hexfile=BUILD/'lexicon-rows.hex';hexfile.write_text(''.join(hexcase(c) for c in cases))
oracle=BUILD/'lexicon-rows-oracle.jsonl'
with tempfile.TemporaryDirectory(prefix='lexicon-rows-oracle-',dir=BUILD) as tmp,oracle.open('w',encoding='utf-8') as out:
 shutil.copytree(ROOT/'data/staging',tmp,dirs_exist_ok=True)
 subprocess.run(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'parserows',win(tmp),win(trace)],stdout=out,check=True)
expected=[json.loads(x) for x in oracle.read_text(encoding='utf-8-sig').splitlines()]
actual=[json.loads(x) for x in subprocess.check_output([str(EXE),win(hexfile)],text=True).splitlines()]
with tempfile.TemporaryDirectory(prefix='lexicon-file-rows-',dir=BUILD) as tmp:
 paths=[];directory=Path(tmp);windows=win(directory)
 for i,case in enumerate(cases):
  name=f'中文词条-{i}'+('.DiCt.YaMl' if case['yaml'] else '.TXT')
  encoding,bom=[('utf-8',b''),('utf-8',b'\xef\xbb\xbf'),('utf-16-le',b'\xff\xfe'),('utf-16-be',b'\xfe\xff'),('utf-32-be',b'\x00\x00\xfe\xff')][i%5]
  (directory/name).write_bytes(bom+case['text'].encode(encoding))
  paths.append(windows+'\\'+name)
 pathlist=directory/'files.txt';pathlist.write_text('\n'.join(paths)+'\n',encoding='utf-8')
 file_actual=[json.loads(x) for x in subprocess.check_output([str(EXE),win(pathlist),'--files'],text=True).splitlines()]
 assert file_actual==expected,'File decode/parse pipeline mismatch'
assert len(actual)==len(expected)==len(cases)
failures=[dict(case=cases[i],expected=a,actual=b) for i,(a,b) in enumerate(zip(expected,actual)) if a!=b]
report=dict(native_file_cases=len(file_actual),file_encodings=['utf-8','utf-8-bom','utf-16-le','utf-16-be','utf-32-be'],unicode_paths=True,real_files=[str(p.relative_to(ROOT)) for p in real_files],cases=len(cases),mismatches=len(failures),failures=failures[:12],coded_rows=sum(len(x['coded']) for x in expected),uncoded_rows=sum(len(x['uncoded']) for x in expected),probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
(BUILD/'lexicon-rows-parity-arm64.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
assert not failures
