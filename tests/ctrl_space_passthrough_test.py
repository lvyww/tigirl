"""Ctrl+Space is owned by Windows even when a legacy config enables it."""
import json
from pathlib import Path
import subprocess
import tempfile
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1]
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
checks=0
with tempfile.TemporaryDirectory(prefix='system-chord-',dir=ROOT/'build') as temporary:
    root=Path(temporary)
    selection=root/'selection.txt';selection.write_text('1选 VK_1\n',encoding='utf-8-sig')
    for enabled in ['是','否']:
        for chinese in ['是','否']:
            for composing in [False,True]:
                for release_control_first in [False,True]:
                    config=root/'config.txt';config.write_text(f'默认中文\t{chinese}\nCtrl+空格切换中英文\t{enabled}\n',encoding='utf-8-sig')
                    # Baseline state, then Ctrl+Space down, repeat, and both release orders.
                    rows=[(0,0,0,1)]
                    if composing:rows += [(65,1,0,1),(65,0,0,1),(66,1,0,1),(66,0,0,1)]
                    start=len(rows)
                    rows += [(162,1,1,1),(32,1,1,1),(32,1,1,2)]
                    rows += [(162,0,0,1),(32,0,0,1)] if release_control_first else [(32,0,1,1),(162,0,0,1)]
                    trace=root/'trace.tsv';trace.write_text(''.join(f'{int(i==0)} {vk} 0 {down} 0 {ctrl} 0 0 0 0 {repeat} 0\n' for i,(vk,down,ctrl,repeat) in enumerate(rows)))
                    result=run_windows([ROOT/'build/tests/ARM64/engine_probe.exe',win(ROOT/'data/tiger-v2.tcd'),win(trace),win(selection),win(config)],capture_output=True,text=True,timeout=30)
                    assert result.returncode==0,(result.stdout,result.stderr)
                    states=[json.loads(line) for line in result.stdout.splitlines()]
                    baseline=states[start-1]
                    for state in states[start:]:
                        assert not state['handled'] and not state['cancel'] and not state['commit'],state
                        for key in ['chinese','mode','raw','candidates']:
                            assert state[key]==baseline[key],(key,state,baseline)
                    checks+=1
report=dict(status='passed',cases=checks,legacy_enabled_ignored=True,repeat_and_both_release_orders=True,chinese_english_and_composition_preserved=True)
(ROOT/'build/ctrl-space-passthrough.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
