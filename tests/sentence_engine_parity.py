"""Full frozen original engine versus native, with controlled publication schedules."""
import hashlib,json,os,random,shutil,subprocess,tempfile
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def run(args):
 r=run_windows(args,capture_output=True,text=True,timeout=180)
 assert r.returncode==0,(r.stdout,r.stderr)
 return r.stdout
for manifest in ['upstream-sha256.json','engine-dependencies-sha256.json']:
 data=json.loads((ROOT/'tools/SentenceOracle'/manifest).read_text());files=data.get('files',data)
 for name,expected_hash in files.items():
  assert hashlib.sha256((ROOT/'tools/SentenceOracle/upstream'/name).read_bytes()).hexdigest()==expected_hash,name
platform=os.environ.get('SENTENCE_TRACE_PLATFORM','ARM64')
assert platform in ['ARM64','x64','Win32']
automatic=bool(os.environ.get('SENTENCE_TRACE_AUTO'))
burst=bool(os.environ.get('SENTENCE_TRACE_BURST'))
legacy=bool(os.environ.get('SENTENCE_TRACE_LEGACY'))
retained=int(os.environ.get('SENTENCE_TRACE_RETAIN','3'))
real=bool(os.environ.get('SENTENCE_TRACE_REAL'))
reference=Path('/mnt/c/Users/yc/Desktop/bime_codex_src_20260513/release_arm64/码表/虎整句')
tag=('real-' if real else '')+('burst-' if burst else '')+('legacy-' if legacy else 'async-auto-' if automatic else '')+(f'retain{retained}-' if retained!=3 else '')+(platform+'-' if platform!='ARM64' else '')
keys=[]
def case(name,text,extra=()):
 events=[]
 codes={' ':32,';':186,"'":222,'/':191,'[':219,'`':192,'.':190,',':188,'=':187,'-':189}
 for c in text:events += [dict(vk=codes.get(c,ord(c.upper())),action=a,shift=c.isupper()) for a in ['down','up']]
 for vk,shift in extra:events += [dict(vk=vk,action=a,shift=shift) for a in ['down','up']]
 for i,k in enumerate(events):keys.append(dict(k,reset=i==0,case=name))
for raw in ['aa','aabb','aabbcc','aabb2','aabb;',"aabb'",'aaz','zz','a',';','/','[',';a','/a','[a','`ni']:
 for suffix in [' ','.',',',';',"'",'a ','=2','- ']:case(raw+suffix,raw+suffix)
 for vk in [8,9,13,27,38,40,192,191,219]:case(raw+str(vk),raw,[(vk,False),(32,False)])
for raw in ['aabb'*8,'cc'*15,'aabbcc'*5]:case('long '+raw,raw+' ')
case('tab reverse','aabb',[(9,True),(32,False)])
rng=random.Random(9031)
for i in range(80):case('random'+str(i),''.join(rng.choice('abc;123') for _ in range(rng.randrange(1,15)))+' ')
if real:
 keys.clear()
 # Derive input from the supplied source text, independently of native import.
 # Exercise both short and full codes; the engine decides which are eligible.
 codes={}
 for line in (reference/'虎整句.txt').read_text(encoding='utf-8-sig').splitlines():
  fields=line.split()
  if len(fields)>=2 and len(fields[0])==1 and fields[1].isascii() and fields[1].isalpha():
   codes.setdefault(fields[0],[]).append(fields[1])
 phrases=['中国人民','今天我们一起学习','这个问题需要进一步分析和解决','中文输入法使用起来很方便','请帮我打开这个文件','明天上午我们在学校见面','你好世界','输入完成以后按空格','这是一个测试','人人都有自己的想法','我想修改刚才输入的文字','天气很好我们出去走走']
 for phrase in phrases:
  assert all(c in codes for c in phrase),phrase
  for kind,choose in [('short',lambda a:min(a,key=len)),('full',lambda a:max(a,key=len))]:
   raw=''.join(choose(codes[c]) for c in phrase)
   for suffix in [' ','.',',','2 ','; ',"' "]:case(kind+':'+phrase+suffix,raw+suffix)
   case(kind+':edit:'+phrase,raw,[(8,False)]*4+[(32,False)])
   case(kind+':select:'+phrase,raw,[(9,False),(40,False),(9,True),(32,False)])
   case(kind+':cancel:'+phrase,raw,[(27,False)])
 for raw in [';','/','[',';a','/a','[a','`ni']:
  case('real transition:'+raw,raw+' ')
if burst:
 schedule=random.Random(98121)
 for i,key in enumerate(keys):
  last=i+1==len(keys) or keys[i+1]['reset']
  key['settle']=last or schedule.randrange(5)==0
