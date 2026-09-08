"""Original automatic word codes, alias/escape decoding and text elements."""
import json,subprocess,tempfile,shutil,random,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
cases=['İIıßẞΣς𐐀𐐁','','中国','中国人','中华人民共和国','A','Ab','Abc','Abcd','中文ABC','𤕫虎','😀虎','虎😀码','虎\u0301码','क्\u200dष虎','क्ष虎','👩🏽\u200d💻虎','🇨🇳虎','\r\n虎','词条=>上屏','相同=>相同','=>词条','词条=>','a=>b=>c','\\n','\\t','\\s','\\\\n','Bime20231222BIME','  词条  ','……虎——码','…虎—码','\u3000词条\u3000']
rng=random.Random(608)
alphabet='中国人民虎码输入法AaZz\n\r\t =，-。·【、】；）！@#￥%&*（+《》~{|}？：,.`[\\]/;\')!$^<_>?"😀𤕫é\u0301\u200d'
for _ in range(600):cases.append(''.join(rng.choice(alphabet) for _ in range(rng.randrange(1,14))))
trace=BUILD/'add-word-cases.jsonl';trace.write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in cases),encoding='utf-8')
with tempfile.TemporaryDirectory(prefix='add-word-oracle-',dir=BUILD) as temp:
 shutil.copytree(ROOT/'data/staging',temp,dirs_exist_ok=True)
 output=subprocess.check_output(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'addword',win(temp),win(trace)],text=True,encoding='utf-8-sig')
expected=[json.loads(x) for x in output.splitlines()];assert len(expected)==len(cases)
(BUILD/'add-word-oracle.jsonl').write_text(output)
inputs=BUILD/'add-word-cases.hex';inputs.write_text('\n'.join(s.encode().hex() for s in cases)+'\n')
exe=BUILD/'tests/ARM64/add_word_probe.exe'
actual=subprocess.check_output([str(exe),win(ROOT/'data/tiger-v2.tcd'),win(inputs)],text=True).splitlines();assert len(actual)==len(cases)
failures=[]
for wanted,line in zip(expected,actual):
 code,packed,normalized,elements=line.split('\t')
 got=dict(normalized=bytes.fromhex(normalized).decode(),code=bytes.fromhex(code).decode(),packed=bytes.fromhex(packed).decode(),elements=[bytes.fromhex(x).decode() for x in elements.split(',')] if elements else [])
 if any(got[k]!=wanted[k] for k in got):failures.append(dict(text=wanted['text'],expected={k:wanted[k] for k in got},actual=got))
report=dict(cases=len(cases),mismatches=len(failures),failures=failures,probe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
(BUILD/'add-word-parity-arm64.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(len(cases),'cases;',len(failures),'mismatches',json.dumps(failures[:5],ensure_ascii=False));assert not failures
