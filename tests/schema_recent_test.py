"""Recent selection against prepared schemas and concurrent native selectors."""
import concurrent.futures,hashlib,json,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];EXE=ROOT/'build/tests/ARM64/schema_select.exe'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
with tempfile.TemporaryDirectory(prefix='schema-recent-',dir=ROOT/'build') as tmp:
 root=Path(tmp);dictionary=ROOT/'data/tiger-v2.tcd';config=root/'config.txt'
 args=[str(EXE),win(root),win(dictionary)]
 def run(command,ok=True):
  result=subprocess.run(args+[command],capture_output=True,text=True)
  assert (result.returncode==0)==ok,result.stderr
  return result.stdout.strip()
 assert run('--list')=='虎码字词'
 assert run('--recent')=='' and not config.exists()
 for name in ['Beta','Alpha']:
  directory=root/'schemas'/name;directory.mkdir(parents=True)
  shutil.copyfile(dictionary,directory/'tiger-v2.tcd')
 (root/'schemas/Unprepared').mkdir()
 assert run('--list').splitlines()==['Alpha','Beta','虎码字词']
 config.write_text('# retained\n当前码表 Beta\n最近码表对 Removed|Ghost\n',encoding='utf-8')
 assert run('--recent')=='虎码字词' # Next sorted schema after Beta, absent MRU ignored.
 config.write_text('# retained\n当前码表 bEtA\n最近码表对 ALPHA|Beta\n',encoding='utf-8')
 assert run('--recent')=='Alpha'
 # Every command must choose from metadata read under the publication lock.
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
  selected=list(pool.map(lambda _:run('--recent'),range(16)))
 assert selected.count('Beta')==8 and selected.count('Alpha')==8,selected
 text=config.read_text(encoding='utf-8-sig')
 assert '当前码表\tAlpha' in text and '最近码表对\tAlpha|Beta' in text and '# retained' in text,text
 config.write_text('当前码表 Unknown\n最近码表对 Gone|Ghost\n',encoding='utf-8')
 assert run('--recent')=='Alpha' # Missing current falls back to the first sorted schema.
 config.write_text('当前码表 虎码字词\n最近码表对 Beta|虎码字词\n',encoding='utf-8')
 (root/'schemas/Beta/user.tcu').write_bytes(b'broken journal')
 before=config.read_bytes();run('--recent',False);assert config.read_bytes()==before
 report=dict(status='passed',single_schema_noop=True,prepared_catalog_sorted=True,mru_and_fallback=True,concurrent_switches=16,corrupt_target_preserves_configuration=True,selector_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
 (ROOT/'build/schema-recent-arm64.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
