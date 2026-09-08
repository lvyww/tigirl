"""Read-only package preflight and isolated negative-package fixtures."""
import hashlib,json,shutil,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';PACKAGE=BUILD/'ARM64/Release'
PS='/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
def check(script):return subprocess.run([PS,'-NoProfile','-ExecutionPolicy','Bypass','-File',win(script),'-CheckOnly'],capture_output=True,text=True,timeout=30)
result=check(ROOT/'install_arm64.ps1');assert result.returncode==0,result.stderr
package=json.loads(result.stdout)
for name in ['schema_manager.exe','schema_select.exe','lexicon_import.exe','timer_reminder.exe']:
 assert hashlib.sha256((PACKAGE/name).read_bytes()).hexdigest()==package['artifacts'][name].lower()
 assert (PACKAGE/name).read_bytes()==(BUILD/'tests/ARM64'/name).read_bytes()
with tempfile.TemporaryDirectory(prefix='package-preflight-',dir=BUILD) as temporary:
 root=Path(temporary);staged=root/'build/ARM64/Release';shutil.copytree(PACKAGE,staged)
 script=root/'install_arm64.ps1';shutil.copyfile(ROOT/'install_arm64.ps1',script)
 shutil.copyfile(ROOT/'shortcut_arm64.ps1',root/'shortcut_arm64.ps1')
 manager=staged/'schema_manager.exe';original=manager.read_bytes();bad=bytearray(original)
 pe=struct.unpack_from('<I',bad,0x3c)[0];struct.pack_into('<H',bad,pe+4,0x8664);manager.write_bytes(bad)
 result=check(script);assert result.returncode!=0 and 'not ARM64' in result.stderr
 manager.unlink();result=check(script);assert result.returncode!=0 and 'Missing build artifact' in result.stderr
 manager.write_bytes(original)
 reminder=staged/'timer_reminder.exe';reminder_bytes=reminder.read_bytes();reminder.unlink()
 result=check(script);assert result.returncode!=0 and 'Missing build artifact' in result.stderr
 reminder.write_bytes(reminder_bytes)
 user=root/'用户 配置';user.mkdir();(user/'.schema-manager-test').touch()
 # Test the same path resolution used by no-argument startup with an isolated
 # override. Use the bundled default scheme so no prepared external schema is needed.
 def psquote(value):return "'"+value.replace("'","''")+"'"
 command='$env:NATIVE_TIGER_USER_ROOT='+psquote(win(user))+'; $managerProcess=Start-Process -FilePath '+psquote(win(manager))+" -ArgumentList @('--test-default','虎码字词') -PassThru -Wait; exit $managerProcess.ExitCode"
 result=subprocess.run([PS,'-NoProfile','-Command',command],capture_output=True,text=True,timeout=30)
 assert result.returncode==0,(result.stdout,result.stderr)
 assert '当前码表\t虎码字词' in (user/'config.txt').read_text(encoding='utf-8-sig')
 report=dict(package=package,management_tools_included=True,reminder_included=True,missing_reminder_rejected=True,wrong_architecture_rejected=True,missing_tool_rejected=True,default_path_window_selection=True,installation_performed=False)
(BUILD/'native-package-preflight.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='package'}))
