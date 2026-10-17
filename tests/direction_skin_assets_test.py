#!/usr/bin/env python3
"""Catch transparent text-area holes and stretch slices crossing the mascot.

Pillow is used only to inspect source PNG pixels; artwork is never modified.
"""
import argparse
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path,
                        default=ROOT / 'assets/skins/directions-20261017-v2')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    skins = json.loads((args.assets / 'skins.json').read_text(encoding='utf-8'))
    results = []
    assert {s['id'] for s in skins} == {'02', '03', '07', '09'}
    for skin in skins:
        for orientation in ('horizontal', 'vertical'):
            frame = skin[orientation]
            with Image.open(args.assets / skin['slug'] / frame['file']) as image:
                assert image.mode == 'RGBA', (skin['name'], 'No alpha channel')
                assert image.getchannel('A').getextrema()[0] == 0, 'Opaque external backdrop'
                # The rectangle is deliberately inside the unadorned text field,
                # away from anti-aliased frame edges and transparent surroundings.
                region = image.getchannel('A').crop(tuple(frame['panel_bounds']))
                low, high = region.getextrema()
                assert low >= 245, (skin['name'], orientation, 'Transparent text-area hole', low)
                left, top, right, bottom = frame['character_bounds']
                x, y = frame['stretch']
                assert 0 <= left < right <= image.width and 0 <= top < bottom <= image.height
                assert not left <= x <= right, 'Horizontal slice crosses character'
                assert not top <= y <= bottom, 'Vertical slice crosses character'
                scale = 17 / skin['font_size']
                height = (bottom - top) * scale
                if orientation == 'horizontal':
                    # H bounding box includes the paws resting on the rim.
                    assert 75 <= height <= 100, (skin['name'], 'Small/oversized horizontal mascot', height)
                    exposed = (frame['frame_top'] - top) * scale
                    assert 75 <= exposed <= 90, (skin['name'], 'Incorrect exposed mascot height', exposed)
                else:
                    assert 105 <= height <= 140, (skin['name'], 'Small/oversized vertical mascot', height)
                results.append({'skin': skin['name'], 'orientation': orientation,
                                'size': image.size, 'panel_alpha_range': [low, high],
                                'character_height_at_17_dip': round(height, 2),
                                'exposed_height_at_17_dip': round(exposed, 2) if orientation == 'horizontal' else None})
    report = {'passed': True, 'cases': len(results), 'results': results}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()
