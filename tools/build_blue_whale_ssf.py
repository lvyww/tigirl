#!/usr/bin/env python3
"""Package the blue-whale skin without altering its artwork."""
from pathlib import Path
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def build(destination=None):
    directory = Path(destination) if destination else ROOT / '皮肤'
    directory.mkdir(parents=True, exist_ok=True)
    artwork = (ROOT / 'assets/skins/blue-whale/panel-narrow.png').read_bytes()
    width, height = struct.unpack('>II', artwork[16:24])
    # Center strips preserve the character and corner bows while the panel grows.
    horizontal = f'0,270,{width - 271}'
    vertical = f'0,812,{height - 813}'
    top, bottom, left, right = 770, height - 924 + 65, 250, 320
    lines = ['[General]', 'skin_name=蓝鲸雪语', 'skin_author=虎娘',
             'preview_comp=panel.png', '[Display]', 'font_size=200',
             'font_ch=Microsoft YaHei UI', 'font_en=Microsoft YaHei UI',
             'pinyin_color=0x79513b', 'zhongwen_first_color=0xa85326',
             'zhongwen_color=0x624433', 'comphint_color=0x927b68',
             'candidate_spacing=120', 'line_spacing=24', 'character_spacing=0']
    for section in ('Scheme_H1', 'Scheme_V1'):
        lines += [f'[{section}]', 'pic=panel.png',
                  f'layout_horizontal={horizontal}', f'layout_vertical={vertical}',
                  f'pinyin_marge={top},32,{left},{right}',
                  f'zhongwen_marge=32,{bottom},{left},{right}']
    for section in ('Scheme_H2', 'Scheme_V2'):
        lines.append(f'[{section}]')
        for prefix in ('pinyin', 'zhongwen'):
            lines += [f'{prefix}_pic=panel.png',
                      f'{prefix}_layout_horizontal={horizontal}',
                      f'{prefix}_layout_vertical={vertical}',
                      f'{prefix}_marge={top},{bottom},{left},{right}']
    output = directory / '蓝鲸雪语.ssf'
    with zipfile.ZipFile(output, 'w') as archive:
        for name, content in [('skin.ini', ('\n'.join(lines)+'\n').encode('utf-8-sig')),
                              ('panel.png', artwork)]:
            info = zipfile.ZipInfo(name, (2026, 10, 7, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, content)
    assert output.stat().st_size < 4 * 1024 * 1024
    return output


if __name__ == '__main__':
    import sys
    print(build(sys.argv[1] if len(sys.argv) > 1 else None))
