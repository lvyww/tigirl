"""Drive real native settings controls in an isolated hidden manager window."""
import hashlib,json,subprocess,tempfile,uuid
from PIL import Image
from windows_process import run_windows, PS
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/'build/tests/ARM64/input_settings_probe.exe'
def win(path):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
with tempfile.TemporaryDirectory(prefix='input-settings-',dir=ROOT/'build') as temporary:
 root=Path(temporary);(root/'.schema-manager-test').touch()
 selection=root/'自定义选重键.txt'
 selection.write_text('# 选重备注\n1选 VK_1\n3选 999 VK_F3\n',encoding='utf-8-sig')
 config=root/'config.txt'
 # Explicitly start disabled so the UI exercise still verifies saving an opt-in.
 config.write_text('跟随皮肤字体\t否\n整句自动提前上屏\t否\n# 用户注释\n未知设置\t保留原值\n默认中文\t是\n最大码长\t4\n每页候选个数\t5\n当前码表\t虎码字词\n皮肤\t自定义未收录.ssf\n',encoding='utf-8-sig')
 result=run_windows([PS,'-NoProfile','-ExecutionPolicy','Bypass','-File',win(ROOT/'tests/settings_capture_desktop.ps1'),'-Exe',win(EXE),'-UserRoot',win(root),'-Dictionary',win(ROOT/'data/tiger-v2.tcd')],capture_output=True,text=True,errors="replace",timeout=180)
 for capture_path in root.glob('*.bmp'):
  with Image.open(capture_path) as capture:capture.convert('RGB').save(ROOT/'build'/capture_path.with_suffix('.png').name)
 assert result.returncode==0,(result.stdout,result.stderr,(root/"error.txt").read_text() if (root/"error.txt").exists() else "")
 # Scrolling must never draw page controls over the fixed tabs / action area.
 for page in range(5):
  with Image.open(root/f'small-{page}-top.bmp') as top, Image.open(root/f'small-{page}-bottom.bmp') as bottom:
   assert top.crop((0,0,top.width,188)).tobytes()==bottom.crop((0,0,bottom.width,188)).tobytes(),(page,'tabs changed while scrolling')
   assert top.crop((0,top.height-120,top.width,top.height)).tobytes()==bottom.crop((0,bottom.height-120,bottom.width,bottom.height)).tobytes(),(page,'footer changed while scrolling')
 font_catalog=(root/'font-catalog.tsv').read_text().splitlines()
 assert font_catalog and font_catalog[0].startswith('#') and len(font_catalog)>3,font_catalog
 for dpi in [96,120,144,192]:
  with Image.open(root/f'donation-settings-{dpi}.bmp') as capture:
   capture.convert('RGB').save(ROOT/f'build/donation-settings-{dpi}.png')
  with Image.open(root/f'input-settings-{dpi}.bmp') as capture:
   capture.convert('RGB').save(ROOT/f'build/input-settings-{dpi}.png')
  with Image.open(root/f'font-dropdown-{dpi}.bmp') as capture:
   capture.convert('RGB').save(ROOT/f'build/font-dropdown-{dpi}.png')
  with Image.open(root/f'font-settings-{dpi}.bmp') as capture:
   capture.convert('RGB').save(ROOT/f'build/font-settings-{dpi}.png')
  with Image.open(root/f'sentence-settings-{dpi}.bmp') as capture:
   capture.convert('RGB').save(ROOT/f'build/sentence-settings-{dpi}.png')
 text=config.read_text(encoding='utf-8-sig')
 for expected in ['自动启用整句模式\t否','整句自动提前上屏\t是','自动选重最低码数\t0','保留最少编码数量\t32','高频字仅使用最优码组句\t0','整句允许全码组句白名单\t\n']:
  assert expected in text,(expected,text)
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
 for expected in ['编码伪装\t甲😀乙','延时显示候选(毫秒)\t250','延时展开注释和拆分(毫秒)\t60000','# 用户注释','未知设置\t保留原值','默认中文\t否','最大码长\t6','每页候选个数\t10','当前码表\t并发方案','翻页键\tPageUp/PageDown','字体\tSegoe UI','字体大小\t17.5','候选布局\t2','皮肤\t默认.ssf','跟随皮肤字体\t否','手动加词快捷键\tCtrl+Alt+0X4B','切换最近码表快捷键\tCtrl+Alt+0X4A','Ctrl+m切换最近码表\t是']:
  assert expected in text,(expected,text)
 selected=selection.read_text(encoding='utf-8-sig')
 for expected in ['# 选重备注','3选 999 VK_F3','1选\t0x31 0x51','2选\t','9选\t0x78']:
  assert expected in selected,(expected,selected)
 engine=ROOT/'build/tests/ARM64/engine_probe.exe'
 def run_keys(binding,last):
  events=[(1,16,1,1),(0,16,0,0),(0,65,1,0),(0,66,1,0),(0,last,1,0)]
  trace=root/'dispatch.tsv';trace.write_text(''.join(f'{reset} {vk} 0 {down} {shift} 0 0 0 0 0 1 0\n' for reset,vk,down,shift in events))
  # This legacy console probe accepts UTF-8 argv; use an ASCII alias under a non-UTF-8 Windows ACP.
  alias=root/'selection-engine.txt';alias.write_bytes(binding.read_bytes())
  result=run_windows([str(engine),win(ROOT/'data/tiger-v2.tcd'),win(trace),win(alias),win(config)],capture_output=True,text=True,timeout=30)
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
 result=run_windows([str(engine),win(ROOT/'data/tiger-v2.tcd'),win(trace),win(root/'selection-engine.txt'),win(config)],capture_output=True,text=True,timeout=30)
 assert result.returncode==0,result.stderr
 shortcut=json.loads(result.stdout)
 assert shortcut.get('openAddWord') and shortcut['handled'],shortcut
 report={'status' :'passed','ui_saved_addword_shortcut_engine_dispatch':True,'ui_saved_binding_engine_dispatch':True,'restored_defaults_engine_dispatch':True,'cleared_binding_not_selected':True,'engine_sha256':hashlib.sha256(engine.read_bytes()).hexdigest(),'selection_keys_saved':True,'selection_other_rows_preserved':True,'native_controls':True,'invalid_range_rejected':True,'edited_values_saved':True,'concurrent_unedited_field_preserved':True,'unknown_fields_and_comments_preserved':True,'invalid_or_reserved_shortcuts_rejected':2,'shortcut_conflict_rejected':True,'shortcuts_saved':True,'skin_choices_checked':True,'skin_saved_reopened':True,'legacy_font_preference_preserved_and_skin_animation_saved':True,'appearance_saved':True,'invalid_font_sizes_rejected':3,'paging_choices':4,'paging_saved':True,'cancel_preserved_file':True,'layout_dpis':[96,120,144,192],'physical_visual_validation':False,'exe_sha256':hashlib.sha256(EXE.read_bytes()).hexdigest()}
 report.update(all_moved_flags_saved_reopened=True,long_multiline_whitelist_saved_reopened=True,settings_tab_stops_disabled=True,hover_tips_registered=True,button_internal_motion_no_repaint=True,buffered_checkbox_click_toggles=True,text_boxes_centered_all_dpis=True,native_edit_entry_backspace_undo=True,old_sunken_edit_frames_removed=True,high_contrast_palette_branch=True,actual_system_high_contrast_toggled=False,dependencies_preserve_values=True,scroll_reachability=True,fixed_window_scroll_reachability=True,error_field_focus=True,preview_no_disk_write=True,unmodified_save_no_write=True,animation_saved_reopened=True,code_mask_saved_reopened_cancelled=True,reveal_delays_saved_reopened=True,reveal_delay_invalid_rejected=8,reveal_delay_cancel_preserved=True,malformed_selection_repaired=True, malformed_selection_backup_exact=True,
               isolated_capture_desktop=True, input_desktop_switched=False,
               capture_method='WM_PRINT of actual controls on a separate, inactive desktop',
               rendered_sentence_input_fields=True,
               font_catalog_count=len(font_catalog),bundled_fonts_first=True,
               font_aliases_verified=True,missing_font_fallback_verified=True,
               font_preview_fixed_size=True,font_dropdown_captured=True,
               sentence_settings_saved_reopened=True, sentence_empty_whitelist_preserved=True,
               sentence_invalid_values_rejected=12, auto_select_default=3, auto_select_zero_saved_reopened=True, auto_select_invalid_values_rejected=4, sentence_cancel_preserved=True,
               malformed_unicode_repaired=4, malformed_unicode_cancel_preserves_bytes=True,
               malformed_unicode_concurrent_change_rejected=True, unreadable_selection_not_repaired=True,
               malformed_selection_cancel_preserves_file=True, concurrent_repair_rejected=True,
               individual_binding_removal=True, per_candidate_defaults=True, empty_binding_removal_disabled=True)
 (ROOT/'build/input-settings-arm64.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report))
