"""Opt-in active-desktop regression: incremental scrolling must match a full repaint.

WM_PRINT reconstructs controls and cannot detect stale pixels already on screen.
This test briefly displays a topmost fixture window and captures only that window.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
from windows_process import run_windows

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'build/tests/ARM64/input_settings_probe.exe'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--active-desktop', action='store_true', required=True,
                    help='Allow a temporary visible settings fixture on the current desktop')
parser.parse_args()

def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()

output = ROOT / 'build/settings-scroll-capture'
output.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='settings-scroll-', dir=ROOT / 'build') as temporary:
    root = Path(temporary)
    (root / '.schema-manager-test').touch()
    config = root / 'config.txt'
    config.write_text('最大码长\t4\n每页候选个数\t5\n整句自动提前上屏\t是\n', encoding='utf-8-sig')
    original, timestamp = config.read_bytes(), config.stat().st_mtime_ns
    result = run_windows([EXE, win(root), '--scroll-test'], capture_output=True,
                         text=True, errors='replace', timeout=90)
    differences = {}
    for page in [3, 1, 2, 0]:
        count = root / f'scroll-{page}.txt'
        if count.exists():
            differences[page] = int(count.read_text())
        for phase in ['before', 'after']:
            capture = root / f'scroll-{page}-{phase}.bmp'
            if capture.exists():
                with Image.open(capture) as image:
                    rgb = image.convert('RGB')
                    assert len(rgb.getcolors(rgb.width * rgb.height)) > 64, 'Blank screen capture'
                    rgb.save(output / capture.with_suffix('.png').name)
    report = dict(status='passed' if result.returncode == 0 else 'failed',
                  actual_screen_capture=True, captures_repainted_with_WM_PRINT=False,
                  excluded_region='pixels outside GetWindowRgn (tiger ears and rounded body)',
                  changed_pixels= differences,
                  config_unchanged=config.read_bytes() == original and config.stat().st_mtime_ns == timestamp,
                  probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
    (output / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
    assert result.returncode == 0, (result.stdout, result.stderr, differences)
    assert len(differences) == 4 and all(value == 0 for value in differences.values())
    assert report['config_unchanged']
    print(json.dumps(report))
