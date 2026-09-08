"""Physical-key differential traces with independent custom-selection profiles."""
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
 'generic':'2选 VK_SHIFT\n3选 VK_CONTROL\n4选 VK_MENU\n1选 VK_LWIN\n5选 VK_CAPITAL',
 'sided':'1选 VK_LSHIFT VK_LCONTROL VK_LMENU VK_LWIN\n2选 VK_RSHIFT VK_RCONTROL VK_RMENU VK_RWIN\n3选 VK_SHIFT VK_CONTROL VK_MENU\n4选 VK_CAPITAL',
 'conflicts':'2选 VK_F1 VK_A VK_SPACE VK_RETURN VK_TAB VK_BACK VK_ESCAPE VK_OEM_1 VK_OEM_7\n3选 VK_F2 VK_A\n4选 VK_F1\n1选',
 'bounds':'10选 VK_RSHIFT VK_F1\n2选 VK_CAPITAL\n3选 96\n4选 0x31\n1选 VK_RSHIFT',
}
keys=[]
def case(name,events):
    for i,event in enumerate(events): keys.append(dict(event,reset=i==0,case=name))
physical=[(16,42,False),(16,54,False),(17,0,False),(17,0,True),(18,0,False),(18,0,True)]
physical += [(vk,0,False) for vk in [160,161,162,163,164,165,91,92,20,112,113,65,32,13,9,8,27,186,222,96,49,50]]
for prefix in ['', 'ab', 'dk', '`ni', 'ab=', 'zzzz']:
    for vk,scan,extended in physical:
        mods=dict(scan=scan,extended=extended)
        held=dict(mods,shift=vk in [16,160,161],ctrl=vk in [17,162,163],alt=vk in [18,164,165],win=vk in [91,92],caps=vk==20)
        # Post-release letters detect a spurious toggle or sticky chord state.
        case(f'{prefix!r} {vk}/{scan}/{extended}',typing(prefix)+[dict(vk=vk,action='down',**held),dict(vk=vk,action='up',**mods)]+typing('ab '))
        case(f'repeat {prefix!r} {vk}/{scan}/{extended}',typing(prefix)+[dict(vk=vk,action='down',**held),dict(vk=vk,action='down',repeat=3,**held),dict(vk=vk,action='up',**mods)]+typing('ab '))
trace=BUILD/'selection-keys.jsonl'
trace.write_text(''.join(json.dumps(k)+'\n' for k in keys),encoding='utf-8')
rows=[' '.join(str(int(x)) for x in [k['reset'],k['vk'],k.get('scan',0),k['action']=='down',k.get('shift',False),k.get('ctrl',False),k.get('alt',False),k.get('win',False),k.get('caps',False),k.get('num',True),k.get('repeat',1),k.get('extended',False)]) for k in keys]
(BUILD/'selection-keys.tsv').write_text('\n'.join(rows)+'\n')
parser=argparse.ArgumentParser(); parser.add_argument('--windows',action='store_true'); parser.add_argument('--replay',action='store_true'); args=parser.parse_args()
all_failures=[]
for name,config in profiles.items():
    selection=BUILD/f'selection-keys-{name}.txt'; selection.write_text(config,encoding='utf-8-sig')
    oracle=BUILD/f'selection-keys-{name}-oracle.jsonl'
    if not args.replay:
        with tempfile.TemporaryDirectory(prefix='selection-key-oracle-',dir=BUILD) as temp, oracle.open('w',encoding='utf-8') as output:
            shutil.copytree(ROOT/'data/staging',temp,dirs_exist_ok=True)
            shutil.copyfile(selection,Path(temp)/'自定义选重键.txt')
            subprocess.run(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'trace',win(temp),win(trace)],stdout=output,check=True)
    exe=BUILD/'tests/ARM64/engine_probe.exe' if args.windows else BUILD/'engine_probe'
    paths=[ROOT/'data/tiger-v2.tcd',BUILD/'selection-keys.tsv',selection]
    actual=subprocess.check_output([str(exe)]+[win(p) if args.windows else str(p) for p in paths],text=True)
    (BUILD/f'selection-keys-{name}-native.jsonl').write_text(actual,encoding='utf-8')
    expected=[json.loads(x) for x in oracle.read_text(encoding='utf-8-sig').splitlines()]
    values=[json.loads(x) for x in actual.splitlines()]
    assert len(expected)==len(values)==len(keys)
    failures=[]
    for i,(want,got) in enumerate(zip(expected,values)):
        r,s=want['result'],want['snapshot']; ui=s['Ui']
        normalized=dict(handled=r['Handled'],cancel=r['CancelComposition'],chinese=ui['IsChinese'],mode=ui['CompositionState'],page=s['CandidatePageIndex'],raw=s['RawInput'],commit=r['TextToOutput'] or '',candidates=ui['Candidates'],annotations=ui['CandidateAnnotations'])
        if got!=normalized: failures.append(dict(profile=name,index=i,case=keys[i]['case'],key=keys[i],differences={k:dict(expected=v,actual=got.get(k)) for k,v in normalized.items() if got.get(k)!=v}))
    all_failures+=failures
    print(json.dumps(dict(profile=name,events=len(keys),mismatches=len(failures),first=failures[:2]),ensure_ascii=False),flush=True)
report=dict(events=len(keys)*len(profiles),profiles=len(profiles),mismatches=len(all_failures),failures=all_failures)
suffix='arm64' if args.windows else 'linux'
(BUILD/f'selection-key-parity-{suffix}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(bool(all_failures))
