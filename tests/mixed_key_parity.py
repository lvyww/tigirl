"""Original-Core differential key traces under non-default ordinary settings."""
import argparse
import json
import hashlib
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
    for c in text: result+=tap({'`':192,'=':187,' ':32,'.':190}.get(c,ord(c.upper())),shift=c.isupper())
    return result
profiles={
 'mixed':'中英文不限长混合输入 是',
 'mixed_short':'中英文不限长混合输入 是\n最大码长 2\n每页候选个数 1',
 'mixed_clear':'中英文不限长混合输入 是\n回车清屏 是\nTAB清屏 否',
}

keys=[]
def case(name,events):
    for i,event in enumerate(events): keys.append(dict(event,reset=i==0,case=name))
for prefix in ['', 'ab', 'dk', 'abcd', 'qwer', 'zzzz', '`ni', '`zhong', '3.14', 'Abc', 'abcdabcdab', 'zzzzzzzzab', 'abcdABCDzzzzab']:
    for vk in [32,49,50,48,186,222,188,190,191,187,189,219,221,9,33,34,13,8,27,20,160]:
        for shift in [False,True]:
            case(f'{prefix!r} key {vk} shift={shift}',typing(prefix)+tap(vk,shift=shift)+typing('ab '))
    for pages in [[187],[219],[221],[9],[33],[34],[9,9,9]]:
        events=[]
        for vk in pages: events+=tap(vk)
        case(f'{prefix!r} pages {pages}',typing(prefix)+events+typing('2'))
case('shift-toggle',typing('ab')+tap(160)+typing('ab ')+tap(160)+typing('dk '))
case('ctrl-space',typing('ab')+[dict(vk=162,action='down',ctrl=True)]+tap(32,ctrl=True)+[dict(vk=162,action='up')]+typing('ab '))
for prefix in ['abcdabcdab','aaaaZZZZabcd','dkdkab']:
    for count in range(len(prefix)+2):
        case(f'backtrack {prefix} {count}',typing(prefix)+tap(8)*count+typing('abcd '))
    for modifier in [dict(ctrl=True),dict(ctrl=True,shift=True),dict(alt=True)]:
        case(f'adjust {prefix} {modifier}',typing(prefix)+tap(50,**modifier)+typing('abcd '))
    case(f'page seal {prefix}',typing(prefix)+tap(187)*2+typing('abcd '))
    case(f'cancel restart {prefix}',typing(prefix)+tap(27)+typing('abcdabcd '))
trace=BUILD/'mixed-keys.jsonl'
trace.write_text(''.join(json.dumps(k)+'\n' for k in keys),encoding='utf-8')
rows=[' '.join(str(int(x)) for x in [k['reset'],k['vk'],k.get('scan',0),k['action']=='down',k.get('shift',False),k.get('ctrl',False),k.get('alt',False),k.get('win',False),k.get('caps',False),k.get('num',True),k.get('repeat',1),k.get('extended',False)]) for k in keys]
(BUILD/'mixed-keys.tsv').write_text('\n'.join(rows)+'\n')
parser=argparse.ArgumentParser(); parser.add_argument('--windows',action='store_true'); parser.add_argument('--replay',action='store_true'); args=parser.parse_args()
all_failures=[]
for name,config in profiles.items():
    selection=ROOT/'data/staging/自定义选重键.txt'
    settings=BUILD/f'mixed-keys-{name}.txt'; settings.write_text((ROOT/'data/staging/config.txt').read_text(encoding='utf-8-sig')+'\n'+config,encoding='utf-8-sig')
    oracle=BUILD/f'mixed-keys-{name}-oracle.jsonl'
    metadata=BUILD/f'mixed-keys-{name}-oracle-inputs.json'
    fingerprints={str(p.name):hashlib.sha256(p.read_bytes()).hexdigest() for p in [trace,settings,selection]}
    if not args.replay:
        with tempfile.TemporaryDirectory(prefix='settings-key-oracle-',dir=BUILD) as temp, oracle.open('w',encoding='utf-8') as output:
            shutil.copytree(ROOT/'data/staging',temp,dirs_exist_ok=True)
            shutil.copyfile(settings,Path(temp)/'config.txt')
            subprocess.run(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'trace',win(temp),win(trace)],stdout=output,check=True)
        metadata.write_text(json.dumps(fingerprints,indent=2)+'\n')
    else:
        assert json.loads(metadata.read_text())==fingerprints,'Oracle inputs changed; regenerate instead of replaying'
    exe=BUILD/'tests/ARM64/engine_probe.exe' if args.windows else BUILD/'engine_probe'
    paths=[ROOT/'data/tiger-v2.tcd',BUILD/'mixed-keys.tsv',selection,settings]
    actual=subprocess.check_output([str(exe)]+[win(p) if args.windows else str(p) for p in paths],text=True)
    (BUILD/f'mixed-keys-{name}-native.jsonl').write_text(actual,encoding='utf-8')
    expected=[json.loads(x) for x in oracle.read_text(encoding='utf-8-sig').splitlines()]
    values=[json.loads(x) for x in actual.splitlines()]
    assert len(expected)==len(values)==len(keys)
    failures=[]
    for i,(want,got) in enumerate(zip(expected,values)):
        r,s=want['result'],want['snapshot']; ui=s['Ui']
        normalized=dict(handled=r['Handled'],cancel=r['CancelComposition'],chinese=ui['IsChinese'],mode=ui['CompositionState'],page=s['CandidatePageIndex'],raw=s['RawInput'],commit=r['TextToOutput'] or '',candidates=ui['Candidates'],annotations=ui['CandidateAnnotations'])
        normalized['surface']=ui['InputCode'] or ''
        if got!=normalized: failures.append(dict(profile=name,index=i,case=keys[i]['case'],key=keys[i],differences={k:dict(expected=v,actual=got.get(k)) for k,v in normalized.items() if got.get(k)!=v}))
    all_failures+=failures
    print(json.dumps(dict(profile=name,events=len(keys),mismatches=len(failures),first=failures[:2]),ensure_ascii=False),flush=True)
report=dict(events=len(keys)*len(profiles),profiles=len(profiles),mismatches=len(all_failures),failures=all_failures)
suffix='arm64' if args.windows else 'linux'
(BUILD/f'mixed-key-parity-{suffix}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(bool(all_failures))
