"""Build/run actual Windows renderer probes without foreground or injected keys."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
PS='/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
COMMAND=[PS] if Path('/proc/sys/fs/binfmt_misc/WSLInterop').exists() else ['/init',PS]
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
def quote(value):return "'"+str(value).replace("'","''")+"'"
def run(exe,args):
    return subprocess.run([*COMMAND,'-NoProfile','-Command','& '+quote(exe)+' '+' '.join(map(quote,args))+'; exit $LASTEXITCODE'],capture_output=True,text=True,timeout=120)
msbuild=r'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
report={'status':'running','checks':[],'physical_input_tested':False,'physical_monitor_transition_tested':False}
report_path=ROOT/'build/directwrite-render-validation.json'
try:
    for arch in ['ARM64','x64','Win32']:
        build=run(msbuild,[win(ROOT/'tests/CandidateRenderProbe.vcxproj'),'/p:Configuration=Release','/p:Platform='+arch,'/v:minimal','/nologo'])
        if build.returncode:raise RuntimeError(build.stdout+build.stderr)
        exe=ROOT/'build/tests'/arch/'candidate_render_probe.exe'
        tested=run(win(exe),[win(ROOT/'data/fonts/LXGWWenKaiGBScreen.ttf'),win(ROOT/'build'/('directwrite-render-'+arch))])
        check={'architecture':arch,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'returncode':tested.returncode,'stdout':tested.stdout,'stderr':tested.stderr}
        report['checks'].append(check)
        if tested.returncode:raise RuntimeError(tested.stdout+tested.stderr)
        check['result']=json.loads(tested.stdout)
        print(json.dumps({'architecture':arch,**check['result']}),flush=True)
    report['status']='passed'
except Exception as error:
    report['status']='failed';report['error']=str(error)
    raise
finally:
    sources=['native/tsf/CandidateRenderer.cpp','native/tsf/CandidateRenderer.h','native/tsf/CandidateDpi.h','tests/candidate_render_probe.cpp','tests/CandidateRenderProbe.vcxproj','tests/candidate_render_test.py','data/fonts/LXGWWenKaiGBScreen.ttf']
    report['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sources}
    report_path.write_text(json.dumps(report,indent=2)+'\n')
