"""Unicode masking and shared preedit/candidate display semantics."""
import hashlib
import json
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
sources=['tests/code_mask_probe.cpp','native/CandidatePresentation.cpp','native/Settings.cpp','native/Text.cpp','native/SelectionKeys.cpp']
exe=ROOT/'build/code_mask_probe'
subprocess.run(['g++','-std=c++17','-O2','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I',str(ROOT/'native'),*[str(ROOT/p) for p in sources],'-o',str(exe)],check=True)
result=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
report={'status':'passed','stdout':result.stdout,'physical_input_tested':False,'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sources+['native/CodeMask.h','native/CandidatePresentation.h']}}
(ROOT/'build/code-mask-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(result.stdout,end='')
