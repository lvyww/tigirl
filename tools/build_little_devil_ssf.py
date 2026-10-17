#!/usr/bin/env python3
"""Build active little-devil skins, or explicitly rebuild the retained archive."""
import argparse
import hashlib
import json
from pathlib import Path

from build_direction_ssf import build

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets/skins/little-devil-20261017'
ARCHIVE = ROOT / '皮肤/归档/20261018-小恶魔'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', nargs='?', type=Path)
    parser.add_argument('--archived', action='store_true',
                        help='Rebuild archived skins into the archive directory by default')
    parser.add_argument('--skin', action='append', choices=('01', '02', '03'),
                        help='Build only this direction; repeatable')
    args = parser.parse_args()
    layout = ASSETS / ('archived-skins.json' if args.archived else 'skins.json')
    available = {skin['id'] for skin in json.loads(layout.read_text(encoding='utf-8'))}
    if args.skin and not set(args.skin) <= available:
        parser.error('Selected skin is not in this set; use --archived for 01/02, or omit it for 03')
    destination = args.destination or (ARCHIVE if args.archived else ROOT / '皮肤')
    for path in build(destination, assets=ASSETS, only=args.skin, layout=layout):
        print(f'{path}  {path.stat().st_size} bytes  sha256={hashlib.sha256(path.read_bytes()).hexdigest()}')
