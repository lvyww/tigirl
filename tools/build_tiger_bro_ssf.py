#!/usr/bin/env python3
"""Package the tiger-bro reference-inspired skin without altering its artwork."""
from pathlib import Path
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def build(destination=None):
    directory = Path(destination) if destination else ROOT / '皮肤'
    directory.mkdir(parents=True, exist_ok=True)
    artwork = (ROOT / 'assets/skins/tiger-bro/panel-narrow.png').read_bytes()
    width, height = struct.unpack('>II', artwork[16:24])
    # Plain center strips avoid the torso and the tiger-stripe corner accents.
    horizontal = f'0,260,{width - 261}'
    vertical = f'0,855,{height - 856}'
    top, bottom, left, right = 890, height - 900 + 90, 250, 180
    lines = ['[General]', 'skin_name=虎兄贵', 'skin_author=虎娘',
             'preview_comp=panel.png', '[Display]', 'font_size=200',
             'font_ch=Microsoft YaHei UI', 'font_en=Microsoft YaHei UI',
             'pinyin_color=0x343434', 'zhongwen_first_color=0x1069a8',
             'zhongwen_color=0x292929', 'comphint_color=0x64717e',
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
    output = directory / '虎兄贵.ssf'
    with zipfile.ZipFile(output, 'w') as archive:
        for name, content in [('skin.ini', ('\n'.join(lines)+'\n').encode('utf-8-sig')),
                              ('panel.png', artwork)]:
            info = zipfile.ZipInfo(name, (2026, 10, 5, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, content)
    assert output.stat().st_size < 4 * 1024 * 1024
    return output


if __name__ == '__main__':
    import sys
    print(build(sys.argv[1] if len(sys.argv) > 1 else None))
