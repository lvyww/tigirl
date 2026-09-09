import json,math,random,subprocess,tempfile,hashlib
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1];B=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def hx(s):
 b=s.encode('utf-16-le',errors='surrogatepass');return ''.join(f'{int.from_bytes(b[i:i+2],"little"):04x}' for i in range(0,len(b),2)) or '-'
rng=random.Random(901);units=['甲','乙','丙','丁','😀','e\u0301','👩\u200d💻','\ud800'];lines=[]
for case in range(240):
 lines.append('B')
 entries=[('甲乙',1000),('乙',1000000000),('甲乙',2000),('',1000)] if case==0 else [(''.join(rng.choices(units,k=rng.randrange(1,7))),rng.choice([-1,0,1,11,100,1000,5000,1000000001,9223372036854775807])) for _ in range(rng.randrange(25))]
 lines += ['E '+hx(t)+' '+str(w) for t,w in entries]
 stream=['甲','乙','甲','丁','甲','乙','']+rng.choices(units+['','未命中','甲乙'],k=70)
 lines += ['Q '+str(rng.choice([-2,-2,-2,-1,99999,0]))+' '+hx(t) for t in stream]
 lines.append('X')
with tempfile.TemporaryDirectory(prefix='sentence-supplement-',dir=B) as tmp:
 p=Path(tmp)/'queries.txt';p.write_text('\n'.join(lines)+'\n');results={};expected=None
 for platform in ['oracle','ARM64','x64','Win32']:
  args=['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/SentenceOracle/bin/Release/net10.0-windows/SentenceOracle.dll'),'--supplement',win(p)] if platform=='oracle' else [B/'tests'/platform/'sentence_supplement_probe.exe',win(p)]
  if platform!='oracle':
   output=Path(tmp)/platform;output.mkdir();args.append(win(output))
  r=run_windows(args,capture_output=True,text=True,timeout=60);assert r.returncode==0,(platform,r.stderr)
  actual=[(int(a),float(b)) for a,b in map(str.split,r.stdout.splitlines())]
  if expected is None:expected=actual
  assert len(actual)==len(expected)
  for i,(a,e) in enumerate(zip(actual,expected)):assert a[0]==e[0] and math.isclose(a[1],e[1],abs_tol=1e-12),(platform,i,a,e)
  results[platform]=len(actual)
 report={'status':'passed','transitions':results,'cases':240,'runtime_integrated':False,'mapped_supplement_storage':True,'malformed_graph_cases':5,'mapped_source_sha256':hashlib.sha256((ROOT/'native/MappedSentenceSupplement.h').read_bytes()).hexdigest(),'probe_sha256':{a:hashlib.sha256((B/'tests'/a/'sentence_supplement_probe.exe').read_bytes()).hexdigest() for a in ['ARM64','x64','Win32']},'source_sha256':hashlib.sha256((ROOT/'native/SentenceSupplement.h').read_bytes()).hexdigest()}
(B/'sentence-supplement-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
