"""Compare original PostProcessKey history after real key-engine dispatch."""
import hashlib,json,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
keys=[]
for vk in range(256):
 for shift in [False,True]:
  for caps in [False,True]:
   for modifier in [{},{'ctrl':True},{'alt':True},{'win':True}]:
    for action in ['down','up']:
     keys.append(dict(vk=vk,action=action,reset=action=='down',shift=shift,caps=caps,num=False,**modifier))
for i,vk in enumerate(list(range(65,91))*3+[186,187,188,189,190,191,192,219,220,221,222]+list(range(96,106))+[8]*110):
 for action in ['down','up']: keys.append(dict(vk=vk,action=action,reset=i==0 and action=='down'))
trace=BUILD/'history-keys.jsonl';trace.write_text(''.join(json.dumps(k)+'\n' for k in keys))
rows=[' '.join(str(int(x)) for x in [k['reset'],k['vk'],0,k['action']=='down',k.get('shift',False),k.get('ctrl',False),k.get('alt',False),k.get('win',False),k.get('caps',False),k.get('num',True),1,False]) for k in keys]
tsv=BUILD/'history-keys.tsv';tsv.write_text('\n'.join(rows)+'\n')
reports={arch:dict(events=0,mismatches=0,failures=[]) for arch in ['linux','arm64']}
for profile in ['english','chinese']:
 settings=BUILD/f'history-keys-{profile}.txt'
 settings.write_text('默认中文 '+('是' if profile=='chinese' else '否')+'\nshift切换中英文 否\nCtrl+空格切换中英文 否\n',encoding='utf-8-sig')
 oracle=BUILD/f'history-keys-{profile}-oracle.jsonl'
 with tempfile.TemporaryDirectory(prefix='history-keys-oracle-',dir=BUILD) as tmp,oracle.open('w',encoding='utf-8') as output:
  shutil.copytree(ROOT/'data/staging',tmp,dirs_exist_ok=True)
  shutil.copyfile(settings,Path(tmp)/'config.txt')
  subprocess.run(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'tracehistory',win(tmp),win(trace)],stdout=output,check=True)
 expected=[json.loads(x)['history'] for x in oracle.read_text(encoding='utf-8-sig').splitlines()]
 for arch,exe in [('linux',BUILD/'engine_probe'),('arm64',BUILD/'tests/ARM64/engine_probe.exe')]:
  paths=[ROOT/'data/tiger-v2.tcd',tsv,ROOT/'data/staging/自定义选重键.txt',settings]
  actual=subprocess.check_output([str(exe)]+[win(p) if arch=='arm64' else str(p) for p in paths]+['--history'],text=True)
  actual=[json.loads(x)['history'] for x in actual.splitlines()]
  assert len(expected)==len(actual)==len(keys)
  failures=[dict(profile=profile,index=i,key=keys[i],expected=a,actual=b) for i,(a,b) in enumerate(zip(expected,actual)) if a!=b]
  report=reports[arch];report['events']+=len(keys);report['mismatches']+=len(failures);report['failures']+=failures[:5]
  report['probe_sha256']=hashlib.sha256(exe.read_bytes()).hexdigest()
  print(profile,arch,len(keys),'events',len(failures),'mismatches',failures[:2],flush=True)
for arch,report in reports.items():
 (BUILD/f'history-key-parity-{arch}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
assert not any(r['mismatches'] for r in reports.values())
