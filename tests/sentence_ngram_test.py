"""Compare the native mapped n-gram reader to frozen original C# scoring."""
import hashlib,json,math,mmap,random,struct,subprocess,tempfile
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1]; BUILD=ROOT/'build'
MODEL=Path('/mnt/c/Users/yc/Desktop/bime_codex_src_20260513/release_arm64/Models/sentence-ngram-v2.bin')
DOTNET=Path('/mnt/c/Program Files/dotnet/dotnet.exe')
ORACLE=ROOT/'tools/SentenceOracle/bin/Release/net10.0-windows/SentenceOracle.dll'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
def hex_token(value):
    if isinstance(value,str):return value
    if value<=0xffff:return f'{value:04x}'
    value-=0x10000;return f'{0xd800+(value>>10):04x}{0xdc00+(value&1023):04x}'
def invoke(platform,model,queries):
    if platform=='oracle':return run_windows([DOTNET,win(ORACLE),win(model),win(queries)],text=True,capture_output=True,timeout=60)
    if platform=='linux':return subprocess.run([BUILD/'sentence_ngram_probe',model,queries],text=True,capture_output=True,timeout=60)
    return run_windows([BUILD/'tests'/platform/'sentence_ngram_probe.exe',win(model),win(queries)],text=True,capture_output=True,timeout=60)
rng=random.Random(20260908);queries=[]
with MODEL.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as m:
    position=12;indices=[]
    for wide in [False,True,False,True,True]:
        n=struct.unpack_from('<Q' if wide else '<I',m,position)[0];position+=8 if wide else 4
        indices.append((position,n,12 if wide else 8));position+=n*(12 if wide else 8)
    assert position==len(m)
    pool=[0,ord('你'),ord('好'),ord('我'),ord('的'),0x1f600,0x20000,0xd800,0xdc00,'-','4f60597d','00610301']
    offset,n,stride=indices[0]
    pool += [struct.unpack_from('<I',m,offset+rng.randrange(n)*stride)[0] for _ in range(300)]
    for _ in range(1500):queries.append(tuple(rng.choice(pool) for _ in range(3)))
    for kind in [1,3]:
        offset,n,stride=indices[kind]
        for _ in range(min(2500,n)):
            key=struct.unpack_from('<Q',m,offset+rng.randrange(n)*stride)[0]
            triple=((key>>42)&0x1fffff,(key>>21)&0x1fffff,key&0x1fffff)
            queries.append(triple)
queries += [('-', '-', '-'),('d83dde00','d800','4f60597d'),('4f60','597d','d83dde00')]
rows=[' '.join(map(hex_token,row))+f' {include}' for row in queries for include in [0,1]]
with tempfile.TemporaryDirectory(prefix='sentence-ngram-',dir=BUILD) as temporary:
    tmp=Path(temporary);q=tmp/'queries.txt';q.write_text('\n'.join(rows)+'\n')
    ref=invoke('oracle',MODEL,q);assert ref.returncode==0,ref.stderr
    expected=[(float(a),int(b)) for a,b in (line.split() for line in ref.stdout.splitlines())]
    assert len(expected)==len(rows)
    results={}
    for platform in ['linux','ARM64','x64','Win32']:
        output=invoke(platform,MODEL,q);assert output.returncode==0,(platform,output.stderr)
        actual=[(float(a),int(b)) for a,b in (line.split() for line in output.stdout.splitlines())]
        assert len(actual)==len(expected)
        for i,(a,e) in enumerate(zip(actual,expected)):
            assert math.isclose(a[0],e[0],rel_tol=1e-13,abs_tol=1e-12) and a[1]==e[1],(platform,rows[i],a,e)
        results[platform]={'queries':len(actual),'max_absolute_error':max(abs(a[0]-e[0]) for a,e in zip(actual,expected))}
    valid=b'TCSKNM01'+struct.pack('<IIIf',1,1,0,.1)+struct.pack('<QIQ Q',0,0,0,0)
    bads={'empty':b'','short':valid[:7],'version':valid[:8]+struct.pack('<I',2)+valid[12:],
        'negative_count':valid[:12]+struct.pack('<I',0xffffffff)+valid[16:],
        'truncated':valid[:-1],'trailing':valid+b'x','unknown_nan':valid[:20]+struct.pack('<f',float('nan'))+valid[24:],
        'unknown_zero':valid[:20]+struct.pack('<f',0)+valid[24:]}
    small=tmp/'small.bin';small.write_bytes(valid)
    for platform in ['oracle','linux','ARM64','x64','Win32']:
        assert invoke(platform,small,q).returncode==0,platform
        for name,contents in bads.items():
            small.write_bytes(contents);r=invoke(platform,small,q)
            assert r.returncode!=0,(platform,name)
        small.write_bytes(valid)
    report={'status':'passed','platforms':results,'invalid_models':list(bads),'same_process_mapping_reuse':True,
        'model_sha256':hashlib.sha256(MODEL.read_bytes()).hexdigest(),
        'native_source_sha256':hashlib.sha256((ROOT/'native/SentenceNgram.cpp').read_bytes()).hexdigest(),
        'oracle_source_sha256':hashlib.sha256((ROOT/'tools/SentenceOracle/upstream/TigerClaw.Core/SentenceNgramModel.cs').read_bytes()).hexdigest(),
        'sentence_decoding_integrated':False,'installed':False}
(BUILD/'sentence-ngram-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
