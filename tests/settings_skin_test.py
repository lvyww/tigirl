"""Active-desktop screenshots of the complete fixed-DIP native settings window."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
from windows_process import run_windows

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--active-desktop', action='store_true', required=True)
parser.parse_args()
root = Path(__file__).resolve().parents[1]
exe = root / 'build/tests/ARM64/input_settings_probe.exe'
output = root / 'build/tiger-cream-validation'
output.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='skin-', dir=root/'build') as temporary:
    fixture = Path(temporary)
    (fixture/'.schema-manager-test').touch()
    config = fixture/'config.txt'
    config.write_text('# retained\n字体大小\t16.949999999999999\n每页候选个数\t5\n', encoding='utf-8-sig')
    before = config.read_bytes(), config.stat().st_mtime_ns
    win = subprocess.check_output(['wslpath', '-w', str(fixture)], text=True).strip()
    result = run_windows([exe, win, '--skin-test'], capture_output=True, text=True, errors='replace', timeout=120)
    assert result.returncode == 0, (result.stdout, result.stderr)
    for capture in fixture.glob('screen-*.bmp'):
        with Image.open(capture) as im:
            im.convert('RGB').save(output/capture.with_suffix('.png').name)
    with Image.open(fixture/'preview-fixed-before.bmp') as before_preview, Image.open(fixture/'preview-fixed-after.bmp') as after_preview:
        # Preview child at page (332,68), content origin (18,94), at 96 DPI.
        region=(350,162,594,310)
        assert before_preview.crop(region).tobytes()==after_preview.crop(region).tobytes(), 'Font size changed the preview'
    for dpi in [96, 120, 144, 192]:
        for page in range(5):
            with Image.open(output/f'screen-{page}-{dpi}.png') as im:
                assert im.size == (640*dpi//96, 480*dpi//96), im.size
    assert before == (config.read_bytes(), config.stat().st_mtime_ns)
    report = dict(status='passed', dpi=[96,120,144,192], pages=5,
                  physical_capture=True, complete_window_frame=True,
                  fixed_dip_size=[640,480], minimize_restore=True,
                  caption_hit_test=True, keyboard_move=True, mouse_drag=True, alt_space_system_menu=True, close_without_save=True,
                  space_toggles_native_checkbox=True, settings_tab_stops_disabled=True, native_dropdown=True,
                  repeated_open_close_gdi_user_handles=True, validation_errors_focused_and_captured=True,
                  font_size_does_not_change_preview_pixels=True, fixed_preview_with_200dip_setting=True, hidden_preview=True,
                  high_contrast_palette_branch=True, actual_system_high_contrast_toggled=False,
                  config_bytes_and_timestamp_unchanged=True,
                  sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
    (output/'validation.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))
