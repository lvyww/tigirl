"""Observe startup mode of a private, process-local TSF profile without fixup."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
EVIDENCE=ROOT/'build/default-chinese-20261007/startup-probe'
PS='/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
parser=argparse.ArgumentParser()
parser.add_argument('--runtime',type=Path,default=EVIDENCE/'old-runtime')
parser.add_argument('--architecture',choices=['ARM64','x64','Win32'],default='ARM64')
parser.add_argument('--label',default='before')
parser.add_argument('--expect-default',action='store_true')
parser.add_argument('--activation',choices=['natural','neutral','activate-before-foreground'],default='natural')
parser.add_argument('--seed',choices=['seed-zero','unseeded','both'],default='seed-zero')
parser.add_argument('--visibility',choices=['hidden','foreground','both'],default='both')
args=parser.parse_args()
def win(p):return subprocess.check_output(['wslpath','-w',str(p.resolve())],text=True).strip()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def registered():
 command="(Get-Item 'Registry::HKEY_CLASSES_ROOT\\CLSID\\{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}\\InprocServer32').GetValue('')"
 return subprocess.check_output([PS,'-NoProfile','-Command',command],text=True).strip()
out=EVIDENCE/args.label
out.mkdir(parents=True,exist_ok=False)
host=EVIDENCE/'bin'/args.architecture/'tsf_host.exe'
dll=args.runtime/'Tigirl.dll'
manifest=args.runtime/'NativeTiger.StartupProbe.manifest'
manifest.write_text((ROOT/'tests/NativeTiger.Test.manifest').read_text().replace('processorArchitecture="arm64"','processorArchitecture="'+{'ARM64':'arm64','x64':'amd64','Win32':'x86'}[args.architecture]+'"'))
before=registered()
report={'label':args.label,'architecture':args.architecture,'status':'running','dll':str(dll),'dll_sha256':sha(dll),'host_sha256':sha(host),'installed':False,'rows':[]}
failures=[]
try:
 for visibility in (['hidden','foreground'] if args.visibility=='both' else [args.visibility]):
  for seed in (['seed-zero','unseeded'] if args.seed=='both' else [args.seed]):
   for case,text,expected in [('missing-config',None,1),('missing-default','最大码长\t4\n',1),('chinese','默认中文\t是\n',1),('english','默认中文\t否\n',0)]:
    folder=out/(visibility+'-'+seed+'-'+case);folder.mkdir()
    (folder/'.tsf-startup-test').touch()
    config=folder/'config.txt'
    if text is not None:config.write_text(text,encoding='utf-8-sig')
    config_before=config.read_bytes() if config.exists() else None
    command=[str(host),'--startup-mode-probe',win(dll),win(manifest),win(folder),visibility,seed,args.activation]
    result=subprocess.run(command,capture_output=True,text=True,timeout=20)
    (folder/'stdout.json').write_text(result.stdout)
    (folder/'stderr.txt').write_text(result.stderr)
    row={'case':case,'visibility':visibility,'seed':seed,'activation':args.activation,'expected':expected,'returncode':result.returncode,
         'config_unchanged':config_before==(config.read_bytes() if config.exists() else None)}
    if result.returncode==0:
     actual=json.loads(result.stdout)
     row['observation']=actual
     samples=[event for event in actual['events'] if event['event'] in ['returned','sample'] and event['phase'] in ['ActivateProfile','after-activation-pump','after-focus-pump','first-letter-preview']]
     row['checkpoints']=[{'phase':event['phase'],'open':event['open'],'conversion':event['conversion'],'foreground':event['foreground']} for event in samples]
     row['matches_default']=all(event['open']['vt']==3 and event['open']['value']==expected for event in samples)
     row['foreground_acquired']=any(event['foreground'] for event in actual['events'])
     if args.expect_default and not row['matches_default']:failures.append(case+' '+visibility+' '+seed+' default mismatch')
     if args.expect_default and visibility=='foreground' and not row['foreground_acquired']:failures.append(case+' foreground was not acquired')
     if args.expect_default and visibility=='foreground' and actual['first_letter_preview_eaten']!=(expected==1):failures.append(case+' first-letter preview mismatch')
    else:row['error']=result.stderr;failures.append(case+' '+visibility+' '+seed+' host failed')
    if not row['config_unchanged']:failures.append(case+' configuration mutated')
    report['rows'].append(row)
    print(json.dumps({k:v for k,v in row.items() if k not in ['observation']},ensure_ascii=False),flush=True)
finally:
 report['registration_unchanged']=before==registered()
 report['dll_unchanged']=report['dll_sha256']==sha(dll)
 report['status']='failed' if failures else 'observed'
 report['failures']=failures
 (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
assert report['registration_unchanged'] and report['dll_unchanged']
if failures:raise SystemExit('; '.join(failures))
