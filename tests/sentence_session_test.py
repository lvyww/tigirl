"""Session lifecycle and initial Engine sentence dispatch, without foreground input."""
import hashlib,json,subprocess
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1]
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
results={}
for platform in ['ARM64','x64','Win32']:
 probe=ROOT/'build/tests'/platform/'sentence_session_probe.exe'
 r=run_windows([probe],capture_output=True,text=True,timeout=30);assert r.returncode==0,r.stderr
 data=json.loads(r.stdout);assert data['status']=='passed'
 results[platform]={'result':data,'sha256':hashlib.sha256(probe.read_bytes()).hexdigest()}
probe=ROOT/'build/tests/ARM64/sentence_engine_probe.exe'
r=run_windows([probe,win(ROOT/'data/tiger-v2.tcd')],capture_output=True,text=True,timeout=30);assert r.returncode==0,r.stderr
results['engine_ARM64']={'result':json.loads(r.stdout),'sha256':hashlib.sha256(probe.read_bytes()).hexdigest()}
report={'status':'passed','platforms':results,'original_engine_differential':False,'tsf_worker_integrated':False,'installed':False,
 'sources':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['native/SentenceSession.h','native/SentenceSession.cpp','native/Engine.h','native/Engine.cpp','tests/sentence_session_probe.cpp','tests/sentence_engine_probe.cpp']}}
(ROOT/'build/sentence-session-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
