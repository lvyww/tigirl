"""Real TSF edit sessions with direct key callbacks; no foreground input."""
import hashlib,json,shutil,subprocess,tempfile,sys
from pathlib import Path
from windows_process import run_windows,PS
from contextlib import contextmanager
ROOT=Path(__file__).resolve().parents[1]
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
@contextmanager
def windows_temporary(prefix):
 root=Path(tempfile.mkdtemp(prefix=prefix,dir=ROOT/'build'))
 try:yield str(root)
 finally:
  # Loaded PE files on DrvFS can yield EIO during Linux unlink even after
  # the host exits. Delete this fixture through the Windows filesystem API.
  quoted=win(root).replace("'","''")
  result=run_windows([PS,'-NoProfile','-Command',f"Remove-Item -LiteralPath '{quoted}' -Recurse -Force -ErrorAction Stop"],capture_output=True,text=True)
  if result.returncode:raise RuntimeError(result.stderr)
report={'status':'running','checks':[],'physical_input_tested':False,'foreground_tested':False}
try:
 for arch,machine in ([('x64','amd64'),('Win32','x86')] if '--release-x64' in sys.argv else [('ARM64','arm64'),('x64','amd64'),('Win32','x86')]):
  with windows_temporary('mask-tsf-'+arch+'-') as temp:
   root=Path(temp);(root/'.tsf-mask-test').touch();package=root/'package';package.mkdir()
   source=ROOT/'build/ARM64X/ARM64EC/Release';dll=ROOT/'build/Win32/Release/Tigirl.dll' if arch=='Win32' else source/'Tigirl.dll'
   if '--release-x64' in sys.argv and arch=='x64':dll=ROOT/'build/x64/Release/Tigirl.dll'
   shutil.copy2(dll,package/'Tigirl.dll');shutil.copy2(source/'tiger-v2.tcd',package/'tiger-v2.tcd')
   manifest=package/'NativeTiger.Test.manifest';manifest.write_text((ROOT/'tests/Tigirl.Test.manifest').read_text().replace('processorArchitecture="arm64"',f'processorArchitecture="{machine}"'),encoding='utf-8-sig')
   (root/'config.txt').write_text('默认中文\t是\n编码伪装\t甲😀\n最大码长\t2\n中英文不限长混合输入\t是\n',encoding='utf-8-sig')
   exe=ROOT/'build/tests'/arch/'tsf_host.exe'
   result=run_windows([str(exe),win(package/'Tigirl.dll'),'-',win(manifest),'--mask-detached',win(root)],capture_output=True,text=True,timeout=30)
   check={'architecture':arch,'returncode':result.returncode,'stderr':result.stderr,'stdout':result.stdout,'dll_sha256':hashlib.sha256(dll.read_bytes()).hexdigest(),'host_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()};report['checks'].append(check)
   assert result.returncode==0,check
   check['result']=json.loads(result.stdout);assert check['result']['status']=='passed'
   print(json.dumps(check),flush=True)
 report['status']='passed'
except Exception as error:
 report['status']='failed';report['error']=str(error);raise
finally:
 (ROOT/'build/code-mask-tsf-validation.json').write_text(json.dumps(report,indent=2)+'\n')
