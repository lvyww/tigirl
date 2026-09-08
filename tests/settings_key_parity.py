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
PS='/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
PS_COMMAND=[PS] if Path('/proc/sys/fs/binfmt_misc/WSLInterop').exists() else ['/init',PS]
def windows_command(executable, arguments):
    def quote(value): return "'" + str(value).replace("'", "''") + "'"
    command='& '+quote(executable)+' '+ ' '.join(quote(x) for x in arguments)+'; exit $LASTEXITCODE'
    return [*PS_COMMAND,'-NoProfile','-Command',command]

def win(path): return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
def tap(vk,**mod): return [dict(vk=vk,action=a,**mod) for a in ('down','up')]
def typing(text):
    result=[]
    for c in text: result+=tap({'`':192,'=':187,' ':32,'.':190}.get(c,ord(c.upper())),shift=c.isupper())
    return result
profiles={
 'english':'默认中文 否\nshift切换中英文 否\nCtrl+空格切换中英文 否',
 'symbols':'中文状态下使用英文标点 是\n/输出顿号 否\n分号次选 否\n引号三选 否',
 'clear':'空码自动清屏 否\n回车清屏 是\nTAB清屏 否\n最大码长无重自动上屏 否',
 'compact':'最大码长 2\n每页候选个数 1\n翻页键 [ ]\n显示注释 否\n显示拆分 是',
 'wide':'最大码长 16\n每页候选个数 10\n翻页键 Shift Tab/Tab\n显示拆分 是',
 'page':'最大码长 1\n每页候选个数 3\n翻页键 PageUp/PageDown\n`键拼音反查 否',
}
keys=[]
def case(name,events):
    for i,event in enumerate(events): keys.append(dict(event,reset=i==0,case=name))
for prefix in ['', 'ab', 'dk', 'abcd', 'qwer', 'zzzz', '`ni', '`zhong', '3.14', 'Abc']:
    for vk in [32,49,50,48,186,222,188,190,191,187,189,219,221,9,33,34,13,8,27,20,160]:
        for shift in [False,True]:
            case(f'{prefix!r} key {vk} shift={shift}',typing(prefix)+tap(vk,shift=shift)+typing('ab '))
    for pages in [[187],[219],[221],[9],[33],[34],[9,9,9]]:
        events=[]
        for vk in pages: events+=tap(vk)
        case(f'{prefix!r} pages {pages}',typing(prefix)+events+typing('2'))
case('shift-toggle',typing('ab')+tap(160)+typing('ab ')+tap(160)+typing('dk '))
case('ctrl-space',typing('ab')+[dict(vk=162,action='down',ctrl=True)]+tap(32,ctrl=True)+[dict(vk=162,action='up')]+typing('ab '))
trace=BUILD/'settings-keys.jsonl'
trace.write_text(''.join(json.dumps(k)+'\n' for k in keys),encoding='utf-8')
rows=[' '.join(str(int(x)) for x in [k['reset'],k['vk'],k.get('scan',0),k['action']=='down',k.get('shift',False),k.get('ctrl',False),k.get('alt',False),k.get('win',False),k.get('caps',False),k.get('num',True),k.get('repeat',1),k.get('extended',False)]) for k in keys]
(BUILD/'settings-keys.tsv').write_text('\n'.join(rows)+'\n')
parser=argparse.ArgumentParser(); parser.add_argument('--windows',action='store_true'); parser.add_argument('--replay',action='store_true'); args=parser.parse_args()
all_failures=[]
oracle_hashes={}
oracle_root=ROOT/'tools/ReferenceOracle'
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
for directory, manifest in [('upstream','upstream-sha256.json'),('overlay','overlay-sha256.json')]:
    for name, expected_hash in json.loads((oracle_root/manifest).read_text()).items():
        assert digest(oracle_root/directory/name)==expected_hash, f'Oracle snapshot changed: {directory}/{name}'
provenance={
    'staging':{str(p.relative_to(ROOT/'data/staging')):digest(p)
        for p in sorted((ROOT/'data/staging').rglob('*')) if p.is_file()},
    'oracle_sources':{str(p.relative_to(oracle_root)):digest(p)
        for p in sorted(oracle_root.rglob('*')) if p.is_file()
        and p.relative_to(oracle_root).parts[0] not in ('bin','obj')},
    'oracle_runtime':{p.name:digest(p)
        for p in sorted((oracle_root/'bin/Release/net10.0-windows').glob('*'))
        if p.is_file() and p.suffix in ('.dll','.json')},
    'dictionary_sha256':digest(ROOT/'data/tiger-v2.tcd'),
}
for name,config in profiles.items():
    selection=ROOT/'data/staging/自定义选重键.txt'
    settings=BUILD/f'settings-keys-{name}.txt'; settings.write_text((ROOT/'data/staging/config.txt').read_text(encoding='utf-8-sig')+'\n'+config,encoding='utf-8-sig')
    oracle=BUILD/f'settings-keys-{name}-oracle.jsonl'
    metadata=BUILD/f'settings-keys-{name}-oracle-inputs.json'
    fingerprints=dict(inputs={str(p.name):digest(p) for p in [trace,settings,selection]}, provenance=provenance)
    if not args.replay:
        with tempfile.TemporaryDirectory(prefix='settings-key-oracle-',dir=BUILD) as temp, oracle.open('w',encoding='utf-8') as output:
            shutil.copytree(ROOT/'data/staging',temp,dirs_exist_ok=True)
            shutil.copyfile(settings,Path(temp)/'config.txt')
            subprocess.run(windows_command(r'C:\Program Files\dotnet\dotnet.exe',[win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'trace',win(temp),win(trace)]),stdout=output,check=True)
        metadata.write_text(json.dumps(fingerprints,indent=2)+'\n')
    else:
        assert json.loads(metadata.read_text())==fingerprints,'Oracle inputs changed; regenerate instead of replaying'
    exe=BUILD/'tests/ARM64/engine_probe.exe' if args.windows else BUILD/'engine_probe'
    paths=[ROOT/'data/tiger-v2.tcd',BUILD/'settings-keys.tsv',selection,settings]
    command=windows_command(win(exe),[win(p) for p in paths]) if args.windows else [str(exe)]+[str(p) for p in paths]
    actual=subprocess.check_output(command,text=True)
    (BUILD/f'settings-keys-{name}-native.jsonl').write_text(actual,encoding='utf-8')
    oracle_hashes[oracle.name]=hashlib.sha256(oracle.read_bytes()).hexdigest()
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
report.update(probe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
              dictionary_sha256=hashlib.sha256((ROOT/'data/tiger-v2.tcd').read_bytes()).hexdigest(),
              oracle_outputs_sha256=oracle_hashes, oracle_replayed=args.replay, oracle_provenance=provenance,
              physical_input_tested=False, tsf_dll_tested=False,
              source_sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in sorted([* (ROOT/'native').glob('*.cpp'), * (ROOT/'native').glob('*.h'),
                      ROOT/'tests/engine_probe.cpp', ROOT/'tests/EngineProbe.vcxproj',
                      ROOT/'tests/DictionaryProbe.vcxproj', Path(__file__)])},
              trace_sha256=hashlib.sha256(trace.read_bytes()).hexdigest())
suffix='arm64' if args.windows else 'linux'
(BUILD/f'settings-key-parity-{suffix}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(bool(all_failures))
