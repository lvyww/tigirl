"""Real folder importer -> immutable sentence sidecars -> original-model decoder."""
import hashlib,json,math,os,random,subprocess,tempfile
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';TOOLS=BUILD/'tests/ARM64'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def run(args,success=True):
 r=run_windows(args,capture_output=True,text=True,timeout=90)
 assert (r.returncode==0)==success,(args,r.stdout,r.stderr)
 return r
model=ROOT/'data/Models/sentence-fivegram-mobile.bin'
results={}
with tempfile.TemporaryDirectory(prefix='sentence-resources-',dir=BUILD) as temp:
 root=Path(temp);schema=root/'测试整句';schema.mkdir();pinyin=root/'无拼音';user=root/'用户';user.mkdir()
 corpus=schema/'补充语料.txt'
 rows=['默认','重复 2000','重复 1000','零 0','负 -1','正 +12','溢出 9223372036854775808','极限 9223372036854775807',r'字\#号 999 # 注释',r'双\\#忽略 1','多 列 10','空白\u00a0 7','制表\t15','空字符 100\0','阿拉伯 ١٢','全角 １２']
 rng=random.Random(4242)
 for i in range(500):rows.append('词'+str(i)+' '+rng.choice(['1000','+42','-3','0','1000000001','9223372036854775807','9223372036854775808','1x','12\0','12\u00a0','\u00a012','\v12']))
 corpus.write_text('\r\n'.join(rows),encoding='utf-8-sig')
 oracle=ROOT/'tools/SentenceOracle/bin/Release/net10.0-windows/SentenceOracle.dll'
 expected=json.loads(run(['/mnt/c/Program Files/dotnet/dotnet.exe',win(oracle),'--supplement-file',win(schema)]).stdout)
 for platform in ['ARM64','x64','Win32']:
  probe=BUILD/'tests'/platform/'sentence_resources_probe.exe'
  actual=json.loads(run([probe,'--supplement',win(corpus)]).stdout)
  assert len(actual)==len(expected),(platform,len(actual),len(expected))
  for a,b in zip(actual,expected):
   assert a['text']==b['text'] and a['weight']==b['weight'],(platform,a,b)
   assert math.isclose(a['reward'],b['reward'],abs_tol=1e-14),(platform,a,b)
  results[platform]={'parser_entries':len(actual),'sha256':hashlib.sha256(probe.read_bytes()).hexdigest()}
 corpus.write_text('中国 1000000\n中华 1000\n',encoding='utf-8')
 (schema/'词条.txt').write_text('aa 显示甲=>中 100\naa 显示乙=>中 90\naa 人 80\nbb 国 100\nbb 华 90\n',encoding='utf-8')
 def ensure(success=True):return run([TOOLS/'Tigirl.Import.exe','--ensure',win(schema),win(pinyin),win(user),'测试整句','zh-CN'],success)
 def selected():
  gen=(user/'schemas/测试整句/current.txt').read_text(encoding='utf-8-sig').strip().split('\t')[1]
  return user/'schemas/测试整句/generations'/gen/'tiger-v2.tcd'
 ensure();first=selected();lexicon=Path(str(first)+'.sentence.tcd');supplement=Path(str(first)+'.supplement.tcd')
 assert first.exists() and lexicon.exists() and supplement.exists()
 original_bytes={p:p.read_bytes() for p in [first,lexicon,supplement]}
 for platform in results:
  probe=BUILD/'tests'/platform/'sentence_resources_probe.exe'
  actual=json.loads(run([probe,win(first),win(model)]).stdout)
  assert actual['aa']==['4e2d','4eba'],actual
  assert actual['decoded'][0]=='4e2d56fd',actual
  assert actual['mapped_bytes']==model.stat().st_size
  results[platform]['resource_decode']=actual
  run([probe,win(first),win(root/'missing-model.bin')],False)
 assert json.loads(ensure().stdout)['reused'] and selected()==first
 # Corrupt a generated companion: ensure must rebuild, never reuse it.
 lexicon.write_bytes(b'corrupt');ensure();second=selected();assert second!=first
 assert first.read_bytes()==original_bytes[first] and supplement.read_bytes()==original_bytes[supplement]
 # Missing supplement likewise invalidates an otherwise valid generation.
 old_supplement=Path(str(second)+'.supplement.tcd');old_supplement.rename(Path(str(old_supplement)+'.missing'))
 ensure();third=selected();assert third!=second
 # Same timestamp edits to source supplements change the content fingerprint.
 stamp=corpus.stat();corpus.write_text('中华 1000000\n',encoding='utf-8');os.utime(corpus,ns=(stamp.st_atime_ns,stamp.st_mtime_ns))
 ensure();fourth=selected();assert fourth!=third
 descriptor=(user/'schemas/测试整句/current.txt').read_bytes();schema.rename(root/'临时移走')
 ensure(False);assert (user/'schemas/测试整句/current.txt').read_bytes()==descriptor
report={'status':'passed','platforms':results,'cache_reuse':True,'corrupt_sidecar_rebuild':True,'missing_sidecar_rebuild':True,'same_timestamp_supplement_change':True,'failed_import_preserves_selection':True,'runtime_overlay_integrated':False,'tsf_integrated':False,
 'sources':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['native/SentenceImport.cpp','native/SentenceImport.h','native/SentenceResources.cpp','native/SentenceResources.h','tools/lexicon_import.cpp','tools/SourceFingerprint.h']}}
(BUILD/'sentence-resources-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
