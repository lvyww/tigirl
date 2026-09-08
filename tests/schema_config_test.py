"""Schema/MRU publication, unrelated-setting concurrency and failure preservation."""
import hashlib,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];EXE=ROOT/'build/tests/ARM64/config_store_probe.exe'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def run(p,n,mode):return subprocess.check_output([str(EXE),win(p),str(n),mode],text=True).strip()
def values(p):
 return dict(line.split('\t',1) for line in p.read_text(encoding='utf-8-sig').splitlines() if '\t' in line and not line.startswith('#'))
with tempfile.TemporaryDirectory(prefix='schema-config-',dir=ROOT/'build') as tmp:
 p=Path(tmp)/'config.txt'
 p.write_text('# retained\n主题 海蓝\n当前码表 Old\n最近码表对 Old|Older\n当前码表,Alpha\n',encoding='utf-16')
 sequences=[('Beta','Beta|Alpha'),('Gamma','Gamma|Beta'),('BETA','BETA|Gamma'),('beta','beta|Gamma'),('虎码字词','虎码字词|beta')]
 for target,pair in sequences:
  run(p,1,'select='+target);v=values(p)
  assert v['当前码表']==target and v['最近码表对']==pair,v
  assert p.read_text(encoding='utf-8-sig').count('当前码表')==1
  assert '# retained\n主题 海蓝\n' in p.read_text(encoding='utf-8-sig')
 p.write_text('# retained\n主题 海蓝\n当前码表 Alpha\n最近码表对\t\u3000Alpha\u3000|alpha|| Beta |Gamma\n',encoding='utf-8')
 run(p,1,'select=Beta');assert values(p)['最近码表对']=='Beta|Alpha'
 # Schema writers and an unrelated toggle must serialize against the same lock.
 jobs=[subprocess.Popen([str(EXE),win(p),'31','select='+target],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) for target in ['Alpha','Beta','Gamma','虎码字词']]
 toggles=[subprocess.Popen([str(EXE),win(p),'17'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) for _ in range(2)]
 readers=[subprocess.Popen([str(EXE),win(p),'1000','schema-read-loop'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) for _ in range(2)]
 for job in jobs+toggles+readers:
  out,err=job.communicate(timeout=60);assert job.returncode==0,err
 v=values(p);recent=v['最近码表对'].split('|')
 assert v['当前码表']==recent[0] and len(recent)==2 and recent[0].casefold()!=recent[1].casefold(),v
 assert v['隐藏候选']=='否',v
 before=p.read_bytes();assert run(p,1,'deny-select')=='blocked';assert p.read_bytes()==before
 for target in ['', '..', '../escape','a|b','a\nb','a.','a/child']:
  bad=subprocess.run([str(EXE),win(p),'1','select='+target],text=True,capture_output=True)
  assert bad.returncode!=0 and p.read_bytes()==before,target
 report=dict(status='passed',ordered_selections=len(sequences),unicode_trim_and_case_dedup=True,schema_writes=124,concurrent_toggles=34,consistent_reader_snapshots=2000,failed_selection_preserves_original=True,invalid_names=7,probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
 (ROOT/'build/schema-config-arm64.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
