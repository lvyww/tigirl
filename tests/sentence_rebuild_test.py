"""Rebuild sentence metadata from mapped source order and live user snapshots."""
import hashlib,json,subprocess,tempfile
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1]
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
results={}
for platform in ['ARM64','x64','Win32']:
 probe=ROOT/'build/tests'/platform/'sentence_rebuild_probe.exe'
 with tempfile.TemporaryDirectory(prefix='sentence-rebuild-',dir=ROOT/'build') as temp:
  r=run_windows([probe,win(temp)],capture_output=True,text=True,timeout=60)
  assert r.returncode==0,(platform,r.stdout,r.stderr)
  result=json.loads(r.stdout);assert result['status']=='passed'
  results[platform]={'result':result,'sha256':hashlib.sha256(probe.read_bytes()).hexdigest()}
assert len({r['result']['revision'] for r in results.values()})==1,'Revision differs across architectures'
report={'status':'passed','platforms':results,'tsf_integrated':False,'sources':{
 str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
 for p in [ROOT/'native'/f for f in ['SentenceRebuild.cpp','SentenceRebuild.h','SentenceRevision.h','SentenceLexicon.cpp','Lexicon.cpp','Lexicon.h']]}}
(ROOT/'build/sentence-rebuild-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
