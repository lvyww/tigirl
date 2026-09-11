"""Original config-line/getter semantics with Tigirl defaults against native settings."""
import hashlib,json,random,subprocess,tempfile
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def hx(s):
 b=s.encode('utf-16-le','surrogatepass');return ''.join(f'{int.from_bytes(b[i:i+2],"little"):04x}' for i in range(0,len(b),2)) or '-'
booleans=['自动启用整句模式','整句自动提前上屏','允许单字重码组句']
numbers=['保留最少编码数量','高频字仅使用最优码组句']
white='整句允许全码组句白名单'
values=['',' ','\t','\u00a0','是','否','TrUe','FALSE','oN','Off','1','0','unknown','-1','-0','+0','+1500','32','33','20001','2147483647','2147483648','-2147483648','-2147483649','12x','12\0','12\0\0','12 \0','12\0 ','+ 12','١٢','１２','\u00a012\u00a0','12.0','+','0000000000000000000000000000000000000012']
cases=['','# 注释\n','高频字仅使用最优码组句\t','整句允许全码组句白名单\t','整句神经重排\t是\nQwen\t开']
for key in booleans+numbers:
 for value in values:
  for sep in ['\t',' ',',']:
   cases.append(key+sep+value)
for value in ['', '中 国\t人','\u00a0中\u2003国\u3000','e\u0301😀👩\u200d💻\ud800','中中人人','\u0301\u00a0\u0301']:
 cases.append(white+'\t'+value)
rng=random.Random(91357)
for _ in range(400):
 lines=[]
 for _ in range(rng.randrange(1,18)):
  key=rng.choice(booleans+numbers+[white,'其他设置'])
  value=rng.choice(values if key!=white else ['中 国','', '一人','😀😀e\u0301'])
  lines.append(rng.choice(['',' ','\t','#'])+key+rng.choice(['\t',' ',','])+value)
 cases.append(rng.choice(['\n','\r\n','\r']).join(lines))
with tempfile.TemporaryDirectory(prefix='sentence-settings-',dir=BUILD) as tmp:
 fixture=Path(tmp)/'cases.txt';fixture.write_text('\n'.join(map(hx,cases))+'\n')
 oracle=ROOT/'tools/SentenceOracle/bin/Release/net10.0-windows/SentenceOracle.dll'
 r=run_windows(['/mnt/c/Program Files/dotnet/dotnet.exe',win(oracle),'--settings',win(fixture)],capture_output=True,text=True,timeout=60);assert r.returncode==0,r.stderr
 expected=[json.loads(line) for line in r.stdout.splitlines()];assert len(expected)==len(cases)
 results={}
 for platform in ['ARM64','x64','Win32']:
  probe=BUILD/'tests'/platform/'sentence_settings_probe.exe'
  r=run_windows([probe,win(fixture)],capture_output=True,text=True,timeout=60);assert r.returncode==0,r.stderr
  actual=[json.loads(line) for line in r.stdout.splitlines()];assert len(actual)==len(expected)
  for i,(a,b) in enumerate(zip(actual,expected)):assert a==b,(platform,i,repr(cases[i]),a,b)
  results[platform]={'cases':len(actual),'sha256':hashlib.sha256(probe.read_bytes()).hexdigest()}
report={'status':'passed','platforms':results,'sources':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['native/SentenceSettings.h','native/SentenceSettings.cpp','tools/SentenceOracle/SentenceSettingsOracle.cs','tests/sentence_settings_test.py']}}
(BUILD/'sentence-settings-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
