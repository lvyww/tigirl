"""Stateful mixed decoder parity against the unchanged original Core decoder."""
import argparse, hashlib, json, random, shutil, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
def win(p): return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
parser=argparse.ArgumentParser()
parser.add_argument('--windows',action='store_true')
parser.add_argument('--replay',action='store_true')
args=parser.parse_args()
cases=[]
def case(raw,maximum=4,version=0,preferred=None,clear=False):
    cases.append(dict(raw=raw,maximum=maximum,version=version,preferred=preferred or {},clear=clear))
for maximum in [-1,0,1,2,4,16]:
    for n in range(70): case(('aaaaZZZZbbbbcccc'*5)[:n],maximum,clear=n==0)
    for n in reversed(range(70)): case(('aaaaZZZZbbbbcccc'*5)[:n],maximum)
for version in [0,0,1,1,2,0]:
    for raw in ['aaaaA','AAAAa','zzzzA','ZZZZa','aaaaZZZZb']:
        case(raw,version=version)
        case(raw,version=version,preferred={0:'选中=>上屏',4:'',99:'忽略'})
        case(raw,version=version,preferred={0:''})
rng=random.Random(497)
for i in range(1000):
    raw=''.join(rng.choice('aAbBzZ/;[') for _ in range(rng.randrange(150)))
    maximum=rng.choice([1,2,4,16])
    case(raw,maximum,i//100,{0:rng.choice(['','候选','😀']),maximum:'固定'},i%137==0)
case('aaaa'*4096+'x',4,100,clear=True)
source=BUILD/'mixed-cases.jsonl'; trace=BUILD/'mixed-cases.tsv'
source.write_text(''.join(json.dumps(c,ensure_ascii=False)+'\n' for c in cases),encoding='utf-8')
trace.write_text(''.join(f"{int(c['clear'])} {c['maximum']} {c['version']} {json.dumps(c['raw'])} {len(c['preferred'])}"+
    ''.join(f' {start} {json.dumps(value,ensure_ascii=False)}' for start,value in c['preferred'].items())+'\n' for c in cases),encoding='utf-8')
oracle=BUILD/'mixed-oracle.jsonl'; metadata=BUILD/'mixed-oracle-inputs.json'
inputs=[source,ROOT/'tools/ReferenceOracle/Oracle.cs',ROOT/'tools/ReferenceOracle/upstream/TigerClaw.Core/MixedInputDecoder.cs']
fingerprints={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
if args.replay:
    assert json.loads(metadata.read_text())==fingerprints,'Regenerate oracle after input changes'
else:
    with tempfile.TemporaryDirectory(prefix='mixed-oracle-',dir=BUILD) as tmp, oracle.open('w',encoding='utf-8') as output:
        shutil.copytree(ROOT/'data/staging',tmp,dirs_exist_ok=True)
        subprocess.run(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'mixed',win(tmp),win(source)],stdout=output,check=True)
    metadata.write_text(json.dumps(fingerprints,indent=2)+'\n')
exe=BUILD/('tests/ARM64/mixed_probe.exe' if args.windows else 'mixed_probe')
actual=subprocess.check_output([str(exe),win(trace) if args.windows else str(trace)],text=True)
expected=[json.loads(line) for line in oracle.read_text(encoding='utf-8-sig').splitlines()]
got=[json.loads(line) for line in actual.splitlines()]
assert len(expected)==len(got)==len(cases)
failures=[dict(index=i,input=cases[i],expected=a,actual=b) for i,(a,b) in enumerate(zip(expected,got)) if a!=b]
report=dict(cases=len(cases),mismatches=len(failures),failures=failures[:5],probe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
suffix='arm64' if args.windows else 'linux'
(BUILD/f'mixed-parity-{suffix}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(report,ensure_ascii=False))
raise SystemExit(bool(failures))
