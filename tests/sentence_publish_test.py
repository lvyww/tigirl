"""Actual importer publishes journal-aware immutable sentence generations."""
import concurrent.futures,hashlib,json,struct,subprocess,tempfile,zlib
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/'build/tests/ARM64/Tigirl.Import.exe'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
def invoke(*args):return run_windows([EXE,*args],capture_output=True,text=True,timeout=60)
def record(kind,code,text):
 c=code.encode('utf-16-le');t=text.encode('utf-16-le')
 p=struct.pack('<III',kind,len(c)//2,len(t)//2)+c+t
 return struct.pack('<II',len(p),zlib.crc32(p))+p
def section(path,number):
 data=path.read_bytes();assert data[:8]==b'TIGERD02'
 count,offset=struct.unpack_from('<IQ',data,36+(number-1)*16)
 def string(at):
  start,size=struct.unpack_from('<QI',data,at)
  return data[start:start+size*2].decode('utf-16-le')
 result={}
 for i in range(count):
  at=offset+i*32;values,n=struct.unpack_from('<QI',data,at+16)
  result[string(at)]=[string(values+j*16) for j in range(n)]
 return result
with tempfile.TemporaryDirectory(prefix='sentence-publish-',dir=ROOT/'build') as tmp:
 root=Path(tmp);source=root/'source';source.mkdir()
 (source/'词条.txt').write_text('zz 中 100\nee 人 100\naa 中 100\nbb 国 100\n',encoding='utf-8')
 base=root/'base.tcd';journal=root/'user.tcu'
 r=invoke(win(source),win(root/'pinyin'),win(base),'zh-CN');assert r.returncode==0,(r.stdout,r.stderr)
 original={p:p.read_bytes() for p in [base,Path(str(base)+'.sentence.tcd'),Path(str(base)+'.supplement.tcd')]}
 changes=[(1,'zz','中'),(0,'ee','中'),(2,'aa','人'),
          (0,'yy','别名一\x1e新'),(0,'qq','新'),(0,'yy','别名二\x1e新')]
 journal.write_bytes(b'TIGERU01'+b''.join(record(*c) for c in changes));before=journal.read_bytes()
 first=root/'first.tcd'
 def revision():
  r=invoke('--sentence-revision',win(base),win(journal));assert r.returncode==0,r.stderr
  return json.loads(r.stdout)['revision']
 initial_revision=revision()
 # Independent SHA-256 implementation verifies the cross-process wire identity.
 digest=hashlib.sha256()
 def number(n):digest.update(struct.pack('<Q',n))
 def text(s):number(len(s.encode('utf-16-le'))//2);digest.update(s.encode('utf-16-le'))
 text('native-sentence-revision-1')
 for p in [base,Path(str(base)+'.sentence.tcd')]:
  data=p.read_bytes();number(len(data));digest.update(data)
 overlay={'aa':['人','中'],'ee':['人','中'],'qq':['新'],'yy':['别名二\x1e新'],'zz':[]}
 number(len(overlay))
 for code,values in sorted(overlay.items()):
  text(code);number(len(values))
  for value in values:text(value)
 number(2);text('yy');text('qq')
 assert initial_revision==digest.hexdigest()
 def rebuild(output,expected=None):return invoke('--rebuild-sentence',win(base),win(journal),win(output),*([expected] if expected else []))
 r=rebuild(first,initial_revision);assert r.returncode==0,(r.stdout,r.stderr)
 assert json.loads(r.stdout)=={'status':'sentence-rebuilt','edited_codes':5,'source_codes':6,'revision':initial_revision}
 assert section(first,4)['_sentence_revision']==[initial_revision]
 values=section(first,1);primary=section(first,5)
 assert values.get('ZZ',[])==[] and values['YY']==['新']
 assert primary['中']==['EE'] and primary['人']==['EE'] and primary['新']==['YY'],primary
 assert list(section(first,3).values())==[[s] for s in ['zz','ee','aa','bb','yy','qq']]
 frozen=first.read_bytes()
 assert journal.read_bytes()==before and all(p.read_bytes()==v for p,v in original.items())
 # Existing destinations cannot be replaced, including the ordinary base.
 assert rebuild(first).returncode!=0 and first.read_bytes()==frozen
 assert rebuild(base).returncode!=0 and base.read_bytes()==original[base]
 # A changed journal yields a new immutable generation, retaining the old one.
 journal.write_bytes(before+record(1,'ee','中'));second=root/'second.tcd'
 stale=root/'stale.tcd';r=rebuild(stale,initial_revision)
 assert r.returncode!=0 and 'revision changed' in r.stderr and not stale.exists()
 next_revision=revision();assert next_revision!=initial_revision
 assert rebuild(second).returncode==0 and section(second,5)['中']==['AA']
 assert section(second,4)['_sentence_revision']==[next_revision]
 # Redundant journal history with the same effective snapshot keeps its identity.
 journal.write_bytes(journal.read_bytes()+record(0,'yy','别名二\x1e新'))
 assert revision()==next_revision
 assert first.read_bytes()==frozen
 # Two independent helper processes compete for one new output: exactly one wins.
 race=root/'race.tcd'
 with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
  outcomes=list(pool.map(lambda _:rebuild(race),range(2)))
 assert sorted(r.returncode==0 for r in outcomes)==[False,True]
 assert race.read_bytes()==second.read_bytes()
 # Shared cache requests for one revision serialize the expensive rebuild.
 cache=root/'cache'
 def ensure(expected=next_revision):return invoke('--ensure-sentence',win(base),win(journal),win(cache),expected)
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
  requests=list(pool.map(lambda _:ensure(),range(4)))
 assert all(r.returncode==0 for r in requests),[(r.stdout,r.stderr) for r in requests]
 ready=[json.loads(r.stdout) for r in requests]
 assert sum(not r['reused'] for r in ready)==1 and len({r['generation'] for r in ready})==1
 generation=ready[0]['generation'];directory=cache/next_revision
 current=directory/'current.txt';descriptor=current.read_bytes()
 cached=directory/'generations'/generation/'tiger-v2.tcd'
 assert cached.read_bytes()==second.read_bytes()
 assert json.loads(ensure().stdout)['reused'] and current.read_bytes()==descriptor
 assert len(list((directory/'generations').iterdir()))==1
 # Damaged cache files are retained and a distinct generation is selected.
 cached.write_bytes(b'damaged-cache-fixture')
 repaired=json.loads(ensure().stdout);assert not repaired['reused'] and repaired['generation']!=generation
 replacement=directory/'generations'/repaired['generation']/'tiger-v2.tcd'
 assert replacement.read_bytes()==second.read_bytes() and cached.read_bytes()==b'damaged-cache-fixture'
 descriptor=current.read_bytes()
 assert ensure(initial_revision).returncode!=0 and current.read_bytes()==descriptor
 assert not (cache/initial_revision).exists()
 # Actual readers in all host architectures adopt only the matching snapshot.
 model=Path('/mnt/c/Users/yc/Desktop/bime_codex_src_20260513/release_arm64/Models/sentence-ngram-v2.bin')
 for platform in ['ARM64','x64','Win32']:
  probe=ROOT/'build/tests'/platform/'sentence_resources_probe.exe'
  def load(expected):return run_windows([probe,win(base),win(model),win(replacement),expected],capture_output=True,text=True,timeout=60)
  r=load(next_revision);assert r.returncode==0,(platform,r.stdout,r.stderr)
  assert json.loads(r.stdout)['aa']==['4eba','4e2d']
  assert load(initial_revision).returncode!=0 and load('').returncode!=0
 # Corrupt journals must fail before publishing anything or changing the input.
 broken=bytearray(journal.read_bytes());broken[-1]^=1;journal.write_bytes(broken)
 rejected=root/'rejected.tcd';r=rebuild(rejected)
 assert r.returncode!=0 and not rejected.exists() and journal.read_bytes()==broken
 assert all(p.read_bytes()==v for p,v in original.items())
report={'status':'passed','platform':'Windows ARM64','live_journal_rebuild':True,
 'existing_output_preserved':True,'concurrent_publication':True,'corruption_rejected':True,
 'base_files_unchanged':True,'stale_revision_rejected':True,'effective_revision':True,
 'shared_cache_single_rebuild':True,'damaged_cache_new_generation':True,'revision_checked_readers':['ARM64','x64','Win32'],'tsf_integrated':False,'sha256':hashlib.sha256(EXE.read_bytes()).hexdigest(),
 'sources':{f:hashlib.sha256((ROOT/f).read_bytes()).hexdigest() for f in
 ['tools/lexicon_import.cpp','native/SentenceRebuild.cpp','native/SentenceRevision.h','native/UserStore.cpp','native/LexiconPublish.cpp']}}
(ROOT/'build/sentence-publish-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
