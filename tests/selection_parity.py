"""Compare native selection-file parsing with the unmodified original parser."""
import json
import random
import shutil
import subprocess
import tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
cases = ['', '# 注释\r\n', '1选', '2选 VK_A VK_A 65\n1选 VK_A', '1选 VK_RSHIFT\n1选 VK_F24',
         '+1选 +49', '01选 0x00000031', '10选 -2147483648 2147483647 0xFFFFFFFF 0x80000000 256 -1 0',
         '1选 0x100000000', '1选 2147483648', '1选 -2147483649', '1选 0x-1',
         '1选 VK_F25', '1选 VK_OEM_3', '0选 49', '11选 49', '1 49', '1选 49 # trailing',
         '1选 vk_a\r2选 Vk_F1', '\u30001选\tVK_SPACE\u3000', '1选\u00a049',
         '1选 VK_A\n2选 INVALID', '1选 0x', '1选 +', '1选 0x00000000000031']
names = ['SHIFT','LSHIFT','RSHIFT','CONTROL','LCONTROL','RCONTROL','MENU','LMENU','RMENU',
         'LWIN','RWIN','CAPITAL','SPACE','BACK','RETURN','TAB','ESCAPE','OEM_1','OEM_2','OEM_4',
         'OEM_7','OEM_COMMA','OEM_PERIOD'] + list('0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ') + [f'F{i}' for i in range(1,25)]
cases += [f'2选 VK_{name}' for name in names]
rng = random.Random(415)
for _ in range(100):
    lines=[]
    for _ in range(rng.randrange(1,15)):
        keys = [rng.choice([f'VK_{rng.choice(names)}', str(rng.randrange(-2,300)), hex(rng.randrange(256))]) for _ in range(rng.randrange(6))]
        lines.append(f'{rng.randrange(1,11)}选\t' + ' '.join(keys))
    cases.append('\r\n'.join(lines))
(BUILD/'selection-cases.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in cases),encoding='utf-8')
(BUILD/'selection-cases.hex').write_text(''.join(x.encode().hex()+'\n' for x in cases),encoding='ascii')
def win(path): return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
with tempfile.TemporaryDirectory(prefix='selection-oracle-',dir=BUILD) as temp:
    staging=Path(temp)/'runtime'
    shutil.copytree(ROOT/'data/staging',staging)
    output=subprocess.check_output(['/mnt/c/Program Files/dotnet/dotnet.exe',win(ROOT/'tools/ReferenceOracle/bin/Release/net10.0-windows/ReferenceOracle.dll'),
        'selection',win(staging),win(BUILD/'selection-cases.jsonl')],text=True,encoding='utf-8-sig')
expected=[json.loads(x) for x in output.splitlines()]
(BUILD/'selection-oracle.jsonl').write_text(output,encoding='utf-8')
executables=[BUILD/'selection_probe']
if (BUILD/'tests/ARM64/selection_probe.exe').exists(): executables.append(BUILD/'tests/ARM64/selection_probe.exe')
for exe in executables:
    trace=win(BUILD/'selection-cases.hex') if exe.suffix=='.exe' else str(BUILD/'selection-cases.hex')
    lines=subprocess.check_output([str(exe),trace],text=True).splitlines()
    assert len(lines)==len(expected)
    for i,(line,want) in enumerate(zip(lines,expected)):
        valid,encoded=line.split('\t')
        actual={'valid':valid=='1','canonical':bytes.fromhex(encoded).decode()}
        assert actual==want,(i,cases[i],actual,want)
    file_checks=0
    with tempfile.TemporaryDirectory(prefix='selection-files-',dir=BUILD) as temp:
        path=Path(temp)/'选词键.txt'
        argument=win(path) if exe.suffix=='.exe' else str(path)
        def load(): return subprocess.run([str(exe),'--file',argument],capture_output=True,text=True)
        # Missing file uses defaults; loading never creates or rewrites it.
        got=load(); assert got.returncode==0 and not path.exists()
        assert bytes.fromhex(got.stdout.strip().split('\t')[1]).decode()==expected[0]['canonical']
        file_checks+=1
        for encoding,bom in [('utf-8',b''),('utf-8',b'\xef\xbb\xbf'),
                ('utf-16-le',b'\xff\xfe'),('utf-16-be',b'\xfe\xff'),
                ('utf-32-le',b'\xff\xfe\0\0'),('utf-32-be',b'\0\0\xfe\xff')]:
            data=bom+cases[3].encode(encoding); path.write_bytes(data)
            got=load(); assert got.returncode==0,got.stderr
            assert bytes.fromhex(got.stdout.strip().split('\t')[1]).decode()==expected[3]['canonical']
            assert path.read_bytes()==data
            file_checks+=1
        for data in [b'\xff\xfe\x31',b'\0\0\xfe\xff\0\x11\0\0',b'\xff\xfe\x00\xd8',b'\xff',b'bad label']:
            path.write_bytes(data); got=load()
            assert got.returncode!=0 and path.read_bytes()==data
            file_checks+=1
    result={'cases':len(cases),'file_checks':file_checks,'mismatches':0,'executable':str(exe),'status':'passed'}
    suffix='arm64' if exe.suffix=='.exe' else 'linux'
    (BUILD/f'selection-parity-{suffix}.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))
