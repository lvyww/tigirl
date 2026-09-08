"""Use original StringInfo elements recorded by add_word_parity.py per commit."""
import json,subprocess,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; BUILD=ROOT/'build'
rows=[json.loads(x) for x in (BUILD/'add-word-oracle.jsonl').read_text(encoding='utf-8-sig').splitlines()]
assert len(rows)==633
actions=[];expected=[];stack=[]
for row in rows:
    actions.append(row['text'].encode().hex());stack+=row['elements'];expected.append(stack[-20:])
for _ in range(len(stack)+2):
    actions.append('-')
    if stack: stack.pop()
    expected.append(stack[-20:])
trace=BUILD/'history-actions.txt'; trace.write_text('\n'.join(actions)+'\n')
for arch,exe in [('linux',BUILD/'history_probe'),('arm64',BUILD/'tests/ARM64/history_probe.exe')]:
    path=subprocess.check_output(['wslpath','-w',str(trace)],text=True).strip() if arch=='arm64' else str(trace)
    output=subprocess.check_output([str(exe),path],text=True,timeout=30)
    actual=[[bytes.fromhex(x).decode() for x in line.split()] for line in output.splitlines()]
    assert len(actual)==len(expected)
    failures=[dict(index=i,expected=a,actual=b) for i,(a,b) in enumerate(zip(expected,actual)) if a!=b]
    report=dict(commits=len(rows),actions=len(actions),mismatches=len(failures),failures=failures[:3],probe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
    (BUILD/f'history-parity-{arch}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(arch,report);assert not failures
