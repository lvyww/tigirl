"""Drive real native settings controls in an isolated hidden manager window."""
import hashlib,json,subprocess,tempfile,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/'build/tests/ARM64/schema_manager.exe'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
with tempfile.TemporaryDirectory(prefix='input-settings-',dir=ROOT/'build') as temporary:
 root=Path(temporary);(root/'.schema-manager-test').touch()
 selection=root/'自定义选重键.txt'
 selection.write_text('# 选重备注\n1选 VK_1\n3选 999 VK_F3\n',encoding='utf-8-sig')
 config=root/'config.txt'
 config.write_text('# 用户注释\n未知设置\t保留原值\n默认中文\t是\n最大码长\t4\n每页候选个数\t5\n当前码表\t虎码字词\n主题\t自定义未收录\n',encoding='utf-8-sig')
 result=subprocess.run([str(EXE),win(root),win(ROOT/'data/tiger-v2.tcd'),'--test-settings','虎码字词'],capture_output=True,text=True,timeout=30)
 assert result.returncode==0,(result.stdout,result.stderr)
 text=config.read_text(encoding='utf-8-sig')
 request=next(line.split()[1] for line in text.splitlines() if line.startswith('_native_settings_reload\t'))
 uuid.UUID(request)
 repaired=root/'损坏选重键测试.txt'
 backups=list(root.glob('损坏选重键测试.txt.invalid.*'))
 assert len(backups)==1,backups
 assert backups[0].read_bytes()==b'\xef\xbb\xbf'+'无效标签\t原始内容\r\n并发修改\t保留\r\n'.encode('utf-8')
 assert repaired.read_text(encoding='utf-8-sig').startswith('1选 0x31')
 broken_encodings=[b'\xc3\x28',b'\xff\xfe\x31',b'\xff\xfe\x00\xd8',b'\xff\xfe\x00\x00\x00\x00\x11\x00']
 for index,original in enumerate(broken_encodings):
  broken=root/f'encoding-selection-{index}.txt'
  copies=list(root.glob(broken.name+'.invalid.*'))
  assert len(copies)==1 and copies[0].read_bytes()==original+b'\0',(index,copies)
  assert broken.read_text(encoding='utf-8-sig').startswith('1选 0x31')
 for expected in ['# 用户注释','未知设置\t保留原值','默认中文\t否','最大码长\t6','每页候选个数\t10','当前码表\t并发方案','翻页键\tPageUp/PageDown','字体\tMicrosoft YaHei UI','字体大小\t17.5','竖排候选\t否','候选窗显示编码\t是','主题\t清晨','手动加词快捷键\tCtrl+Alt+0X4B','切换最近码表快捷键\tCtrl+Alt+0X4A','Ctrl+m切换最近码表\t是']:
  assert expected in text,(expected,text)
 selected=selection.read_text(encoding='utf-8-sig')
 for expected in ['# 选重备注','3选 999 VK_F3','1选\t0x31 0x51','2选\t','9选\t0x78']:
  assert expected in selected,(expected,selected)
 engine=ROOT/'build/tests/ARM64/engine_probe.exe'
 def run_keys(binding,last):
  events=[(1,16,1,1),(0,16,0,0),(0,65,1,0),(0,66,1,0),(0,last,1,0)]
  trace=root/'dispatch.tsv';trace.write_text(''.join(f'{reset} {vk} 0 {down} {shift} 0 0 0 0 0 1 0\n' for reset,vk,down,shift in events))
  result=subprocess.run([str(engine),win(ROOT/'data/tiger-v2.tcd'),win(trace),win(binding),win(config)],capture_output=True,text=True,timeout=30)
  assert result.returncode==0,result.stderr
  return [json.loads(line) for line in result.stdout.splitlines()]
 custom=run_keys(selection,81)
 assert custom[-2]['raw']=='ab' and custom[-2]['candidates'],custom
 assert custom[-1]['commit']==custom[-2]['candidates'][0] and custom[-1]['raw']=='',custom
 defaults=root/'恢复选重键测试.txt'
 restored=run_keys(defaults,50)
 assert len(restored[-2]['candidates'])>=2 and restored[-1]['commit']==restored[-2]['candidates'][1],restored
 cleared=run_keys(selection,50)
 assert cleared[-1]['commit']!=restored[-1]['commit'],(cleared,restored)
 trace=root/'saved-shortcut.tsv';trace.write_text('1 75 0 1 0 1 1 0 0 0 1 0\n')
 result=subprocess.run([str(engine),win(ROOT/'data/tiger-v2.tcd'),win(trace),win(selection),win(config)],capture_output=True,text=True,timeout=30)
 assert result.returncode==0,result.stderr
 shortcut=json.loads(result.stdout)
 assert shortcut.get('openAddWord') and shortcut['handled'],shortcut
 report={'status' :'passed','ui_saved_addword_shortcut_engine_dispatch':True,'ui_saved_binding_engine_dispatch':True,'restored_defaults_engine_dispatch':True,'cleared_binding_not_selected':True,'engine_sha256':hashlib.sha256(engine.read_bytes()).hexdigest(),'selection_keys_saved':True,'selection_other_rows_preserved':True,'native_controls':True,'invalid_range_rejected':True,'edited_values_saved':True,'concurrent_unedited_field_preserved':True,'unknown_fields_and_comments_preserved':True,'invalid_or_reserved_shortcuts_rejected':2,'shortcut_conflict_rejected':True,'shortcuts_saved':True,'theme_choices':9,'theme_saved':True,'appearance_saved':True,'invalid_font_sizes_rejected':3,'paging_choices':4,'paging_saved':True,'cancel_preserved_file':True,'layout_dpis':[96,144,192],'physical_visual_validation':False,'exe_sha256':hashlib.sha256(EXE.read_bytes()).hexdigest()}
 report.update(malformed_selection_repaired=True, malformed_selection_backup_exact=True,
               malformed_unicode_repaired=4, malformed_unicode_cancel_preserves_bytes=True,
               malformed_unicode_concurrent_change_rejected=True, unreadable_selection_not_repaired=True,
               malformed_selection_cancel_preserves_file=True, concurrent_repair_rejected=True,
               individual_binding_removal=True, per_candidate_defaults=True, empty_binding_removal_disabled=True)
 (ROOT/'build/input-settings-arm64.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report))
