"""Add-word shortcut traces; the oracle reports actions without opening dialogs."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
def win(path): return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
def tap(vk,**mod): return [dict(vk=vk,action=a,**mod) for a in ('down','up')]
def typing(text):
    result=[]
    for c in text: result+=tap({'`':192,'=':187,' ':32}.get(c,ord(c.upper())))
    return result
profiles={
 'default':'',
 'custom':'手动加词快捷键 Ctrl+VK_E',
 'alt_shift':'手动加词快捷键 Alt+Shift+VK_F12',
 'hex_caps':'手动加词快捷键 Ctrl+0x14',
 'disabled':'Ctrl+等号手动加词 否',
 'reserved':'手动加词快捷键 Ctrl+VK_1',
 'native_reserved':'手动加词快捷键 Alt+VK_OEM_5',
 'native_unreserved':'手动加词快捷键 Alt+VK_OEM_5\nAlt+\\启用或禁用外挂版 否',
 'conflict':'手动加词快捷键 Ctrl+VK_E\nCtrl+m切换最近码表 是\n切换最近码表快捷键 Ctrl+VK_E',
 'invalid':'手动加词快捷键 Ctrl+Ctrl+VK_E',
 'hex_space':'手动加词快捷键 Ctrl+0x 45',
}
keys=[]
def case(name,events):
    for i,event in enumerate(events): keys.append(dict(event,reset=i==0,case=name))
for prefix in ['', 'ab', '`ni', 'zzzz']:
 for vk in [187,69,123,20,49,220]:
  for mods in [dict(ctrl=True),dict(alt=True),dict(ctrl=True,shift=True),dict(alt=True,shift=True),dict(ctrl=True,win=True)]:
   down=dict(vk=vk,action='down',**mods)
   events=[down,dict(down,repeat=3),dict(down,action='up'),down,dict(down,action='up')]
   case(f'{prefix!r} {vk} {mods}',typing(prefix)+events+typing('ab '))
trace=BUILD/'add-word-keys.jsonl'
trace.write_text(''.join(json.dumps(k)+'\n' for k in keys),encoding='utf-8')
rows=[' '.join(str(int(x)) for x in [k['reset'],k['vk'],k.get('scan',0),k['action']=='down',k.get('shift',False),k.get('ctrl',False),k.get('alt',False),k.get('win',False),k.get('caps',False),k.get('num',True),k.get('repeat',1),k.get('extended',False)]) for k in keys]
(BUILD/'add-word-keys.tsv').write_text('\n'.join(rows)+'\n')
parser=argparse.ArgumentParser(); parser.add_argument('--windows',action='store_true'); parser.add_argument('--replay',action='store_true'); parser.add_argument('--profile',choices=list(profiles)); args=parser.parse_args()
if args.profile: profiles={args.profile:profiles[args.profile]}
all_failures=[]
for name,config in profiles.items():
    selection=BUILD/f'add-word-keys-{name}.txt'; selection.write_text(config,encoding='utf-8-sig')
    oracle=BUILD/f'add-word-keys-{name}-oracle.jsonl'
    if not args.replay:
        with tempfile.TemporaryDirectory(prefix='add-word-key-oracle-',dir=BUILD) as temp, oracle.open('w',encoding='utf-8') as output:
            shutil.copytree(ROOT/'data/staging',temp,dirs_exist_ok=True)
            shutil.copyfile(selection,Path(temp)/'config.txt')
            subprocess.run(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'trace',win(temp),win(trace)],stdout=output,check=True)
    exe=BUILD/'tests/ARM64/engine_probe.exe' if args.windows else BUILD/'engine_probe'
    empty=BUILD/'add-word-empty-selection.txt'; empty.write_text('')
    paths=[ROOT/'data/tiger-v2.tcd',BUILD/'add-word-keys.tsv',empty,selection]
    actual=subprocess.check_output([str(exe)]+[win(p) if args.windows else str(p) for p in paths],text=True)
    (BUILD/f'add-word-keys-{name}-native.jsonl').write_text(actual,encoding='utf-8')
    expected=[json.loads(x) for x in oracle.read_text(encoding='utf-8-sig').splitlines()]
    values=[json.loads(x) for x in actual.splitlines()]
    assert len(expected)==len(values)==len(keys)
    failures=[]
    for i,(want,got) in enumerate(zip(expected,values)):
        r,s=want['result'],want['snapshot']; ui=s['Ui']
        normalized=dict(handled=r['Handled'],cancel=r['CancelComposition'],chinese=ui['IsChinese'],mode=ui['CompositionState'],page=s['CandidatePageIndex'],raw=s['RawInput'],commit=r['TextToOutput'] or '',candidates=ui['Candidates'],annotations=ui['CandidateAnnotations'])
        if r['OpenAddCiWindow']: normalized['openAddWord']=True
        if got!=normalized: failures.append(dict(profile=name,index=i,case=keys[i]['case'],key=keys[i],differences={k:dict(expected=v,actual=got.get(k)) for k,v in normalized.items() if got.get(k)!=v}))
    all_failures+=failures
    print(json.dumps(dict(profile=name,events=len(keys),mismatches=len(failures),first=failures[:2]),ensure_ascii=False),flush=True)
report=dict(events=len(keys)*len(profiles),profiles=len(profiles),mismatches=len(all_failures),failures=all_failures)
suffix='arm64' if args.windows else 'linux'
(BUILD/f'add-word-key-parity-{suffix}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(bool(all_failures))
