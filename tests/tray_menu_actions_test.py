"""Exercise companion menu actions with an isolated user root and source table."""
import hashlib,json,os,subprocess,uuid
from pathlib import Path
from windows_process import PS,run_windows
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
PACKAGE=BUILD/'ARM64X/ARM64EC/Release'
def win(p):return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def quote(s):return "'"+str(s).replace("'","''")+"'"
root=BUILD/('tray-menu-actions-'+uuid.uuid4().hex);root.mkdir()
source=root/'source/测试方案';source.mkdir(parents=True)
(source/'词条.txt').write_text('ab 原词 次词\ncd 显示<commit>上屏\n',encoding='utf-8')
config=root/'config.txt'
config.write_text('码表存储位置\t'+win(source.parent)+'\nCtrl+空格切换中英文\t否\n主题\t默认\n',encoding='utf-8-sig')
results=[]
def action(name,value='-'):
 cmd='$env:NATIVE_TIGER_USER_ROOT='+quote(win(root))+'; & '+' '.join(map(quote,[win(PACKAGE/'Tigirl.exe'),'--menu-action',name,value]))+' | Out-String -Stream; exit $LASTEXITCODE'
 r=subprocess.run(['/init',PS,'-NoProfile','-Command',cmd],capture_output=True,text=True,timeout=60)
 results.append(dict(action=name,value=value,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
 assert r.returncode==0,results[-1]
action('theme','清晨');text=config.read_text(encoding='utf-8-sig');assert '主题\t清晨' in text and 'Ctrl+空格切换中英文\t否' in text
initial=(source/'词条.txt').read_bytes()
action('use','测试方案');assert '当前码表\t测试方案' in config.read_text(encoding='utf-8-sig')
def dictionary():
 descriptor=root/'schemas/测试方案/current.txt'
 if descriptor.exists():return descriptor.parent/'generations'/descriptor.read_text(encoding='utf-8-sig').strip().split('\t')[1]/'tiger-v2.tcd'
 return root/'schemas/测试方案/tiger-v2.tcd'
first=dictionary()
journal=root/'schemas/测试方案/user.tcu'
r=run_windows([BUILD/'tests/ARM64/user_store_probe.exe',win(first),win(journal),'write','zz','menu-word-',1],capture_output=True,text=True,timeout=30);assert r.returncode==0,r.stderr
saved=journal.read_bytes()
(source/'词条.txt').write_text('ab 更新词 次词\n',encoding='utf-8')
action('reload');assert dictionary()!=first,(str(first),str(dictionary()));assert first.exists();assert journal.read_bytes()==saved
action('export');outputs=list((root/'码表导出').glob('*.txt'));assert len(outputs)==1
export=outputs[0].read_text(encoding='utf-8-sig');assert '更新词' in export and '原词' not in export and 'zz menu-word-0' in export
assert journal.read_bytes()==saved
report=dict(status='passed',results=results,artifacts=str(root),export=str(outputs[0]),configuration_preserved=True,user_words_preserved=True,manager_sha256=hashlib.sha256((PACKAGE/'Tigirl.exe').read_bytes()).hexdigest(),installed=False)
(BUILD/'tray-menu-actions.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
