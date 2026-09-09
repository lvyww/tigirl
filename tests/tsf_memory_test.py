"""Measure live private TSF activations sharing the actual dictionary."""
import hashlib,json,selectors,subprocess,tempfile,sys
from pathlib import Path
from tsf_architectures import variants
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
EXE=BUILD/'tests/ARM64/tsf_host.exe';DLL=BUILD/'ARM64/Release/Tigirl.dll'
ACTIVE='--active' in sys.argv
ARM64X='--arm64x' in sys.argv
COUNT=2 if '--pair' in sys.argv else 4
DLL,HOSTS=variants(ARM64X)
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
def receive(process):
 with selectors.DefaultSelector() as watcher:
  watcher.register(process.stdout,selectors.EVENT_READ)
  if not watcher.select(30):raise RuntimeError('TSF process did not respond within the observation window')
  line=process.stdout.readline().strip()
  if not line:raise RuntimeError('TSF host exited: '+process.stderr.read())
  return line
with tempfile.TemporaryDirectory(prefix='tsf-memory-',dir=BUILD) as temporary:
 readers=[];measurements=[]
 try:
  for i in range(COUNT):
   platform,exe,manifest=HOSTS[i%len(HOSTS)]
   root=Path(temporary)/str(i);root.mkdir();(root/'.tsf-memory-test').touch()
   process=subprocess.Popen([str(exe),win(DLL),'-',win(manifest),'--memory-active' if ACTIVE else '--memory',win(root)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=1)
   readers.append(process);assert receive(process)=='ready'
  for process in readers:process.stdin.write('measure\n');process.stdin.flush()
  for i,process in enumerate(readers):
   measured=json.loads(receive(process));measured['host_architecture']=HOSTS[i%len(HOSTS)][0];measurements.append(measured)
  for result in measurements:
   assert result['dictionary_pages']>10000,result
   assert result['resident_pages']==result['dictionary_pages'],result
   assert result['multiply_shared_pages']==result['dictionary_pages'],result
  for process in readers:process.stdin.write('close\n');process.stdin.flush()
  for process in readers:
   assert process.wait(timeout=15)==0,process.stderr.read()
  report={'status':'passed','actual_tsf_activations':COUNT,'phase':'explicit TSF callbacks and UI-less candidates' if ACTIVE else 'idle activation with all dictionary pages touched','input_callbacks_measured':ACTIVE,'physical_keyboard_or_rendering_measured':False,'measurements':measurements,'dll_sha256':hashlib.sha256(DLL.read_bytes()).hexdigest(),'host_sha256':hashlib.sha256(EXE.read_bytes()).hexdigest()}
  report['host_hashes']={platform:hashlib.sha256(exe.read_bytes()).hexdigest() for platform,exe,_ in HOSTS}
  report['architecture_mix']=[HOSTS[i%len(HOSTS)][0] for i in range(COUNT)]
  (BUILD/('tsf-memory'+('-active' if ACTIVE else '')+('-pair' if COUNT==2 else '')+('-arm64x.json' if ARM64X else '-arm64.json'))).write_text(json.dumps(report,indent=2)+'\n')
  print(json.dumps(report))
 finally:
  for process in readers:
   if process.poll() is None:
    process.stdin.close()
    try:process.wait(timeout=5)
    except subprocess.TimeoutExpired:process.kill();process.wait()
