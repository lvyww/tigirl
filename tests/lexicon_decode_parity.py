"""Raw table bytes compared with original encoding detection and .NET reading."""
import hashlib,json,random,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';EXE=BUILD/'tests/ARM64/lexicon_decode_probe.exe'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
cases=[b'']+[bytes([i]) for i in range(256)]
boms=[b'',bytes.fromhex('efbbbf'),bytes.fromhex('fffe'),bytes.fromhex('feff'),bytes.fromhex('fffe0000'),bytes.fromhex('0000feff')]
edges=['','00','ff','efbb','c080','eda080','e08080','f0808080','f48fbfbf','f4908080','e282','f09f98','e24180','800080','00d8','00dc','00d800','00d84100','00d800dc','00d800d8','d800dc00','0000d800','00110000','ffffffff']
for bom in boms:
 for raw in edges:cases.append(bom+bytes.fromhex(raw))
 for encoding in ['utf-8','utf-16-le','utf-16-be','utf-32-le','utf-32-be']:
  cases.append(bom+'甲\tAb\n😀\0\ufeff尾'.encode(encoding))
rng=random.Random(20260912)
for _ in range(1600):cases.append(rng.choice(boms)+rng.randbytes(rng.randrange(80)))
# Decoder state at the StreamReader buffer boundary, including incomplete units.
for n in [1021,1022,1023,1024,1025,4095,4096]:
 for bom in boms:
  for tail in ['f09f9880','e282','00d800dc','00d800','0001f600','ff']:
   cases.append(bom+b'a'*n+bytes.fromhex(tail))
real_files=sorted((ROOT/'data/staging/码表/虎码字词').glob('*.dict.yaml'))+sorted((ROOT/'data/staging/码表/虎码字词').glob('*.txt'))+sorted((ROOT/'data/staging/码表/虎码字词').glob('*.注释'))+sorted((ROOT/'data/staging/码表/虎码字词').glob('*.拆分'))+sorted((ROOT/'data/staging/拼音反查码表').glob('*.txt'))
cases.extend(p.read_bytes() for p in real_files)
trace=BUILD/'lexicon-decode.hex';trace.write_text(''.join(c.hex()+'\n' for c in cases))
with tempfile.TemporaryDirectory(prefix='decode-oracle-',dir=BUILD) as temporary:
 (Path(temporary)/'.native-tiger-staging').touch()
 expected=subprocess.check_output(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),'decodebytes',win(temporary),win(trace)],text=True).splitlines()
with tempfile.TemporaryDirectory(prefix='decode-native-',dir=BUILD) as temporary:
 actual=subprocess.check_output([str(EXE),win(trace),win(Path(temporary)/'中文码表.bin')],text=True).splitlines()
assert len(actual)==len(expected)==len(cases),(len(actual),len(expected),len(cases))
failures=[dict(index=i,input=cases[i].hex()[:400],expected=e[:400],actual=a[:400]) for i,(e,a) in enumerate(zip(expected,actual)) if e!=a]
report=dict(cases=len(cases),mismatches=len(failures),failures=failures[:20],native_file_reads=len(cases),invalid_paths_rejected=2,unicode_path=True,real_files=[str(p.relative_to(ROOT)) for p in real_files],probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
(BUILD/'lexicon-decode-parity-arm64.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report));assert not failures
