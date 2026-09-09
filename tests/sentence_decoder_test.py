"""Full native sentence lattice against frozen original, including real n-grams."""
import hashlib,json,math,random,subprocess,tempfile,os
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def hx(s):
 b=s.encode('utf-16-le',errors='surrogatepass')
 return ''.join(f'{int.from_bytes(b[i:i+2],"little"):04x}' for i in range(0,len(b),2)) or '-'
rng=random.Random(20260909);cases=[]
base=[('a',['的','一','是','😀','e\u0301']),('aa',['的','一','是']),('ab',['中国','中','𰻞']),('bb',['人','人民','的']),('aaaa',['的的','一']),(';a',['符']),('/a',['号']),('[a',['括'])]
queries=['','123',';','a','aa','aaaa','aaaaaa','a1a1','a0','a00','a01','a2a3','abbb','abbbab','a;a\'',';aaa','aa;a','/aaa','[aaa','a2147483648','a١',' A\tA\u0085a A ','ab;','bb1bb2','a999','a00aa']
for dup in [0,1]:
 for beam in [1,2,2000]:
  for isolation in [0,3000]:
   for boundary in [0,1]:
    cases.append((base,[],[],[('中国人民',10000),('的的',1000)],queries,[beam,.03,isolation,2,0,boundary,2,5,dup,20]))
# Duplicate text reached by different segmentations; >256 pending states in original.
wide=[('aa',['的'*i for i in range(1,31)]),('aaaa',['的'*i for i in range(1,31)]),('bb',['人'])]
cases.append((wide,[],[],[],['aaaa','aaaaaaaa','aaaaaaaaaaaa'],[7,.03,0,2,0,1,0,0,1,20]))
# Composition traces exercise append/delete and selector-aware lattice reuse.
for dup in [0,1]:
 for beam in [1,7,2000]:
  raw='aabbaabbaabb'
  trace=[raw[:n] for n in range(1,len(raw)+1)]+[raw[:n] for n in range(len(raw),0,-1)]
  trace+=['aabbaa','aabbaa','aabbaa1','aabbaa12','aabbaa1','aabbaa','aabbaa;', 'aabbaa;aa','','aabbaabb','aabbaabbaa','bb','aabbaabb']
  cases.append((base,[],[],[('中国人',1000)],trace,[beam,.03,3000,2,0,1,2,5,dup,20]))
chars=['的','一','是','中','国','人','中国','人民','𰻞','e\u0301','😀','👩\u200d💻','\ud800']
codes=['a','b','aa','ab','ba','bb','aaa','aab','bba','aaaa',';a','/a','[b']
for _ in range(160):
 entries=[(code,rng.sample(chars,rng.randrange(1,6))) for code in rng.sample(codes,rng.randrange(3,len(codes)+1))]
 qs=[''.join(rng.choice(['aa','ab','bb','ba','a1','a2','b;','b\'']) for _ in range(rng.randrange(1,6))) for _ in range(16)]
 qs+=['a','b','aa','ab','aabb','aaaa']
 options=[rng.choice([1,2,5,30,2000]),rng.choice([0,.03,.2]),rng.choice([0,3000,10000]),rng.choice([.5,2]),rng.randrange(2),rng.randrange(2),rng.choice([0,2]),rng.choice([0,5]),rng.randrange(2),rng.choice([1,5,20])]
 cases.append((entries,rng.sample(chars,3),rng.sample(chars,2),[(rng.choice(chars)+rng.choice(chars),rng.choice([1,1000,1000000000])) for _ in range(3)],qs,options))
lines=[]
for i,(entries,common,white,supplements,qs,options) in enumerate(cases):
 lines+=['B '+str(i),'O '+' '.join(map(str,options)),'C '+' '.join(map(hx,common)),'W '+' '.join(map(hx,white))]
 lines+=['E '+hx(code)+' '+' '.join(map(hx,values)) for code,values in entries]
 lines+=['S '+hx(text)+' '+str(weight) for text,weight in supplements]
 lines+=['Q '+hx(q)+' '+hx(['','的','中国','不存在'][n%4]) for n,q in enumerate(qs)]+['X']
def compare(a,b,path=''):
 if isinstance(a,float) or isinstance(b,float):assert math.isclose(a,b,rel_tol=2e-13,abs_tol=2e-12),(path,a,b)
 elif isinstance(a,dict):
  assert a.keys()==b.keys(),(path,a,b)
  for k in a:compare(a[k],b[k],path+'/'+k)
 elif isinstance(a,list):
  assert len(a)==len(b),(path,len(a),len(b))
  for i,(x,y) in enumerate(zip(a,b)):compare(x,y,path+'/'+str(i))
 else:assert a==b,(path,a,b)
model=Path('/mnt/c/Users/yc/Desktop/bime_codex_src_20260513/release_arm64/Models/sentence-ngram-v2.bin')
results={}
with tempfile.TemporaryDirectory(prefix='sentence-decoder-',dir=BUILD) as temp:
 root=Path(temp);fixture=root/'fixtures.txt';fixture.write_text('\n'.join(lines)+'\n')
 for variant in ['neutral/full','ngram/full','neutral/incremental','ngram/incremental']:
  mode,decoding=variant.split('/');incremental='1' if decoding=='incremental' else '0'
  modelarg=win(model) if mode=='ngram' else '-'
  oracle=ROOT/'tools/SentenceOracle/bin/Release/net10.0-windows/SentenceOracle.dll'
  r=run_windows(['/mnt/c/Program Files/dotnet/dotnet.exe',win(oracle),'--decoder',win(fixture),modelarg,incremental],capture_output=True,text=True,timeout=180)
  assert r.returncode==0,r.stderr
  expected=[json.loads(line) for line in r.stdout.splitlines()]
  assert len(expected)==sum(len(c[4]) for c in cases)
  for platform in os.environ.get('SENTENCE_TEST_PLATFORMS','ARM64,x64,Win32').split(','):
   out=root/(platform+'-'+mode+'-'+decoding);out.mkdir();probe=BUILD/'tests'/platform/'sentence_decoder_probe.exe'
   r=run_windows([probe,win(fixture),win(out),modelarg,incremental],capture_output=True,text=True,timeout=180)
   assert r.returncode==0,(platform,r.stderr)
   actual=[json.loads(line) for line in r.stdout.splitlines()]
   try:compare(actual,expected,platform+'/'+variant)
   except AssertionError:
    (BUILD/'sentence-decoder-actual.json').write_text(json.dumps(actual,indent=2))
    (BUILD/'sentence-decoder-expected.json').write_text(json.dumps(expected,indent=2))
    (BUILD/'sentence-decoder-failed-fixtures.txt').write_text(fixture.read_text())
    raise
   results[platform+'/'+variant]={'cases':len(cases),'queries':len(actual),'candidates':sum(len(q.get('candidates',[])) for q in actual),'probe_sha256':hashlib.sha256(probe.read_bytes()).hexdigest()}
report={'status':'passed','platforms':results,'full_decoder':True,'incremental_decoder':True,'early_commit_evidence':True,'runtime_integrated':False,
 'model_sha256':hashlib.sha256(model.read_bytes()).hexdigest(),
 'sources':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['native/SentenceDecoder.cpp','native/SentenceDecoder.h','native/SentenceCharacterRanks.cpp','tests/sentence_decoder_test.py']}}
(BUILD/'sentence-decoder-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
