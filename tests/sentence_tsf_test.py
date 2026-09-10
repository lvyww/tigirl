"""Hidden real TSF edit sessions with asynchronously loaded sentence resources."""
import hashlib,json,subprocess,tempfile,os,shutil,struct,zlib
from pathlib import Path
from windows_process import run_windows,PS
from tsf_architectures import ROOT,variants
BUILD=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def quote(s):return "'"+str(s).replace("'","''")+"'"
model=Path('/mnt/c/Users/yc/Desktop/bime_codex_src_20260513/release_arm64/Models/sentence-ngram-v2.bin')
checks=[]
bundled=bool(os.environ.get('SENTENCE_BUNDLED_MODEL'))
model_setting='' if bundled else '整句语言模型\t'+win(model)+'\n'
for arm64x in ([True] if os.environ.get('SENTENCE_ARM64X_ONLY') else [False]):
 if os.environ.get('SENTENCE_X86'):
  dll=Path(os.environ.get('SENTENCE_X86_PACKAGE_DIR',str(BUILD/'Win32/Release')))/'Tigirl.dll';manifest=dll.parent/'NativeTiger.x86.manifest'
  manifest.write_text((ROOT/'tests/Tigirl.Test.manifest').read_text().replace('processorArchitecture="arm64"','processorArchitecture="x86"'))
  hosts=[('Win32',BUILD/'tests/Win32/tsf_host.exe',manifest)]
 else:dll,hosts=variants(arm64x)
 for platform,host,manifest in hosts:
  with tempfile.TemporaryDirectory(prefix='sentence-tsf-',dir=BUILD) as tmp:
   root=Path(tmp);source=root/'source';source.mkdir();user=root/'user';user.mkdir()
   journal_mode=bool(os.environ.get('SENTENCE_JOURNAL'))
   (source/'词条.txt').write_text(('aa 错' if journal_mode else 'aa 中')+' 100\naa 人 90\nbb 国 100\nbb 华 90\n',encoding='utf-8')
   (source/'补充语料.txt').write_text('中国 1000000\n中华 1000\n',encoding='utf-8')
   r=run_windows([BUILD/'tests/ARM64/Tigirl.Import.exe','--schema',win(source),win(root/'pinyin'),win(user),'测试整句','zh-CN'],capture_output=True,text=True,timeout=60);assert r.returncode==0,r.stderr
   if journal_mode:
    if bundled:
     assert (dll.parent/'Tigirl.Import.exe').read_bytes()==(BUILD/'tests/ARM64/Tigirl.Import.exe').read_bytes(),'Packaged importer is stale'
    else:shutil.copy2(BUILD/'tests/ARM64/Tigirl.Import.exe',dll.parent/'Tigirl.Import.exe')
    def record(kind,text):
     c='aa'.encode('utf-16-le');t=text.encode('utf-16-le');payload=struct.pack('<III',kind,len(c)//2,len(t)//2)+c+t
     return struct.pack('<II',len(payload),zlib.crc32(payload))+payload
    (user/'schemas/测试整句/user.tcu').write_bytes(b'TIGERU01'+record(1,'错')+record(2,'中'))
    (user/'.sentence-journal-test').touch()
    journal=user/'schemas/测试整句/user.tcu';initial=journal.read_bytes()
    c='bb'.encode('utf-16-le');t='国'.encode('utf-16-le');payload=struct.pack('<III',1,2,1)+c+t
    journal.write_bytes(initial+struct.pack('<II',len(payload),zlib.crc32(payload))+payload)
    base=user/'schemas/测试整句/tiger-v2.tcd'
    revision=run_windows([BUILD/'tests/ARM64/Tigirl.Import.exe','--sentence-revision',win(base),win(journal)],capture_output=True,text=True,timeout=60)
    assert revision.returncode==0,revision.stderr
    future=json.loads(revision.stdout)['revision'];journal.write_bytes(initial)
    (user/'.sentence-next-cache-lock').write_text(win(user/'cache/sentence'/future/'.import.lock'),encoding='utf-8')
    # Prepare a replacement base, but let the running host publish its descriptor.
    (source/'词条.txt').write_text('aa 错 100\naa 人 90\nbb 国 100\nbb 州 90\n',encoding='utf-8')
    updated=run_windows([BUILD/'tests/ARM64/Tigirl.Import.exe','--update',win(source),win(root/'pinyin'),win(user),'测试整句','zh-CN'],capture_output=True,text=True,timeout=60)
    assert updated.returncode==0,updated.stderr
    (user/'schemas/测试整句/current.txt').rename(user/'.sentence-next-base')
    other=root/'other';other.mkdir();(other/'词条.txt').write_text('aa 中 100\nbb 文 100\ncc 明 100\n',encoding='utf-8')
    imported=run_windows([BUILD/'tests/ARM64/Tigirl.Import.exe','--schema',win(other),win(root/'pinyin'),win(user),'另一整句','zh-CN'],capture_output=True,text=True,timeout=60)
    assert imported.returncode==0,imported.stderr
    (user/'.sentence-next-config').write_text('当前码表\t另一整句\n'+model_setting+'高频字仅使用最优码组句\t0\n整句自动提前上屏\t是\n',encoding='utf-8-sig')
   if journal_mode:(user/'.sentence-timing-config').write_bytes((user/'.sentence-next-config').read_bytes()+'保留最少编码数量\t3\n'.encode('utf-8'))
   (user/'config.txt').write_text('当前码表\t测试整句\n'+model_setting+'高频字仅使用最优码组句\t0\n',encoding='utf-8-sig')
   command='$env:NATIVE_TIGER_USER_ROOT='+quote(win(user))+'; & '+' '.join(quote(s) for s in [win(host),win(dll),'-',win(manifest),'--sentence'])+' | Out-String -Stream; exit $LASTEXITCODE'
   r=subprocess.run(['/init',PS,'-NoProfile','-Command',command],capture_output=True,text=True,timeout=45)
   result={'platform':platform,'journal_aware':journal_mode,'dll_sha256':hashlib.sha256(dll.read_bytes()).hexdigest(),'host_sha256':hashlib.sha256(host.read_bytes()).hexdigest(),'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr}
   checks.append(result)
   if bundled:
    packaged_model=dll.parent/'Models/sentence-ngram-v2.bin'
    result['bundled_model_sha256']=hashlib.sha256(packaged_model.read_bytes()).hexdigest()
    assert result['bundled_model_sha256']==hashlib.sha256(model.read_bytes()).hexdigest()
   name='sentence-tsf-x86-validation.json' if os.environ.get('SENTENCE_X86') else 'sentence-tsf-arm64x-validation.json' if arm64x else 'sentence-tsf-validation.json'
   if journal_mode:name=name.replace('-validation','-journal-validation')
   if bundled:name=name.replace('-validation','-bundled-validation')
   (BUILD/name).write_text(json.dumps({'checks':checks,'installed':False,'physical_input':False},indent=2)+'\n')
   assert r.returncode==0,result
   outcome=json.loads(r.stdout)
   for field in ['computed_result_deferred_positive','computed_result_stale_context','computed_result_stale_thread_focus']:
    assert outcome.get(field) is True,(field,result)
   if journal_mode:assert list((user/'cache/sentence').glob('*/current.txt')),'TSF did not prepare shared journal cache'
print(json.dumps({'status':'passed','checks':checks}))
