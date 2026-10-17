#!/usr/bin/env python3
"""Build selected SSF skins, embedding their source RGBA artwork unchanged."""
import argparse
import hashlib
import json
import math
import struct
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets/skins/directions-20261017'
LAYOUT = ROOT / 'assets/skins/directions-20261017-tuned/skins.json'
RUNTIME_LIMIT = 4 * 1024 * 1024


def bgr(color):
    red, green, blue = bytes.fromhex(color.removeprefix('#'))
    return f'0x{blue:02x}{green:02x}{red:02x}'


def build(destination=None, assets=None, only=None, art_scale=1, layout=None):
    if not math.isfinite(art_scale) or art_scale <= 0:
        raise ValueError('Art scale must be finite and positive')
    # Explicit source directories retain their historical definitions. The
    # default build uses the current selected-skin profile over retained PNGs.
    if assets is None:
        assets = ASSETS
        if layout is None:
            layout = LAYOUT
    assets = Path(assets)
    destination = Path(destination) if destination else ROOT / '皮肤'
    destination.mkdir(parents=True, exist_ok=True)
    definitions = json.loads((Path(layout) if layout else assets / 'skins.json').read_text(encoding='utf-8'))
    if only:
        definitions = [skin for skin in definitions if skin['id'] in only]
        if {skin['id'] for skin in definitions} != set(only):
            raise ValueError('Unknown skin ID')
    results = []
    for skin in definitions:
        authored_font = skin.get('font_size', 200)
        font_size = round(authored_font / art_scale)
        if not 3 <= font_size <= 200:
            raise ValueError('Scaled SSF font_size must be between 3 and 200')
        # The runtime scales art by user_font / authored_font. Lowering this
        # basis enlarges the original PNG without editing its pixels. Reduce
        # text spacing in source coordinates to preserve its displayed size.
        text_scale = font_size / authored_font
        candidate_spacing = round(skin.get('candidate_spacing', 120) * text_scale)
        line_spacing = round(skin.get('line_spacing', 24) * text_scale)
        code_gap = round(skin.get('code_gap', 32) * text_scale)
        lines = ['[General]', f'skin_name={skin["name"]}', 'skin_author=虎娘',
                 'preview_comp=horizontal.png', '[Display]', f'font_size={font_size}',
                 'font_ch=Microsoft YaHei UI', 'font_en=Microsoft YaHei UI',
                 f'pinyin_color={bgr(skin["ink"])}',
                 f'zhongwen_first_color={bgr(skin["first"])}',
                 f'zhongwen_color={bgr(skin["ink"])}',
                 f'comphint_color={bgr(skin["hint"])}',
                 f'candidate_spacing={candidate_spacing}', f'line_spacing={line_spacing}', 'character_spacing=0']
        entries = {}
        for orientation, code in [('horizontal', 'H'), ('vertical', 'V')]:
            frame = skin[orientation]
            minimums = {key: frame[key] for key in ('min_width', 'min_height') if key in frame}
            for key, value in minimums.items():
                if type(value) is not int or not 1 <= value <= 8192:
                    raise ValueError(f'{skin["name"]}: invalid {key}')
            artwork = (assets / skin['slug'] / frame['file']).read_bytes()
            if artwork[:8] != b'\x89PNG\r\n\x1a\n' or artwork[25] != 6:
                raise ValueError(f'{skin["name"]}: expected RGBA PNG')
            width, height = struct.unpack('>II', artwork[16:24])
            def axis(key, size, index):
                values = frame.get(key)
                if values is None:
                    at = frame['stretch'][index]
                    values = [0, at, size - at - 1]
                if (len(values) != 3 or any(type(n) is not int for n in values) or
                        values[0] not in (0, 1) or min(values[1:]) < 0 or
                        sum(values[1:]) >= size):
                    raise ValueError(f'{skin["name"]}: invalid {key}')
                return ','.join(map(str, values))

            def insets(key):
                values = frame.get(key, frame['insets'])
                if (len(values) != 4 or any(type(n) is not int or n < 0 for n in values) or
                        values[0] + values[1] >= height or values[2] + values[3] >= width):
                    raise ValueError(f'{skin["name"]}: invalid {key}')
                return values

            horizontal = axis('stretch_horizontal', width, 0)
            vertical = axis('stretch_vertical', height, 1)
            top, bottom, left, right = insets('combined_insets')
            filename = f'{orientation}.png'
            entries[filename] = artwork
            lines += [f'[Scheme_{code}1]', f'pic={filename}',
                      f'layout_horizontal={horizontal}', f'layout_vertical={vertical}',
                      f'pinyin_marge={top},{code_gap},{left},{right}',
                      f'zhongwen_marge={code_gap},{bottom},{left},{right}']
            lines.extend(f'{key}={value}' for key, value in minimums.items())
            lines.append(f'[Scheme_{code}2]')
            for prefix in ('pinyin', 'zhongwen'):
                top, bottom, left, right = insets('code_insets' if prefix == 'pinyin' else 'candidate_insets')
                lines += [f'{prefix}_pic={filename}',
                          f'{prefix}_layout_horizontal={horizontal}',
                          f'{prefix}_layout_vertical={vertical}',
                          f'{prefix}_marge={top},{bottom},{left},{right}']
                lines.extend(f'{prefix}_{key}={value}' for key, value in minimums.items())
        entries['skin.ini'] = ('\n'.join(lines) + '\n').encode('utf-8-sig')
        path = destination / f'{skin["name"]}.ssf'
        with zipfile.ZipFile(path, 'w') as archive:
            for name, content in sorted(entries.items()):
                info = zipfile.ZipInfo(name, (2026, 10, 17, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                archive.writestr(info, content)
        if path.stat().st_size >= RUNTIME_LIMIT:
            raise ValueError(f'{path}: exceeds runtime 4 MiB limit')
        results.append(path)
    return results


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', nargs='?', type=Path)
    parser.add_argument('--assets', type=Path,
                        help='Explicit historical source directory (uses its own skins.json)')
    parser.add_argument('--layout', type=Path,
                        help='Override INI/layout definitions while retaining source PNG bytes')
    parser.add_argument('--skin', action='append', help='Build only the specified ID; repeatable')
    parser.add_argument('--art-scale', type=float, default=1,
                        help='Enlarge original art without changing user text size (integer SSF basis)')
    args = parser.parse_args()
    for result in build(args.destination, args.assets, args.skin, args.art_scale, args.layout):
        print(f'{result}  {result.stat().st_size} bytes  sha256={hashlib.sha256(result.read_bytes()).hexdigest()}')
