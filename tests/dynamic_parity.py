"""Original dynamic-token conversion with before/after clocks and random membership."""
import json,subprocess,shutil,tempfile,datetime,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
tokens=['{日期}','{日期.}','{日期-}','{日期/}','{时分秒}','{时分}','{星期}','{周}','普通文字','','{未知}','前缀{日期}','{甲|乙}','{||甲||乙|}','{|}','{||}','{ | }','{😀|𤕫}','{甲|{日期}}','{甲|甲|乙}']
trace=BUILD/'dynamic-cases.jsonl';trace.write_text(''.join(json.dumps(t,ensure_ascii=False)+'\n' for t in tokens),encoding='utf-8')
with tempfile.TemporaryDirectory(prefix='dynamic-oracle-',dir=BUILD) as temp:
 shutil.copytree(ROOT/'data/staging',temp,dirs_exist_ok=True)
 output=subprocess.check_output(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'dynamic',win(temp),win(trace)],text=True,encoding='utf-8-sig')
expected=[json.loads(x) for x in output.splitlines()];assert len(expected)==len(tokens)
(BUILD/'dynamic-oracle.jsonl').write_text(output,encoding='utf-8')
rows=[]
def row(text,clock):
 rows.append(' '.join(str(clock[k]) for k in ['year','month','day','hour','minute','second','weekday'])+' '+clock['weekdayName'].encode().hex()+' '+text.encode().hex())
for case in expected:
 for clock in ['before','after']:row(case['text'],case[clock])
fixed=[]
for date in [datetime.datetime(2000,2,29,0,0,1),datetime.datetime(2026,12,31,23,59,59),datetime.datetime(2027,1,1,1,2,3)]:
 for day in range(7):
  clock=dict(year=date.year,month=date.month,day=date.day,hour=date.hour,minute=date.minute,second=date.second,weekday=day,weekdayName='星期'+'日一二三四五六'[day])
  wants=[date.strftime('%Y年%m月%d日'),date.strftime('%Y.%m.%d'),date.strftime('%Y-%m-%d'),date.strftime('%Y/%m/%d'),date.strftime('%H:%M:%S'),date.strftime('%H:%M'),clock['weekdayName'],'周'+'日一二三四五六'[day]]
  for token,want in zip(tokens[:8],wants):row(token,clock);fixed.append(want)
for _ in range(64):row('{甲|乙}',expected[0]['before'])
tsv=BUILD/'dynamic-cases.tsv';tsv.write_text('\n'.join(rows)+'\n')
for exe in [BUILD/'dynamic_probe',BUILD/'tests/ARM64/dynamic_probe.exe']:
 assert exe.is_file(),f'Build required probe first: {exe}'
 actual=[bytes.fromhex(x).decode() for x in subprocess.check_output([str(exe),win(tsv) if exe.suffix=='.exe' else str(tsv)],text=True).splitlines()]
 assert len(actual)==len(rows)
 for i,case in enumerate(expected):
  text=case['text']; choices=[x for x in text[1:-1].split('|') if x] if len(text)>3 and text.startswith('{') and text.endswith('}') and '|' in text else []
  if choices:
   assert case['output'] in choices and all(x in choices for x in actual[i*2:i*2+2]),case
  else: assert case['output'] in actual[i*2:i*2+2],(case,actual[i*2:i*2+2])
 assert actual[len(expected)*2:len(expected)*2+len(fixed)]==fixed
 assert set(actual[-64:])=={'甲','乙'}
 result=dict(oracle_tokens=len(tokens),fixed_clock_cases=len(fixed),random_draws=64,copied_random_state=True,status='passed',probe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
 suffix='arm64' if exe.suffix=='.exe' else 'linux';(BUILD/f'dynamic-parity-{suffix}.json').write_text(json.dumps(result,indent=2)+'\n');print(suffix,json.dumps(result))
 engine=exe.with_name('dynamic_engine_probe'+exe.suffix)
 assert engine.is_file(),f'Build required engine probe first: {engine}'
 dictionary=ROOT/'data/tiger-v2.tcd'
 integration=json.loads(subprocess.check_output([str(engine),win(dictionary) if exe.suffix=='.exe' else str(dictionary)],text=True))
 assert integration['status']=='passed'
 integration['probe_sha256']=hashlib.sha256(engine.read_bytes()).hexdigest()
 (BUILD/f'dynamic-engine-{suffix}.json').write_text(json.dumps(integration,indent=2)+'\n');print(suffix,json.dumps(integration))
