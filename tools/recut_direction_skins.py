#!/usr/bin/env python3
"""Recompose the original skin pixels using explicit crops, masks and scaling.

No generative edits. Pillow RGBa resizing prevents transparent-edge color bleed.
All crop polygons and placements live in the versioned recipe JSON.
"""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageOps
import cv2
import numpy as np
from build_direction_ssf import build

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/skins/directions-20261017'
ASSETS = ROOT / 'assets/skins/directions-20261017-recut'


def resize(image, size):
    return image.convert('RGBa').resize(size, Image.Resampling.LANCZOS).convert('RGBA')


def extract(image, points, chroma_key=False, clip=None, exclude=()):
    mask = Image.new('L', (image.width * 4, image.height * 4))
    ImageDraw.Draw(mask).polygon([(round(x * 4), round(y * 4)) for x, y in points], fill=255)
    mask = mask.resize(image.size, Image.Resampling.LANCZOS)
    if clip:
        limit = Image.new('L', image.size)
        x0, y0, x1, y1 = clip
        ImageDraw.Draw(limit).rectangle((x0, y0, x1-1, y1-1), fill=255)
        mask = ImageChops.multiply(mask, limit)
    for polygon in exclude:
        cut = Image.new('L', image.size)
        ImageDraw.Draw(cut).polygon([tuple(p) for p in polygon], fill=255)
        mask = ImageChops.subtract(mask, cut)
    # Generated originals contain isolated 1/255-alpha specks far from the art.
    # They are not part of the visible silhouette and must not anchor a crop.
    clean_alpha = image.getchannel('A').point(lambda a: a if a > 8 else 0)
    # Refine only a narrow band around the hand-authored cut. The interior is
    # locked foreground and the exterior locked background; no pixels are drawn.
    alpha = np.asarray(clean_alpha)
    if chroma_key:
        rgb = np.asarray(image.convert('RGB')).astype(np.int16)
        # The neon symbols are saturated pink/cyan on a flat dark panel. Key
        # only these explicitly marked symbol crops, never character pixels.
        brightness = rgb.max(axis=2)
        saturation = rgb.max(axis=2) - rgb.min(axis=2)
        strength = np.clip(np.minimum((brightness-105)/50, (saturation-45)/35), 0, 1)
        mask = ImageChops.multiply(mask, Image.fromarray((strength*255).astype(np.uint8)))
        layer = image.copy()
        layer.putalpha(ImageChops.multiply(clean_alpha, mask))
        box = layer.getbbox()
        return layer.crop(box), box
    rough = np.asarray(mask) >= 128
    kernel = np.ones((25, 25), np.uint8)
    inside = cv2.erode(rough.astype(np.uint8), kernel) != 0
    outside = cv2.dilate(rough.astype(np.uint8), kernel) == 0
    labels = np.where(rough, cv2.GC_PR_FGD, cv2.GC_PR_BGD).astype(np.uint8)
    labels[inside] = cv2.GC_FGD
    labels[outside | (alpha == 0)] = cv2.GC_BGD
    cv2.setRNGSeed(0)
    cv2.grabCut(np.asarray(image.convert('RGB')), labels, None,
                np.zeros((1,65),np.float64), np.zeros((1,65),np.float64),
                2, cv2.GC_INIT_WITH_MASK)
    keep = ((labels == cv2.GC_FGD) | (labels == cv2.GC_PR_FGD)).astype(np.uint8) * 255
    # Keep the source alpha, including its existing antialiasing, at the cut.
    mask = ImageChops.multiply(mask, Image.fromarray(keep))
    layer = image.copy()
    layer.putalpha(ImageChops.multiply(clean_alpha, mask))
    box = layer.getbbox()
    if box is None:
        raise ValueError('Empty layer')
    return layer.crop(box), box


