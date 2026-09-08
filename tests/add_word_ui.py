"""Drive only this probe's native dialog controls and a disposable word journal."""
import json,subprocess,tempfile,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
exe=ROOT/'build/tests/ARM64/add_word_ui_probe.exe'
with tempfile.TemporaryDirectory(prefix='add-word-ui-',dir=ROOT/'build') as temp:
 result=subprocess.run([str(exe),win(ROOT/'data/tiger-v2.tcd'),win(Path(temp)/'words.tcu'),win(ROOT/'build/add-word-dialog.bmp')],text=True,capture_output=True,timeout=30)
 print(result.stdout,result.stderr);result.check_returncode()
 report=json.loads(result.stdout);report['probe_sha256']=hashlib.sha256(exe.read_bytes()).hexdigest()
 (ROOT/'build/add-word-ui-arm64.json').write_text(json.dumps(report,indent=2)+'\n')
