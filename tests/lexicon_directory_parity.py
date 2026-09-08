"""Ordered file-to-main-snapshot integration against original BuildLexiconSnapshot."""
import hashlib,json,shutil,struct,subprocess,tempfile,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';EXE=BUILD/'tests/ARM64/construct_import_probe.exe'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
def hexvalues(value):
 if isinstance(value,str):return ''.join(f'{u[0]:04x}' for u in struct.iter_unpack('<H',value.encode('utf-16-le','surrogatepass')))
 if isinstance(value,list):return [hexvalues(x) for x in value]
 if isinstance(value,dict):return {k:hexvalues(v) for k,v in value.items()}
 return value
all_sections="--all" in sys.argv
cases=[]
with tempfile.TemporaryDirectory(prefix='directory-import-',dir=BUILD) as temporary:
 root=Path(temporary);(root/'.native-tiger-staging').touch()
 if all_sections:
  pyroot=root/'拼音反查码表';shutil.copytree(ROOT/'data/staging/拼音反查码表',pyroot)
  (pyroot/'.txt').write_text('ce 测试甲 10\n',encoding='utf-8')
  (pyroot/'a.TXT').write_text('ce 测试乙 10\n测试丙\tCE\t20\n无编码忽略\n',encoding='utf-8')
  (pyroot/'ignored.dict.yaml').write_text('...\nce 不该导入 999\n',encoding='utf-8')
 for variant in range(4):
  directory=root/f'方案{variant}';directory.mkdir()
  files={f'方案{variant}.txt':'ab 主表 10\n甲\tjia\n乙\tyii\n甲乙\n',f'方案{variant}.dict.yaml':'name: test\n...\nab 同名YAML 10\n',
   'ä.txt':'ab 元音 10\n','z.txt':'ab 尾部 10\n','a.dict.yaml':'...\nab 普通YAML 10\n',
   '用户调整.txt':'ab 普通调整行 10\n{置顶}ab\t尾部\n{前移}ab\t普通YAML\n{删除}jia\t甲\n',
   '补充语料.txt':'ab 不该导入 10000\n','ignored.注释':'ab 忽略\n'}
  if all_sections:
   files.update({'.注释':'甲 第一\n甲 第一\n','b.注释':'甲 第二\n','a.注释':'甲 \\n\n甲=>乙 值=>别名\n',
    '.拆分':'甲 初始\n','b.拆分':'甲 最后\n','a.拆分':'甲 中间\n'})
  if variant==1:files['构词.txt']='甲\tqx\n乙\twy\n甲乙\n'
  if variant==2:files['构词.txt']=''
  if variant==3:files.pop('用户调整.txt')
  for i,(name,text) in enumerate(files.items()):
   enc,bom=[('utf-8',b''),('utf-16-le',b'\xff\xfe'),('utf-16-be',b'\xfe\xff')][i%3]
   (directory/name).write_bytes(bom+text.encode(enc))
  (directory/'nested.txt').mkdir();(directory/'nested.txt'/'bad.txt').write_text('ab 嵌套忽略 10000')
  for culture in ['en-US','zh-CN','sv-SE','tr-TR']:cases.append(dict(directory=directory.name,culture=culture))
 real=root/'虎码字词';shutil.copytree(ROOT/'data/staging/码表/虎码字词',real)
 cases.append(dict(directory=real.name,culture='zh-CN'))
 if all_sections:
  for case in cases:case['all']=True
 trace=root/'cases.jsonl';trace.write_text(''.join(json.dumps(c)+'\n' for c in cases))
 paths=root/'paths.txt';paths.write_text(''.join(c['culture']+'\t'+win(root/c['directory'])+('\t'+win(root/'拼音反查码表') if all_sections else '')+'\n' for c in cases),encoding='utf-8')
 expected=[json.loads(line) for line in subprocess.check_output(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'constructrows',win(root),win(trace)],text=True).splitlines()]
 actual=[hexvalues(json.loads(line)) for line in subprocess.check_output([str(EXE),win(paths),'--all-directories' if all_sections else '--directories'],text=True).splitlines()]
assert len(actual)==len(expected)==len(cases)
failures=[dict(case=c,expected=e,actual=a) for c,e,a in zip(cases,expected,actual) if e!=a]
report=dict(all_sections=all_sections,cases=len(cases),mismatches=len(failures),failures=failures[:1],main_entries=sum(len(x['main']) for x in expected),indexed_entries=sum(len(x['indexed']) for x in expected),real_schema=True,probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
(BUILD/('lexicon-all-directory-parity-arm64.json' if all_sections else 'lexicon-directory-parity-arm64.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='failures'}));assert not failures
