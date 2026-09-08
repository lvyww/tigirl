"""Original decimal currency and numeric-prefix oracle; no timers are scheduled."""
import json,subprocess,shutil,tempfile,random,re,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; BUILD=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
rng=random.Random(51608)
cases=['S'+s for s in ['0','0.01','0.1','0.005','1.005','1.0049','100000001','100001000','79228162514264337593543950335','79228162514264337593543950335.4','79228162514264337593543950335.5','79228162514264337593543950336','.',',',',,,',',.1','1,2,3.4,5','000.000']]
for exponent in range(30):
 for delta in [-1,0,1,10,100,1000]:
  if 10**exponent+delta>=0:cases.append('S'+str(10**exponent+delta))
for _ in range(1500):
 integer=''.join(rng.choice('0000123456789') for _ in range(rng.randrange(0,34)))
 fraction=''.join(rng.choice('0000123456789') for _ in range(rng.randrange(0,38)))
 cases.append('S'+integer+('.'+fraction if fraction else ''))
for base in ['0','1','99999999999999999999999999','7922816251426433759354395033']:
 for tail in ['00499999999999999999999999999999','00500000000000000000000000000000','00500000000000000000000000000001','995','99999999999999999999999999999999']:
  cases.append('S'+base+'.'+tail)
for prefix in ['S','s','D','d','Ds','DS','X']:
 for value in ['','1','1.','1.2','1,2','1e3','1e-9999','1e9999','NaN','Infinity','inf','nan(1)','+NaN','-Infinity','+1','-1','.1',' 1 ','1x','1e','..']:
  cases.append(prefix+value)
cases=list(dict.fromkeys(cases));trace=BUILD/'currency-cases.jsonl';trace.write_text(''.join(json.dumps(x)+'\n' for x in cases))
with tempfile.TemporaryDirectory(prefix='currency-oracle-',dir=BUILD) as temp:
 shutil.copytree(ROOT/'data/staging',temp,dirs_exist_ok=True)
 output=subprocess.check_output(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'currency',win(temp),win(trace)],text=True,encoding='utf-8-sig')
expected=[json.loads(x) for x in output.splitlines()];assert len(expected)==len(cases)
(BUILD/'currency-oracle.jsonl').write_text(output)
plain=BUILD/'currency-cases.txt';plain.write_text('\n'.join(cases)+'\n')
for platform,exe in [('linux',BUILD/'uppercase_probe'),('arm64',BUILD/'tests/ARM64/uppercase_probe.exe')]:
 assert exe.is_file(),f'Build {exe} first'
 actual=subprocess.check_output([str(exe),win(plain) if platform=='arm64' else str(plain)],text=True).splitlines();assert len(actual)==len(cases)
 failures=[]
 for case,row in zip(expected,actual):
  prefix,text=row.split('\t');text=bytes.fromhex(text).decode()
  wanted=case['output'] if re.fullmatch(r'S([0-9,]*[.])?[0-9,]+',case['text']) else case['text']
  if text!=wanted or bool(int(prefix))!=case['prefix']:failures.append(dict(input=case['text'],expected=wanted,actual=text,expected_prefix=case['prefix'],actual_prefix=bool(int(prefix))))
 report=dict(cases=len(cases),mismatches=len(failures),failures=failures,probe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
 (BUILD/f'currency-parity-{platform}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
 print(platform,len(cases),'mismatches',len(failures),json.dumps(failures[:8],ensure_ascii=False))
 assert not failures
