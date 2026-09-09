"""Original SentenceLexiconIndex vs serialized native sentence lexicon views."""
import hashlib,json,math,random,subprocess,tempfile
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def hx(s):return ''.join(f'{int.from_bytes(s.encode("utf-16-le",errors="surrogatepass")[i:i+2],"little"):04x}' for i in range(0,len(s.encode('utf-16-le',errors='surrogatepass')),2)) or '-'
rng=random.Random(33117);cases=[]
base=[('ldac',['燕']),('ladc',['燕']),('l',['燕']),('xy',['甲','燕']),('pq',['乙']),('aaa',['甲']),('a',['甲']),('z',['字词','字词','']),('e',['e\u0301','😀','👩\u200d💻','\ud800'])]
for common in [[],['燕','甲'],['燕','甲','字词','e\u0301','😀']]:
 for white in [[],['燕'],['甲','e\u0301']]:cases.append((base,common,white))
cases += [([],[],[]),([(' ',['空']),('x',[])],[],[]),([('Z',['甲']),('z',['乙']),('\u00a0ABC\u00a0',['丙']),('abc',['丁','丁'])],[],[])]
chars=['甲','乙','丙','丁','燕','字词','e\u0301','😀','👩\u200d💻','\ud800']
codes=['a','b','ab','aa','abb','bba','ldac','ladc','Q','q','Σ','ς','İ','ı','ſ','S','𐐀','𐐨']
for _ in range(240):
 entries=[(rng.choice(codes),[rng.choice(chars+['']) for _ in range(rng.randrange(6))]) for _ in range(rng.randrange(1,35))]
 cases.append((entries,rng.sample(chars,rng.randrange(len(chars)+1)),rng.sample(chars,rng.randrange(4))))
lines=[]
for i,(entries,common,white) in enumerate(cases):
 queries=set(['','none','L','l','la','ld','p','x','a','e','z','S','s','ı','I','Σ','σ','ς','𐐀','𐐨'])
 for code,values in entries:
  queries.add(code);queries.add(code.strip());queries.add(code.lower())
  queries.update(code[:n] for n in range(1,len(code)))
 lines+=['B '+str(i),'C '+' '.join(map(hx,common)),'W '+' '.join(map(hx,white))]
 lines+=['E '+hx(code)+' '+' '.join(map(hx,values)) for code,values in entries]
 lines+=['Q '+hx(q) for q in sorted(queries)]+['X']
with tempfile.TemporaryDirectory(prefix='sentence-lexicon-',dir=BUILD) as temp:
 root=Path(temp);fixture=root/'fixtures.txt';fixture.write_text('\n'.join(lines)+'\n')
 oracle=ROOT/'tools/SentenceOracle/bin/Release/net10.0-windows/SentenceOracle.dll'
 r=run_windows(['/mnt/c/Program Files/dotnet/dotnet.exe',win(oracle),'--lexicon',win(fixture)],capture_output=True,text=True,timeout=60)
 assert r.returncode==0,r.stderr
 expected=[json.loads(line) for line in r.stdout.splitlines()];assert len(expected)==len(cases)
 results={}
 def compare(a,b,path=''):
  if isinstance(a,float) or isinstance(b,float):assert math.isclose(a,b,rel_tol=1e-14,abs_tol=1e-14),(path,a,b)
  elif isinstance(a,dict):
   assert a.keys()==b.keys(),path
   for k in a:compare(a[k],b[k],path+'/'+k)
  elif isinstance(a,list):
   assert len(a)==len(b),(path,a,b)
   for i,(x,y) in enumerate(zip(a,b)):compare(x,y,path+'/'+str(i))
  else:assert a==b,(path,a,b)
 for platform in ['ARM64','x64','Win32']:
  out=root/platform;out.mkdir();probe=BUILD/'tests'/platform/'sentence_lexicon_probe.exe'
  r=run_windows([probe,win(fixture),win(out)],capture_output=True,text=True,timeout=60);assert r.returncode==0,(platform,r.stderr)
  actual=[json.loads(line) for line in r.stdout.splitlines()];compare(actual,expected,platform)
  results[platform]={'cases':len(actual),'queries':sum(len(c['queries']) for c in actual),'probe_sha256':hashlib.sha256(probe.read_bytes()).hexdigest()}
 report={'status':'passed','platforms':results,'source_order_ties':True,'filtered_prefixes':True,'common_and_whitelist':True,
  'empty_lexicon':True,'unicode_ordinal_and_graphemes':True,'serialized_readonly_views':True,'same_process_mapping_reuse':True,
  'runtime_integration':False,'source_sha256':hashlib.sha256((ROOT/'native/SentenceLexicon.cpp').read_bytes()).hexdigest()}
(BUILD/'sentence-lexicon-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
