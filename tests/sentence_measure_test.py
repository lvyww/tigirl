"""Real-scheme resource sharing and exploratory decoder cost, not installed TSF latency."""
import hashlib,json,shutil,statistics,subprocess,tempfile
from pathlib import Path
from windows_process import run_windows,PS
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/real-sentence-measure';OUT.mkdir(exist_ok=True)
REFERENCE=Path('/mnt/c/Users/yc/Desktop/bime_codex_src_20260513/release_arm64')
model=REFERENCE/'Models/sentence-ngram-v2.bin';source=REFERENCE/'码表/虎整句'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def run(args):
 r=run_windows(args,capture_output=True,text=True,timeout=180)
 assert r.returncode==0,(r.stdout,r.stderr)
 return r.stdout
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
stage=Path(tempfile.mkdtemp(prefix='run-',dir=OUT));tables=stage/'source';tables.mkdir()
source_hashes={}
for p in source.glob('*.txt'):shutil.copyfile(p,tables/p.name);source_hashes[p.name]=sha(p)
run([ROOT/'build/tests/ARM64/Tigirl.Import.exe','--schema',win(tables),win(stage/'pinyin'),win(stage/'user'),'虎整句','zh-CN'])
dictionary=stage/'user/schemas/虎整句/tiger-v2.tcd';probe=ROOT/'build/tests/ARM64/sentence_measure_probe.exe'
runs=[]
for index in range(3):
 output=run([probe,win(dictionary),win(model),'--bench']);(stage/f'bench-{index}.jsonl').write_text(output)
 runs.append([json.loads(s) for s in output.splitlines()])
output=run([probe,win(dictionary),win(model),'--contexts']);(stage/'contexts.jsonl').write_text(output)
contexts=[json.loads(s) for s in output.splitlines()]
run([PS,'-NoProfile','-ExecutionPolicy','Bypass','-File',win(ROOT/'tests/measure_sentence_memory.ps1'),'-Mixed','-Dictionary',win(dictionary),'-Model',win(model)])
memory=json.loads((OUT/'memory-mixed.json').read_text(encoding='utf-8-sig'))
full=[]
for length in [8,16,32,64,128]:
 samples=[next(s for s in r if s.get('raw_length')==length) for r in runs]
 full.append({'raw_length':length,'median_ms':statistics.median(s['ms'] for s in samples),'median_process_private':statistics.median(s['private_after'] for s in samples)})
report={'status':'measured','scope':'Read-only resource mappings across ARM64/x64/x86 processes; optimized standalone ARM64 decoder, real scheme, warm filesystem cache; not installed application latency','stage':str(stage.relative_to(ROOT)),'source':str(source),'source_sha256':source_hashes,'model_sha256':sha(model),'memory':memory,'full_decode':full,'incremental':[r[-1] for r in runs],'contexts':contexts,'installed':False,'latency_acceptable':False,'limitations':['single primary-code workload','allocator retention included in private bytes','no TSF edit-session scheduling time','long-input memory and commit-key latency need improvement']}
paths=[ROOT/'native/SentenceDecoder.cpp',ROOT/'native/SentenceResources.cpp',ROOT/'tests/sentence_measure_probe.cpp',ROOT/'tests/SentenceMeasureProbe.vcxproj',ROOT/'tests/measure_sentence_memory.ps1',ROOT/'tests/sentence_measure_test.py']+[ROOT/'build/tests'/p/'sentence_measure_probe.exe' for p in ['ARM64','x64','Win32']]
report['sha256']={str(p.relative_to(ROOT)):sha(p) for p in paths}
(OUT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'status':'measured','full_decode':full,'incremental_p95_ms':[r[-1]['p95_ms'] for r in runs],'report':str(OUT/'validation.json')},ensure_ascii=False))
