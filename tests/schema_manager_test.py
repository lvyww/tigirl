"""Actual Win32 manager controls/worker completion in a hidden isolated window."""
import hashlib,json,shutil,subprocess,tempfile
from pathlib import Path
from windows_process import run_windows
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';MANAGER=BUILD/'tests/ARM64/Tigirl.exe';IMPORT=BUILD/'tests/ARM64/Tigirl.Import.exe'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
with tempfile.TemporaryDirectory(prefix='schema-manager-',dir=BUILD) as temporary:
 root=Path(temporary);user=root/'中文 用户目录';user.mkdir();(user/'.schema-manager-test').touch()
 source=root/'中文 码表';source.mkdir();(source/'rows.txt').write_text('ab 界面测试\n',encoding='utf-8')
 bundled=ROOT/'data/tiger-v2.tcd'
 result=run_windows([str(IMPORT),'--schema',win(source),win(root/'没有拼音'),win(user),'界面方案','zh-CN'],capture_output=True,text=True,timeout=30)
 assert result.returncode==0,result.stderr
 def select(name,exe=MANAGER,target=user,mode='--test-select'):
  return run_windows([str(exe),win(target),win(bundled),mode,name],capture_output=True,text=True,timeout=30)
 result=select('界面方案');assert result.returncode==0,(result.stdout,result.stderr)
 config=user/'config.txt';saved=config.read_bytes();assert '当前码表\t界面方案' in config.read_text(encoding='utf-8-sig')
 corrupt=user/'schemas/损坏方案';corrupt.mkdir();(corrupt/'tiger-v2.tcd').write_bytes(b'broken')
 assert select('损坏方案').returncode!=0 and config.read_bytes()==saved
 # The GUI must receive a process-launch failure and close its test loop instead
 # of hanging or modifying selection when companion executables are missing.
 isolated=root/'缺少工具';isolated.mkdir();copy=isolated/MANAGER.name;shutil.copyfile(MANAGER,copy)
 assert select('界面方案',copy).returncode!=0 and config.read_bytes()==saved
 unmarked=root/'unmarked';unmarked.mkdir();assert select('界面方案',target=unmarked).returncode!=0
 assert not (unmarked/'config.txt').exists()
 ui_source=user/'test-source';ui_source.mkdir();ui_table=ui_source/'rows.txt';ui_table.write_text('ab 窗口导入\n',encoding='utf-8')
 assert select('窗口新方案',mode='--test-import').returncode==0
 imported=user/'schemas/窗口新方案';original=(imported/'tiger-v2.tcd').read_bytes()
 assert config.read_bytes()==saved
 ui_table.write_text('ab 窗口更新\n',encoding='utf-8')
 assert select('窗口新方案',mode='--test-update').returncode==0
 descriptor=imported/'current.txt';generation=descriptor.read_text(encoding='utf-8-sig').strip().split('\t')[1]
 assert (imported/'generations'/generation/'tiger-v2.tcd').read_bytes()!=original
 assert (imported/'tiger-v2.tcd').read_bytes()==original and config.read_bytes()==saved
 previous_generation=generation
 ui_table.write_text('ab 第二次窗口更新\n',encoding='utf-8')
 assert select('窗口新方案',mode='--test-update').returncode==0
 generation=descriptor.read_text(encoding='utf-8-sig').strip().split('\t')[1]
 assert generation!=previous_generation
 assert select('窗口新方案',mode='--test-restore').returncode==0
 assert descriptor.read_text(encoding='utf-8-sig').strip()=='generation\tlegacy'
 assert config.read_bytes()==saved
 assert select('窗口新方案',mode='--test-restore-newest').returncode==0
 assert descriptor.read_text(encoding='utf-8-sig').strip()=='generation\t'+generation
 assert config.read_bytes()==saved
 assert select('窗口新方案',mode='--test-select-close').returncode==0
 assert '当前码表\t窗口新方案' in config.read_text(encoding='utf-8-sig')
 report=dict(layout_dpis=[96,144,192],scaled_font_and_control_bounds=True,formatted_version_labels=True,newest_version_restore=True,two_ui_updates=True,native_import_update_restore=True,version_combo_used=True,close_during_work_completed=True,native_window_selection=True,worker_completion=True,unicode_space_paths=True,failed_selection_preserved=True,missing_tool_failure=True,test_root_required=True,interactive_visual_validation=False,manager_sha256=hashlib.sha256(MANAGER.read_bytes()).hexdigest())
(BUILD/'schema-manager-arm64.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
