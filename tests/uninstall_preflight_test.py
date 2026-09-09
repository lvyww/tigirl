"""Uninstall validation only; never invoke registration removal or elevation."""
import json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';PS='/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
def check(record):return subprocess.run([PS,'-NoProfile','-ExecutionPolicy','Bypass','-File',win(ROOT/'uninstall_arm64.ps1'),'-Record',win(record),'-CheckOnly'],capture_output=True,text=True,timeout=15)
record=BUILD/'native-install.json';original=json.loads(record.read_text(encoding='utf-8-sig'))
result=check(record);assert result.returncode==0,result.stderr;plan=json.loads(result.stdout)
assert plan['registered'] and plan['profile_registered'] and plan['unregister_required']
with tempfile.TemporaryDirectory(prefix='uninstall-preflight-',dir=BUILD) as temporary:
 root=Path(temporary);cases=[]
 bad=dict(original,tip='foreign-profile');cases.append(bad)
 bad=dict(original,dll_hash='0'*64);cases.append(bad)
 bad=dict(original,dll=win(root/'Tigirl.dll'));cases.append(bad)
 old=json.loads((BUILD/'previous-install-19466b1647bcaa2b.json').read_text(encoding='utf-8-sig'))
 bad=dict(original,dll=old['dll'],dll_hash=old['hash']);cases.append(bad)
 for i,bad in enumerate(cases):
  path=root/f'invalid-{i}.json';path.write_text(json.dumps(bad));result=check(path);assert result.returncode!=0,(i,result.stdout)
after=check(record);assert after.returncode==0 and json.loads(after.stdout)==plan
report=dict(preflight=plan,rejected_records=len(cases),registration_plan_unchanged=True,uninstall_executed=False)
(BUILD/'uninstall-preflight-arm64.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='preflight'}))
