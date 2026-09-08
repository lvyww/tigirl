"""Real ARM64 readers retain old mappings while concurrent importers publish."""
import concurrent.futures,hashlib,json,selectors,subprocess,tempfile,threading,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
IMPORT=BUILD/'tests/ARM64/lexicon_import.exe';READER=BUILD/'tests/ARM64/schema_generation_reader.exe'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
def hextext(text):return ''.join(f'{ord(c):04x}' for c in text)
def receive(process):
 with selectors.DefaultSelector() as watcher:
  watcher.register(process.stdout,selectors.EVENT_READ)
  if not watcher.select(10):raise RuntimeError('Native reader response timeout')
  line=process.stdout.readline().strip()
  if not line:raise RuntimeError('Native reader terminated')
  return line
with tempfile.TemporaryDirectory(prefix='generation-live-',dir=BUILD) as temporary:
 root=Path(temporary);user=root/'user';missing=root/'missing';schema=user/'schemas/方案'
 sources=[]
 for i in range(7):
  directory=root/f'source{i}';directory.mkdir();(directory/'rows.txt').write_text('ab '+('旧词' if i==0 else f'新词{i}')+'\n',encoding='utf-8');sources.append(directory)
 # Resolve all paths before starting concurrent work so process-launch overhead
 # from wslpath does not serialize the experiment.
 userwin=win(user);missingwin=win(missing);sourcewin=[win(p) for p in sources];schemawin=win(schema)
 def publish(i):
  result=subprocess.run([str(IMPORT),'--schema' if i==0 else '--update',sourcewin[i],missingwin,userwin,'方案','zh-CN'],capture_output=True,text=True,timeout=30)
  assert result.returncode==0,result.stderr
  return i
 publish(0);legacy=(schema/'tiger-v2.tcd').read_bytes()
 readers=[];done=threading.Event();start=threading.Event();allowed={hextext('旧词')}|{hextext(f'新词{i}') for i in range(1,7)}
 try:
  for _ in range(4):
   process=subprocess.Popen([str(READER),schemawin],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=1)
   readers.append(process);assert receive(process)=='ready '+hextext('旧词')
  def read_loop(process):
   start.wait();seen=set();count=0;deadline=time.monotonic()+30
   while not done.is_set() or count<200:
    if time.monotonic()>deadline:raise RuntimeError('Concurrent update deadline exceeded')
    process.stdin.write('read\n');process.stdin.flush();old,current=receive(process).split()
    assert old==hextext('旧词') and current in allowed,(old,current)
    seen.add(current);count+=1
   process.stdin.write('read\n');process.stdin.flush();old,current=receive(process).split()
   assert old==hextext('旧词') and current!=old and current in allowed
   return dict(reads=count+1,observed=sorted(seen),final=current)
  with concurrent.futures.ThreadPoolExecutor(max_workers=10) as pool:
   futures=[pool.submit(read_loop,p) for p in readers]
   start.set();writers=[pool.submit(publish,i) for i in range(1,7)]
   try:published=[future.result(timeout=35) for future in writers]
   finally:done.set()
   results=[future.result(timeout=35) for future in futures]
  assert len({result['final'] for result in results})==1
  assert (schema/'tiger-v2.tcd').read_bytes()==legacy
  versions=list((schema/'generations').glob('*/tiger-v2.tcd'));assert len(versions)==6
  assert not list(schema.rglob('.tiger-*.tmp'))
 finally:
  done.set();start.set()
  for process in readers:
   try:
    if process.poll() is None:process.stdin.write('quit\n');process.stdin.flush()
    process.wait(timeout=5)
   except (BrokenPipeError,subprocess.TimeoutExpired):process.kill();process.wait()
   assert process.returncode==0,process.stderr.read()
 report=dict(readers=4,publishers=6,published_generations=len(versions),results=results,legacy_mapping_stable=True,whole_generation_reads=True,reader_sha256=hashlib.sha256(READER.read_bytes()).hexdigest(),import_sha256=hashlib.sha256(IMPORT.read_bytes()).hexdigest())
(BUILD/'schema-generation-concurrency-arm64.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
