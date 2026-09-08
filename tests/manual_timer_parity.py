"""Original regex and .NET scheduling arithmetic, without scheduling popups."""
import json,random,subprocess,tempfile,shutil,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
numbers=['','0','00','1','1.5','.5','1.','.',',',',,,','1,2','1,,2','1,.2','1.2,','-1','+1','1e2','NaN','Infinity',' 1','1 ','１','1\n','1\n\n','1\r\n','35791.39411666666','35791.39411666667','0.0000001','0.'+'0'*400+'1','9'*400]
cases=[prefix+n for prefix in ['Ds','DS','ds','D','S'] for n in numbers]
rng=random.Random(603)
cases+=['Ds'+''.join(rng.choice('0123456789,.') for _ in range(rng.randrange(1,400))) for _ in range(1000)]
for _ in range(1000):
 number=''.join(rng.choice('0123456789,') for _ in range(rng.randrange(1,400)))
 if rng.randrange(2):
  position=rng.randrange(len(number));number=number[:position]+'.'+number[position:]
 cases.append('DS'+number)
trace=BUILD/'manual-timer-cases.jsonl';trace.write_text(''.join(json.dumps(x)+'\n' for x in cases))
inputs=BUILD/'manual-timer-cases.hex';inputs.write_text(''.join(x.encode().hex()+'\n' for x in cases))
with tempfile.TemporaryDirectory(prefix='manual-timer-oracle-',dir=BUILD) as tmp:
 shutil.copytree(ROOT/'data/staging',tmp,dirs_exist_ok=True)
 output=subprocess.check_output(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'timer',win(tmp),win(trace)],text=True)
expected=[json.loads(x) for x in output.splitlines()]
for arch,exe in [('linux',BUILD/'manual_timer_probe'),('arm64',BUILD/'tests/ARM64/manual_timer_probe.exe')]:
 actual=[json.loads(x) for x in subprocess.check_output([str(exe),win(inputs) if arch=='arm64' else str(inputs)],text=True).splitlines()]
 assert len(expected)==len(actual)==len(cases)
 failures=[dict(input=cases[i],expected=a,actual=b) for i,(a,b) in enumerate(zip(expected,actual)) if a!=b]
 report=dict(cases=len(cases),mismatches=len(failures),failures=failures[:5],probe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
 (BUILD/f'manual-timer-parity-{arch}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
 print(arch,report);assert not failures