model=Path('/mnt/c/Users/yc/Desktop/bime_codex_src_20260513/release_arm64/Models/sentence-ngram-v2.bin')
with tempfile.TemporaryDirectory(prefix='sentence-engine-',dir=BUILD) as temp:
 root=Path(temp);(root/'.native-tiger-staging').touch();schema=root/'码表/测试整句';schema.mkdir(parents=True)
 (schema/'词条.txt').write_text('aa 中 100\naa 人 90\nbb 国 100\nbb 华 90\ncc 明 100\nzz 字 100\n; 符 100\n/ 号 100\n[ 括 100\n;a 特 100\n/a 殊 100\n[a 短 100\n',encoding='utf-8')
 (schema/'补充语料.txt').write_text('中国 1000000\n中华 1000\n',encoding='utf-8')
 source_hashes={}
 if real:
  for p in schema.iterdir():p.unlink()
  for p in reference.glob('*.txt'):
   shutil.copyfile(p,schema/p.name)
   source_hashes[p.name]=hashlib.sha256((schema/p.name).read_bytes()).hexdigest()
 pinyin=root/'拼音反查码表';pinyin.mkdir();(pinyin/'py.txt').write_text('ni 你\n',encoding='utf-8')
 config=root/'config.txt';config.write_text('当前码表\t测试整句\n默认中文\t是\n整句自动提前上屏\t'+('是' if automatic else '否')+'\n保留最少编码数量\t'+str(retained)+'\n高频字仅使用最优码组句\t0\n开机自动启动\t否\n',encoding='utf-8-sig')
 if real:config.write_text(config.read_text(encoding='utf-8-sig').replace('高频字仅使用最优码组句\t0','高频字仅使用最优码组句\t1500'),encoding='utf-8-sig')
 (root/'Models').mkdir();os.link(model,root/'Models/sentence-ngram-v2.bin')
 trace=root/'keys.jsonl';trace.write_text('\n'.join(json.dumps(k) for k in keys))
 tsv=root/'keys.tsv';tsv.write_text('\n'.join(' '.join(str(int(v)) for v in [k['reset'],k['vk'],0,k['action']=='down',k.get('shift',False),False,False,False,False,True,1,False,k.get('settle',True)]) for k in keys))
 expected=[json.loads(s) for s in run(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/SentenceEngineOracle/bin/Release/net10.0-windows/SentenceEngineOracle.dll'),win(root),win(trace)]+(['--async'] if automatic or burst else [])).splitlines()]
 user=root/'native';run([BUILD/'tests/ARM64/Tigirl.Import.exe','--schema',win(schema),win(pinyin),win(user),'测试整句','zh-CN'])
 dll=user/'schemas/测试整句/tiger-v2.tcd'
 actual=[json.loads(s) for s in run([BUILD/'tests'/platform/'sentence_trace_probe.exe',win(dll),win(tsv),win(root/'selection.txt'),win(config),win(model)]+(['--legacy-auto' if legacy else '--auto',str(retained)] if automatic else [])).splitlines()]
 assert len(expected)==len(actual)==len(keys)
 assert any(e['snapshot']['Ui']['CompositionState']==5 for e in expected),'Original sentence mode not active'
 failures=[]
 for i,(e,a) in enumerate(zip(expected,actual)):
  r=e['result'];s=e['snapshot'];u=s['Ui']
  normalized=dict(handled=r['Handled'],cancel=r['CancelComposition'],chinese=u['IsChinese'],mode=u['CompositionState'],page=s['CandidatePageIndex'],raw=s['RawInput'],commit=r['TextToOutput'] or '',candidates=u['Candidates'],annotations=u['CandidateAnnotations'],selected=u['SelectedCandidateIndex'],surface=u['InputCode'],committed=s['SentenceCommittedText'],committedRaw=s['SentenceCommittedRawLength'])
  if a!=normalized:failures.append(dict(index=i,key=keys[i],differences={k:dict(expected=v,actual=a.get(k)) for k,v in normalized.items() if a.get(k)!=v}))
 for name,rows in [('oracle',expected),('native',actual)]:
  (BUILD/f'sentence-engine-{tag}{name}.jsonl').write_text('\n'.join(json.dumps(r,ensure_ascii=False) for r in rows)+'\n')
 report_name='sentence-engine-'+tag+'parity.json'
 report=dict(status='failed' if failures else 'passed',scope='Full original engine; '+('controlled deferred-worker bursts' if burst else 'paced asynchronous auto commit' if automatic else 'synchronous auto off')+', '+platform+' native',events=len(keys),mismatches=len(failures),failures=failures,installed=False)
 report['automatic_commit_events']=sum(bool(e['result']['TextToOutput']) and e['snapshot']['SentenceCommittedRawLength']>0 for e in expected)
 report['minimum_retained_raw']=retained
 report['platform']=platform
 report['real_scheme']=real
 report['source_tables']=source_hashes
 report['common_character_limit']=1500 if real else 0
 (BUILD/f'sentence-engine-{tag}keys.jsonl').write_text('\n'.join(json.dumps(k,ensure_ascii=False) for k in keys)+'\n')
 report['controlled_bursts']=burst
 report['deferred_events']=sum(not k.get('settle',True) for k in keys)
 if automatic:assert report['automatic_commit_events']>0
 report['sources']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'native/Engine.cpp',ROOT/'native/SentenceSession.cpp',ROOT/'native/SentenceDecoder.cpp',ROOT/'native/SentenceDecoder.h',ROOT/'tests/sentence_trace_probe.cpp',ROOT/'tests/sentence_engine_parity.py',ROOT/'tools/SentenceEngineOracle/Driver.cs']}
 report['binaries']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'tools/SentenceEngineOracle/bin/Release/net10.0-windows/SentenceEngineOracle.dll',BUILD/'tests'/platform/'sentence_trace_probe.exe']}
 (BUILD/report_name).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(dict(events=len(keys),mismatches=len(failures),first=failures[:8]),ensure_ascii=False,indent=2))
 raise SystemExit(bool(failures))
