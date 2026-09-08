"""Validate a real prepared schema before the native selector publishes config."""
import hashlib,json,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];EXE=ROOT/'build/tests/ARM64/schema_select.exe'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
with tempfile.TemporaryDirectory(prefix='schema-selector-',dir=ROOT/'build') as tmp:
 root=Path(tmp);dictionary=ROOT/'data/tiger-v2.tcd';schema=root/'schemas/测试方案Ab';schema.mkdir(parents=True)
 shutil.copyfile(dictionary,schema/'tiger-v2.tcd')
 def select(name):return subprocess.run([str(EXE),win(root),win(dictionary),name],text=True,capture_output=True)
 assert select('虎码字词').returncode==0
 assert select('测试方案ab').returncode==0
 config=root/'config.txt';text=config.read_text(encoding='utf-8-sig')
 assert '当前码表\t测试方案Ab' in text and '最近码表对\t测试方案Ab|虎码字词' in text,text
 before=config.read_bytes()
 for name in ['Missing','../escape']:
  assert select(name).returncode!=0 and config.read_bytes()==before
 bad=root/'schemas/Corrupt';bad.mkdir();(bad/'tiger-v2.tcd').write_bytes(b'broken')
 assert select('Corrupt').returncode!=0 and config.read_bytes()==before
 (schema/'user.tcu').write_bytes(b'broken journal')
 assert select('测试方案ab').returncode!=0 and config.read_bytes()==before
 assert select('虎码字词').returncode==0
 greek=root/'schemas/Σ方案';greek.mkdir();shutil.copyfile(dictionary,greek/'tiger-v2.tcd')
 assert select('ς方案').returncode==0
 assert '当前码表\tΣ方案' in config.read_text(encoding='utf-8-sig')
 config.write_text('当前码表 σ方案\n最近码表对 ς方案|虎码字词\n',encoding='utf-8')
 assert select('Σ方案').returncode==0
 assert '最近码表对\tΣ方案|虎码字词' in config.read_text(encoding='utf-8-sig')
 report=dict(status='passed',ordinal_case_aliases=True,valid_selections=5,failed_selections_preserved=4,unicode_schema=True,selector_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
 (ROOT/'build/schema-selector-arm64.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