def panel(corner, size, corner_size):
    """Build a closed frame from a clean top-right corner and its straight edges."""
    w, h = size
    n = corner_size
    assert min(w, h) >= 2 * n
    tr = resize(corner, (n, n))
    out = Image.new('RGBA', size, corner.getpixel((0, corner.height - 1)))
    top = resize(tr.crop((0, 0, 1, n)), (w - 2 * n, n))
    side = resize(tr.crop((0, n - 1, n, n)), (n, h - 2 * n))
    for patch, at in [(tr, (w-n, 0)), (ImageOps.mirror(tr), (0, 0)),
                      (ImageOps.flip(tr), (w-n, h-n)),
                      (ImageOps.flip(ImageOps.mirror(tr)), (0, h-n)),
                      (top, (n, 0)), (ImageOps.flip(top), (n, h-n)),
                      (side, (w-n, n)), (ImageOps.mirror(side), (0, n))]:
        out.paste(patch, at)
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path,
                        default=ROOT / 'build/skin-pack-20261017-recut/skins')
    args = parser.parse_args()
    recipes = json.loads((ASSETS / 'recipes.json').read_text())
    originals = {s['id']: s for s in json.loads((SOURCE / 'skins.json').read_text())}
    definitions = []
    evidence = []
    for recipe in recipes:
        old = originals[recipe['id']]
        definition = {k: old[k] for k in ('id', 'name', 'slug', 'ink', 'first', 'hint')}
        definition.update(font_size=100, candidate_spacing=60, line_spacing=12, code_gap=16)
        folder = ASSETS / old['slug']
        folder.mkdir(parents=True, exist_ok=True)
        images = {orient: Image.open(SOURCE / old['slug'] / old[orient]['file']).convert('RGBA')
                  for orient in ('horizontal', 'vertical')}
        for orient in ('horizontal', 'vertical'):
            spec = recipe[orient]
            canvas = Image.new('RGBA', tuple(spec['size']))
            corner_spec = spec['frame_source']
            corner = images[corner_spec['orientation']].crop(tuple(corner_spec['box']))
            if 'fill_polygon' in corner_spec:
                sample = corner_spec['fill_sample']
                color = images[sample['orientation']].getpixel(tuple(sample['point']))[:3] + (255,)
                ImageDraw.Draw(corner).polygon(corner_spec['fill_polygon'], fill=color)
            if corner_spec.get('mirror'):
                corner = ImageOps.mirror(corner)
            if corner_spec.get('flip'):
                corner = ImageOps.flip(corner)
            left, top, right, bottom = spec['frame_box']
            canvas.alpha_composite(panel(corner, (right-left, bottom-top), spec['corner_size']), (left, top))
            layers = []
            foreground = Image.new('L', canvas.size)
            for component in spec['components']:
                layer, source_box = extract(images[orient], component['polygon'], component.get('chroma_key', False), component.get('clip'), component.get('exclude', []))
                scale = component['scale']
                layer = resize(layer, (round(layer.width * scale), round(layer.height * scale)))
                x, y = component['position']
                if component.get('source_y'):
                    y += round(source_box[1] * scale)
                if component.get('anchor', '').endswith('right'):
                    x -= layer.width
                if component.get('anchor', '').startswith('bottom'):
                    y -= layer.height
                assert x >= 0 and y >= 0 and x + layer.width <= canvas.width and y + layer.height <= canvas.height, (old['name'], orient, component['name'], (x, y), layer.size)
                canvas.alpha_composite(layer, (x, y))
                placed = Image.new('L', canvas.size)
                placed.paste(layer.getchannel('A'), (x, y))
                foreground = ImageChops.lighter(foreground, placed)
                mt, mb, ml, mr = spec['insets']
                safe_bottom = mt + 312 if orient == 'horizontal' else canvas.height - mb
                safe = (ml, mt, canvas.width-mr, safe_bottom)
                intersection = placed.crop(safe).point(lambda a: 255 if a > 32 else 0)
                layers.append({'name': component['name'], 'source_box': source_box,
                               'scale': scale, 'destination': [x, y, x+layer.width, y+layer.height],
                               'text_intrusion_pixels': intersection.histogram()[255],
                               'text_intrusion_box': intersection.getbbox()})
            stretch_x, stretch_y = spec['stretch']
            column_alpha = foreground.crop((stretch_x,0,stretch_x+1,canvas.height)).getextrema()[1]
            row_alpha = foreground.crop((0,stretch_y,canvas.width,stretch_y+1)).getextrema()[1]
            assert column_alpha == 0, (old['name'], orient, 'Width slice crosses artwork', column_alpha)
            if orient == 'vertical':
                assert row_alpha == 0, (old['name'], 'Height slice crosses artwork', row_alpha)
            # Horizontal content is at most two rows, so it never needs height
            # growth. Native tests also verify this for all review scenarios.
            else:
                assert spec['insets'][0] + spec['insets'][1] + 2*140 + 2*16 <= canvas.height
            path = folder / (orient + '.png')
            canvas.save(path, optimize=True)
            definition[orient] = {'file': path.name, 'stretch': spec['stretch'], 'insets': spec['insets']}
            evidence.append({'skin': old['name'], 'orientation': orient, 'size': canvas.size,
                             'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'layers': layers,
                             'stretch_column_foreground_alpha': column_alpha,
                             'stretch_row_foreground_alpha': row_alpha,
                             'horizontal_height_growth': False if orient == 'horizontal' else None})
        definitions.append(definition)
    (ASSETS / 'skins.json').write_text(json.dumps(definitions, ensure_ascii=False, indent=2) + '\n')
    (ASSETS / 'composition-report.json').write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + '\n')
    for p in build(args.destination, ASSETS):
        print(p, p.stat().st_size)


if __name__ == '__main__':
    main()
