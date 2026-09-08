"""Native generation update, selector resolution, rollback and old-file retention."""
import hashlib,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
IMPORT=BUILD/'tests/ARM64/lexicon_import.exe';SELECT=BUILD/'tests/ARM64/schema_select.exe'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
with tempfile.TemporaryDirectory(prefix='schema-generation-',dir=BUILD) as temporary:
 root=Path(temporary);source=root/'source';source.mkdir();table=source/'source.txt';table.write_text('ab 旧词\n',encoding='utf-8')
 user=root/'user';schema=user/'schemas/测试方案';missing=root/'missing-pinyin'
 def publish(mode):return subprocess.run([str(IMPORT),mode,win(source),win(missing),win(user),'测试方案','zh-CN'],capture_output=True,text=True)
 def select():return subprocess.run([str(SELECT),win(user),win(ROOT/'data/tiger-v2.tcd'),'测试方案'],capture_output=True,text=True)
 assert publish('--schema').returncode==0
 assert select().returncode==0
 config=(user/'config.txt').read_bytes();legacy=(schema/'tiger-v2.tcd').read_bytes()
 generations=[]
 for text in ['ab 新词\n','ab 第三版\n']:
  table.write_text(text,encoding='utf-8');result=publish('--update');assert result.returncode==0,result.stderr
  descriptor=(schema/'current.txt').read_text(encoding='utf-8-sig');generation=descriptor.strip().split('\t')[1]
  assert len(generation)==32
  path=schema/'generations'/generation/'tiger-v2.tcd';assert path.is_file()
  generations.append((path,path.read_bytes()));assert select().returncode==0
  assert (schema/'tiger-v2.tcd').read_bytes()==legacy
  for old,data in generations:assert old.read_bytes()==data
 assert generations[0][1]!=generations[1][1]
 selected=(schema/'current.txt').read_bytes();before=(user/'config.txt').read_bytes()
 (schema/'user.tcu').write_bytes(b'broken journal')
 assert publish('--update').returncode!=0
 assert (schema/'current.txt').read_bytes()==selected and (user/'config.txt').read_bytes()==before
 (schema/'user.tcu').unlink()
 for invalid in ['../escape','a'*31,'A'*32,'a'*32]:
  (schema/'current.txt').write_text('generation\t'+invalid+'\n',encoding='utf-8')
  assert select().returncode!=0
  assert (user/'config.txt').read_bytes()==before
 (schema/'current.txt').write_bytes(selected);assert select().returncode==0
 def versions():return subprocess.run([str(SELECT),win(user),win(ROOT/'data/tiger-v2.tcd'),'--versions','测试方案'],capture_output=True,text=True)
 def restore(generation):return subprocess.run([str(SELECT),win(user),win(ROOT/'data/tiger-v2.tcd'),'--restore','测试方案',generation],capture_output=True,text=True)
 corrupt=schema/'generations'/('b'*32);corrupt.mkdir();(corrupt/'tiger-v2.tcd').write_bytes(b'broken')
 listing=versions();assert listing.returncode==0
 assert {'legacy'}|{p.parent.name for p,_ in generations} <= set(listing.stdout.splitlines())
 assert 'b'*32 not in listing.stdout.splitlines()
 config_before=(user/'config.txt').read_bytes()
 for payload,target in [(b'bad descriptor',generations[0][0].parent.name),(b'\xff\xff\xff','legacy')]:
  (schema/'current.txt').write_bytes(payload)
  assert restore(target).returncode==0
  assert select().returncode==0
  assert (user/'config.txt').read_bytes()==config_before
  assert (schema/'current.txt').read_text(encoding='utf-8-sig').strip()=='generation\t'+target
 saved_descriptor=(schema/'current.txt').read_bytes()
 for target in ['../escape','b'*32,'c'*32]:
  assert restore(target).returncode!=0
  assert (schema/'current.txt').read_bytes()==saved_descriptor
 (schema/'user.tcu').write_bytes(b'broken journal')
 assert restore(generations[1][0].parent.name).returncode!=0
 assert (schema/'current.txt').read_bytes()==saved_descriptor
 report=dict(restores=2,failed_restores_preserved=4,validated_version_listing=True,updates=2,legacy_preserved=True,old_generations_preserved=True,failed_journal_update_preserves_selection=True,invalid_descriptors_rejected=4,selector_resolves_generation=True,import_sha256=hashlib.sha256(IMPORT.read_bytes()).hexdigest(),selector_sha256=hashlib.sha256(SELECT.read_bytes()).hexdigest())
(BUILD/'schema-generation-arm64.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
