#!/usr/bin/env python3
"""Package the three illustrated Tigirl skins without altering source artwork."""
from pathlib import Path
import zipfile
import struct

ROOT = Path(__file__).resolve().parents[1]
# Compact, single-row backgrounds expand for code and additional candidates.
# A 200 px baseline reduces the illustration without changing runtime text size.
SKINS = (
    ('虎纹奶油', 'tiger-cream', 670, 204, 155, 200, '0x246dcc', '0x303b50', '0x626e80'),
    ('蜜桃虎娘', 'peach-tigirl', 650, 154, 210, 240, '0x7545be', '0x48405c', '0x847388'),
    ('元气虎娘', 'energetic-tigirl', 640, 274, 170, 220, '0x1978be', '0x202e41', '0x5f7069'),
)


def build(destination=None):
    destination = Path(destination) if destination else ROOT / '皮肤'
    destination.mkdir(parents=True, exist_ok=True)
    results = []
    for name, asset, top, bottom, left, right, first, normal, hint in SKINS:
        # Add breathing room inside the painted panel, independently of the
        # transparent ornament area already accounted for in the base insets.
        # At font size 17 this adds 7.65 px vertically and 6.8 px horizontally.
        top += 90
        bottom += 90
        left += 80
        right += 80
        artwork = (ROOT / 'assets/skins/tigirl-trio' / f'{asset}-narrow.png').read_bytes()
        width, height = struct.unpack('>II', artwork[16:24])
        horizontal = f'0,300,{width - 301}'
        band = {'tiger-cream': 730, 'peach-tigirl': 755, 'energetic-tigirl': 710}[asset]
        vertical = f'0,{band},{height - band - 1}'
        lines = ['[General]', f'skin_name={name}', 'skin_author=虎娘',
                 'preview_comp=panel.png', '[Display]', 'font_size=200',
                 'font_ch=Microsoft YaHei UI', 'font_en=Microsoft YaHei UI',
                 f'pinyin_color={normal}', f'zhongwen_first_color={first}',
                 f'zhongwen_color={normal}', f'comphint_color={hint}',
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
        path = destination / f'{name}.ssf'
        entries = {'skin.ini': ('\n'.join(lines) + '\n').encode('utf-8-sig'),
                   'panel.png': artwork}
        with zipfile.ZipFile(path, 'w') as archive:
            for filename, content in entries.items():
                info = zipfile.ZipInfo(filename, (2026, 10, 5, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                archive.writestr(info, content)
        assert path.stat().st_size < 4 * 1024 * 1024
        results.append(path)
    return results


if __name__ == '__main__':
    import sys
    for result in build(sys.argv[1] if len(sys.argv) > 1 else None):
        print(result)
