"""Persisted wheel increments, bounds and independent writer serialization."""
import json,subprocess,tempfile,hashlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1];EXE=ROOT/'build/tests/ARM64/config_store_probe.exe'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
with tempfile.TemporaryDirectory(prefix='candidate-wheel-',dir=ROOT/'build') as temp:
 path=Path(temp)/'config.txt'
 def wheel(delta):
  r=run_windows([str(EXE),win(path),str(delta),'wheel'],capture_output=True,text=True,timeout=30)
  assert r.returncode==0,(r.stdout,r.stderr)
  return float(r.stdout.strip())
 path.write_text('# keep\n字体大小\t17\n未知\t保留\n',encoding='utf-8-sig')
 assert wheel(120)==17.5
 assert wheel(-120)==17
 assert wheel(60)==17.25
 assert wheel(-60)==17
 assert wheel(32767)==153.53
 assert wheel(32767)==200
 assert wheel(-32768)==63.47
 assert wheel(-32768)==3
 assert wheel(-120)==3
 assert wheel(120)==3.5
 path.write_text('# keep\n字体大小\t17\n未知\t保留\n',encoding='utf-8-sig')
 with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(wheel,[120]*8))
 assert wheel(0)==21
 text=path.read_text(encoding='utf-8-sig');assert '# keep' in text and '未知\t保留' in text and '_reload' not in text
 r=run_windows([str(EXE),win(path),'1','cycle'],capture_output=True,text=True,timeout=30)
 assert r.returncode==0,(r.stdout,r.stderr)
 assert '未知\t保留' in path.read_text(encoding='utf-8-sig')
 report={'cycle_preferences':True,'status':'passed','bounds':[3,200],'step':0.5,'fractional_delta':True,'concurrent_writes':8,'other_fields_preserved':True,'exe_sha256':hashlib.sha256(EXE.read_bytes()).hexdigest()}
 (ROOT/'build/candidate-wheel-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
