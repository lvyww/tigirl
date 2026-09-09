"""Native files-to-binary import plus immutable no-replace publication."""
import concurrent.futures,hashlib,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';EXE=BUILD/'tests/ARM64/Tigirl.Import.exe'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
def run(schema,pinyin,output):
 return subprocess.run([str(EXE),win(schema),win(pinyin),win(output),'zh-CN'],capture_output=True,text=True)
with tempfile.TemporaryDirectory(prefix='publish-import-',dir=BUILD) as temporary:
 root=Path(temporary);output=root/'完整码表'/'tiger.tcd'
 actual=run(ROOT/'data/staging/码表/虎码字词',ROOT/'data/staging/拼音反查码表',output)
 assert actual.returncode==0,actual.stderr
 expected=(ROOT/'data/tiger-v2.tcd').read_bytes();binary=output.read_bytes()
 assert binary==expected,dict(native_bytes=len(binary),expected_bytes=len(expected),native_sha256=hashlib.sha256(binary).hexdigest(),expected_sha256=hashlib.sha256(expected).hexdigest())
 schema=root/'小方案';schema.mkdir();(schema/'小方案.txt').write_text('ab 测试\n',encoding='utf-8')
 missing_pinyin=root/'没有拼音目录'
 protected=root/'protected.tcd';protected.write_bytes(b'existing generation')
 denied=run(schema,missing_pinyin,protected)
 assert denied.returncode!=0 and protected.read_bytes()==b'existing generation'
 destination=root/'并发发布.tcd'
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
  results=list(pool.map(lambda _:run(schema,missing_pinyin,destination),range(4)))
 assert sum(r.returncode==0 for r in results)==1,[r.stderr for r in results]
 successful=json.loads(next(r.stdout for r in results if r.returncode==0))
 assert successful['pinyin_records']==0
 assert not list(root.rglob('.tiger-*.tmp')),'Temporary files left after publication failure'
 invalid=root/'空方案';invalid.mkdir()
 rejected=run(invalid,missing_pinyin,root/'empty.tcd')
 assert rejected.returncode!=0 and not (root/'empty.tcd').exists()
 user=root/'用户目录';selector=BUILD/'tests/ARM64/Tigirl.SchemaSelect.exe'
 def import_schema(name):
  return subprocess.run([str(EXE),'--schema',win(schema),win(missing_pinyin),win(user),name,'zh-CN'],capture_output=True,text=True)
 def select(name):return subprocess.run([str(selector),win(user),win(ROOT/'data/tiger-v2.tcd'),name],capture_output=True,text=True)
 assert import_schema('新方案').returncode==0
 assert '新方案' in select('--list').stdout
 assert select('新方案').returncode==0
 config=(user/'config.txt').read_bytes();saved=(user/'schemas/新方案/tiger-v2.tcd').read_bytes()
 for name in ['新方案','虎码字词','../escape','bad/name']:
  assert import_schema(name).returncode!=0
  assert (user/'config.txt').read_bytes()==config
  assert (user/'schemas/新方案/tiger-v2.tcd').read_bytes()==saved
 with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
  aliases=list(pool.map(import_schema,['Σ方案','ς方案']))
 assert sum(r.returncode==0 for r in aliases)==1,[r.stderr for r in aliases]
 assert select('σ方案').returncode==0
 # Empty directories left by failed publication can be reused without deleting
 # any contents. Unicode aliases must resolve to the same existing directory.
 recovered=user/'schemas/Σ恢复';recovered.mkdir()
 assert import_schema('ς恢复').returncode==0
 assert (recovered/'tiger-v2.tcd').is_file()
 assert select('σ恢复').returncode==0
 selected=(user/'config.txt').read_bytes()
 for name,filename in [('保留用户数据','user.tcu'),('保留临时数据','.tiger-interrupted.tmp')]:
  directory=user/'schemas'/name;directory.mkdir();payload=directory/filename;payload.write_bytes(b'preserve recovery data')
  assert import_schema(name).returncode!=0
  assert payload.read_bytes()==b'preserve recovery data'
  assert not (directory/'tiger-v2.tcd').exists()
  assert (user/'config.txt').read_bytes()==selected
 report=dict(empty_directory_recovered=True,nonempty_recovery_directories_preserved=2,named_schema_selectable=True,invalid_schema_rejections=4,concurrent_unicode_aliases=2,unicode_alias_successes=1,real_schema_byte_identical=True,bytes=len(binary),sha256=hashlib.sha256(binary).hexdigest(),concurrent_publishers=4,successful_publishers=1,existing_destination_preserved=True,missing_optional_pinyin=True,empty_main_rejected=True,temporary_files_cleaned=True,exe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
(BUILD/'lexicon-publish-arm64.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
