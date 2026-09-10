"""Real foreground TSF mouse messages; no global injected input."""
import hashlib,json,shutil,subprocess,tempfile,sys
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1]
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
report={'status':'running','checks':[],'physical_input_tested':False,'input_desktop_switched':False}
try:
 for arch,machine in [('ARM64','arm64'),('x64','amd64'),('Win32','x86')]:
  with tempfile.TemporaryDirectory(prefix='candidate-mouse-'+arch+'-',dir=ROOT/'build') as temp:
   root=Path(temp);(root/'.tsf-candidate-mouse-test').touch();(root/'.tsf-candidate-mask-test').touch();package=root/'package';package.mkdir()
   source=ROOT/'build/ARM64X/ARM64EC/Release'
   dll=ROOT/'build/Win32/Release/Tigirl.dll' if arch=='Win32' else source/'Tigirl.dll'
   for name in ['Tigirl.dll','tiger-v2.tcd','Tigirl.exe']:
    shutil.copy2(dll if name=='Tigirl.dll' else source/name,package/name)
   manifest=package/'NativeTiger.Test.manifest'
   manifest.write_text((ROOT/'tests/NativeTiger.Test.manifest').read_text().replace('processorArchitecture="arm64"',f'processorArchitecture="{machine}"'),encoding='utf-8-sig')
   (root/'config.txt').write_text('默认中文\t是\n字体\tSegoe UI\n字体大小\t17\n编码伪装\t甲😀\n未知设置\t保留\n',encoding='utf-8-sig')
   exe=ROOT/'build/tests'/arch/'tsf_host.exe'
   result=run_windows([str(exe),win(package/'Tigirl.dll'),win(root/'capture.bmp'),win(manifest),'--candidate-async' if '--async' in sys.argv else '--candidate-mouse',win(root)],capture_output=True,text=True,timeout=45)
   check={'architecture':arch,'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr,'dll_sha256':hashlib.sha256(dll.read_bytes()).hexdigest(),'host_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()}
   report['checks'].append(check)
   if (root/'error.txt').exists():check['error']=(root/'error.txt').read_text()
   if (root/'result.json').exists():check['result']=json.loads((root/'result.json').read_text())
   assert result.returncode==0,check
   assert check['result']['status']=='passed',check
   assert '未知设置\t保留' in (root/'config.txt').read_text(encoding='utf-8-sig')
   print(json.dumps(check),flush=True)
 report['status']='passed'
except Exception as error:
 report['status']='failed';report['error']=str(error)
 raise
finally:
 (ROOT/('build/candidate-async-host-validation.json' if '--async' in sys.argv else 'build/candidate-mouse-validation.json')).write_text(json.dumps(report,indent=2)+'\n')
