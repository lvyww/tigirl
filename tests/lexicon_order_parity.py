"""Compare actual Windows directory enumeration/collation with original loader."""
import hashlib,json,random,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';EXE=BUILD/'tests/ARM64/lexicon_order_probe.exe'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
names=['方案.txt','方案.dict.yaml','构词.txt','用户调整.txt','常用符号.txt','快符.txt','a.txt','B.txt','á.txt','a\u0301.txt','ä.txt','Å.txt','z.txt','10.txt','2.txt','a-b.txt','ab.txt',"a'b.txt",'a b.txt','a_b.txt','a.b.txt','a\u00adb.txt','Σ.txt','ς2.txt','İ.txt','ı.txt','北京.txt','重庆.txt','广州.txt','中文.TXT','unicode.DICT.YAML','a.dict.yaml','!abc.txt','😀.txt','𠀀.txt','.txt','.dict.yaml','ignored.txtx','ignored.yaml','ignored.注释','ignored.拆分']
cultures=['','en-US','zh-CN','zh-TW','sv-SE','tr-TR','de-DE','ja-JP']
cases=[]
with tempfile.TemporaryDirectory(prefix='order-oracle-',dir=BUILD) as temporary:
 root=Path(temporary);(root/'.native-tiger-staging').touch()
 rng=random.Random(20260913)
 for schema in ['方案','a','Σ方案','empty']:
  directory=root/schema;directory.mkdir()
  files=names+[schema.upper()+'.TXT',schema.upper()+'.DICT.YAML'] if schema!='empty' else []
  rng.shuffle(files)
  for name in files:(directory/name).touch(exist_ok=True)
  (directory/'not-a-file.txt').mkdir();(directory/'not-a-file.txt'/'nested.txt').touch()
  for culture in cultures:cases.append(dict(directory=schema,culture=culture))
 real=root/'真实码表'/'虎码字词';real.mkdir(parents=True)
 # Ordering depends on names and enumeration, not file contents.
 for source in (ROOT/'data/staging/码表/虎码字词').iterdir():
  if source.is_file():(real/source.name).touch()
 for culture in cultures:cases.append(dict(directory='真实码表/虎码字词',culture=culture))
 trace=root/'cases.jsonl';trace.write_text(''.join(json.dumps(c)+'\n' for c in cases))
 expected=[json.loads(line) for line in subprocess.check_output(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'orderfiles',win(root),win(trace)],text=True).splitlines()]
 actual=[subprocess.check_output([str(EXE),win(root/c['directory']),c['culture']],text=True).splitlines() for c in cases]
 # A missing directory is an import error, not an empty valid schema.
 missing=subprocess.run([str(EXE),win(root/'missing'),'en-US'],capture_output=True)
 assert missing.returncode!=0
assert len(actual)==len(expected)==len(cases)
failures=[dict(case=c,expected=e,actual=a) for c,e,a in zip(cases,expected,actual) if e!=a]
report=dict(cases=len(cases),cultures=cultures,mismatches=len(failures),failures=failures[:4],missing_directory_rejected=True,probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
(BUILD/'lexicon-order-parity-arm64.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report));assert not failures
