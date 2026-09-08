"""ARM64 configuration toggles, independent hosts, and failed replacement."""
import subprocess,tempfile,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; EXE=ROOT/'build/tests/ARM64/config_store_probe.exe'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def run(path,n,*args):return subprocess.check_output([str(EXE),win(path),str(n),*args],text=True).strip()
with tempfile.TemporaryDirectory(prefix='config-store-',dir=ROOT/'build') as temp:
 root=Path(temp);path=root/'config.txt'
 assert run(path,1)=='1';assert run(path,1)=='0'
 cases=['# 注释\r\n主题\t海蓝\r\n隐藏候选\t否\r\n未知项目\t保留', '隐藏候选 是\n隐藏候选,否\n# 隐藏候选 是\n', '隐藏候选\n字体 #霞鹜文楷 GB 屏幕阅读版\n']
 for encoding in ['utf-8-sig','utf-16','utf-32']:
  for text in cases:
   path.write_bytes(text.encode(encoding));assert run(path,1)=='1'
   updated=path.read_text(encoding='utf-8-sig')
   assert updated.count('隐藏候选\t是')==1
   for line in text.splitlines():
    if not line.startswith(('隐藏候选 ', '隐藏候选\t','隐藏候选,')):assert line in updated
   assert run(path,1)=='0'
 path.write_text('# retained\n隐藏候选 否\n主题 海蓝\n',encoding='utf-8')
 jobs=[subprocess.Popen([str(EXE),win(path),'13'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) for _ in range(4)]
 readers=[subprocess.Popen([str(EXE),win(path),'1000',mode],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) for mode in ['read-loop','focus-loop']]
 outputs=[]
 for job in jobs:
  out,err=job.communicate(timeout=30);assert job.returncode==0,err;outputs+=out.splitlines()
 assert len(outputs)==52 and outputs.count('1')==26 and outputs.count('0')==26
 for reader in readers:
  out,err=reader.communicate(timeout=30);assert reader.returncode==0 and out.strip()=='read',err
 assert run(path,0)=='0' and '# retained' in path.read_text(encoding='utf-8-sig')
 assert not list(root.glob('config.txt.tmp.*')),'Successful writes left temporary files'
 before=path.read_bytes();assert run(path,1,'deny-replace')=='blocked';assert path.read_bytes()==before
 # An invalid original must not be overwritten by a default interpretation.
 path.write_bytes(b'\xff\xff\xff');before=path.read_bytes()
 bad=subprocess.run([str(EXE),win(path),'1'],text=True,capture_output=True)
 assert bad.returncode and path.read_bytes()==before
 path.write_bytes(('#'+'中'*1398100).encode());before=path.read_bytes()
 large=subprocess.run([str(EXE),win(path),'1'],text=True,capture_output=True)
 assert large.returncode and path.read_bytes()==before
 report=dict(status='passed',encoding_cases=9,concurrent_processes=4,concurrent_toggles=52,concurrent_readers=2,reader_iterations=2000,blocked_replace_preserves_original=True,invalid_input_preserved=True,output_limit_preserves_original=True,successful_write_cleanup=True,probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
 (ROOT/'build/config-store-arm64.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
