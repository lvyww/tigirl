"""Original extracted early-commit policy vs native stateful decisions."""
import hashlib,json,random,subprocess,tempfile
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1];rng=random.Random(80731)
def hx(s):return s.encode('utf-16-be',errors='surrogatepass').hex() or '-'
def length(s):return len(s.encode('utf-16-le',errors='surrogatepass'))//2
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
lines=[];steps=0
for trial in range(260):
 lines.append('B '+('3 2 3 1' if trial<20 else f'{rng.choice([1,2,3,5])} {rng.choice([1,2,3])} {rng.choice([0,3,5])} {rng.randrange(2)}'))
 n=4
 stem=['中国人民','😀甲乙丙','e\u0301甲乙丙','甲乙丙丁'][trial%4]
 # Text-element prefixes include surrogate pairs and combining sequences.
 units=[['中','国','人','民'],['😀','甲','乙','丙'],['e\u0301','甲','乙','丙'],['甲','乙','丙','丁']][trial%4]
 for tick in range(40):
  n=max(1,n+(1 if trial<20 else rng.choice([1,1,1,1,0,-1,2])))
  raw='a'*n;full=raw+('a' if (trial%3==0 if trial<20 else rng.random()<.25) else '')
  if trial>=20 and rng.random()<.04:full+='aa'
  enabled,suspended,matching=(1,0,1) if trial<20 else (int(rng.random()>.03),int(rng.random()<.03),int(rng.random()>.04))
  truncated,merged,neutral=(0,0,0) if trial<20 else (int(rng.random()<.04),int(rng.random()<.4),int(rng.random()<.35))
  retain=0 if trial<20 else rng.choice([0,0,3,4,8,32])
  lines.append(f'S {hx(full)} {hx(raw)} {enabled} {suspended} {matching} {retain} {truncated} {merged} {neutral}')
  for i in range(1,5):
   if trial>=20 and rng.random()<.15:continue
   text=''.join(units[:i]);share=1 if trial<20 else rng.choice([.99,.9949,.995,.999,.99999,1,1])
   closed=1 if trial<20 else int(rng.random()>.15)
   lines.append(f'P {hx(text)} {2*i} {share} {closed}')
   if trial>=20 and rng.random()<.1:lines.append(f'P {hx(text)} {2*i} .994 1')
  if trial>=20 and rng.random()<.3:lines.append(f'P {hx(units[0]+"戊")} 4 .999999 1')
  if trial<20 or rng.random()<.8:
   text=stem if trial<20 or rng.random()<.8 else units[0]+'戊'
   supplement=0 if trial<20 else rng.choice([0,0,10])
   boundaries=' '.join(f'{2*i}:{length("".join(units[:i]))}' for i in range(1,5) if trial<20 or rng.random()<.85)
   lines.append(f'C {hx(text)} {supplement} {boundaries}')
  lines.append('X');steps+=1
with tempfile.TemporaryDirectory(prefix='sentence-policy-',dir=ROOT/'build') as tmp:
 fixture=Path(tmp)/'steps.txt';fixture.write_text('\n'.join(lines)+'\n')
 oracle=ROOT/'tools/SentenceOracle/bin/Release/net10.0-windows/SentenceOracle.dll'
 r=run_windows(['/mnt/c/Program Files/dotnet/dotnet.exe',win(oracle),'--auto-commit',win(fixture)],capture_output=True,text=True,timeout=60)
 assert r.returncode==0,r.stderr
 expected=[json.loads(s) for s in r.stdout.splitlines()];assert len(expected)==steps
 commits=sum(s['commit'] is not None for s in expected);assert commits>100,commits
 results={}
 for platform in ['ARM64','x64','Win32']:
  probe=ROOT/'build/tests'/platform/'sentence_auto_commit_probe.exe'
  r=run_windows([probe,win(fixture)],capture_output=True,text=True,timeout=60);assert r.returncode==0,(platform,r.stderr)
  actual=[json.loads(s) for s in r.stdout.splitlines()];assert len(actual)==steps
  for i,(a,b) in enumerate(zip(actual,expected)):assert a==b,(platform,i,a,b)
  results[platform]={'steps':steps,'commits':commits,'sha256':hashlib.sha256(probe.read_bytes()).hexdigest()}
report={'status':'passed','platforms':results,'engine_integrated':False,'sources':{f:hashlib.sha256((ROOT/f).read_bytes()).hexdigest() for f in ['native/SentenceAutoCommit.cpp','native/SentenceAutoCommit.h','tools/SentenceOracle/AutoCommitOracle.cs','tests/sentence_auto_commit_test.py']}}
(ROOT/'build/sentence-auto-commit-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
