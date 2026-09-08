"""Uppercase currency key traces against original Core; excludes timer commits."""
import json,subprocess,tempfile,shutil,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; BUILD=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def tap(vk,**mod):return [dict(vk=vk,action=a,**mod) for a in ['down','up']]
def typing(text):
 result=[]
 oem={'.':190,',':188,';':186,'/':191,' ':32,'!':49,'-':189}
 for c in text:result+=tap(oem.get(c,ord(c.upper())),shift=c.isupper() or c=='!')
 return result
keys=[]
def case(name,events):
 for i,k in enumerate(events):keys.append(dict(k,reset=i==0,case=name))
for amount in ['0','0.005','1.005','1.0049','101000','100001000','100000001','79228162514264337593543950335','79228162514264337593543950336','1,234.56','.12','1.','1..2','1,2,3','1e3','1e-3','NaN','Infinity','abc','']:
 for ending in [32,13,9,27,8,188,190,186]:
  case('S'+amount+f' end {ending}',typing('S'+amount)+tap(ending)+typing('ab '))
 case('S'+amount+' shifted punctuation',typing('S'+amount+'!ab '))
for code in ['S1','S0.1','S1e3','SNaN','D1','D1e3','Abc']:
 case('editing '+code,typing(code)+tap(8)+typing('2.3 '))
 case('toggle '+code,typing(code)+tap(160)+typing('ab '))
trace=BUILD/'currency-keys.jsonl'; trace.write_text(''.join(json.dumps(k)+'\n' for k in keys))
tsv=BUILD/'currency-keys.tsv';tsv.write_text('\n'.join(' '.join(str(int(x)) for x in [k['reset'],k['vk'],0,k['action']=='down',k.get('shift',False),False,False,False,False,True,1,False]) for k in keys)+'\n')
with tempfile.TemporaryDirectory(prefix='currency-keys-',dir=BUILD) as temp:
 shutil.copytree(ROOT/'data/staging',temp,dirs_exist_ok=True)
 output=subprocess.check_output(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'trace',win(temp),win(trace)],text=True,encoding='utf-8-sig')
(BUILD/'currency-keys-oracle.jsonl').write_text(output)
expected=[]
for line in output.splitlines():
 want=json.loads(line);r,s=want['result'],want['snapshot'];ui=s['Ui']
 expected.append(dict(handled=r['Handled'],cancel=r['CancelComposition'],chinese=ui['IsChinese'],mode=ui['CompositionState'],page=s['CandidatePageIndex'],raw=s['RawInput'],commit=r['TextToOutput'] or '',candidates=ui['Candidates'],annotations=ui['CandidateAnnotations']))
for platform,exe in [('linux',BUILD/'engine_probe'),('arm64',BUILD/'tests/ARM64/engine_probe.exe')]:
 paths=[ROOT/'data/tiger-v2.tcd',tsv]
 actual=[json.loads(x) for x in subprocess.check_output([str(exe)]+[win(p) if platform=='arm64' else str(p) for p in paths],text=True).splitlines()]
 assert len(actual)==len(expected)==len(keys)
 failures=[dict(index=i,case=keys[i]['case'],expected=w,actual=a) for i,(w,a) in enumerate(zip(expected,actual)) if w!=a]
 report=dict(events=len(keys),cases=sum(k['reset'] for k in keys),mismatches=len(failures),failures=failures,probe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
 (BUILD/f'currency-key-parity-{platform}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
 print(platform,'events',len(keys),'mismatches',len(failures),json.dumps(failures[:3],ensure_ascii=False)); assert not failures
