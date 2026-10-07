"""Generate flat SSF fixtures and optionally run the native pixel seam regression.

Run on Windows with --probe build/tests/ARM64/ssf_render_probe.exe.
Without --probe, only generate fixtures (also supported on Linux).
"""
import argparse
from pathlib import Path
import struct
import subprocess
import zipfile
import zlib
from ssf_test import png_chunk


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, default=Path('build/ssf-seams'))
    parser.add_argument('--probe', type=Path)
    args = parser.parse_args()
    fixtures = args.work / 'fixtures'
    fixtures.mkdir(parents=True, exist_ok=True)
    for alpha in (255, 128):
        for mode in (0, 1):
            width, height = 31, 23
            pixels = (b'\0' + bytes((100, 150, 200, alpha)) * width) * height
            png = (b'\x89PNG\r\n\x1a\n' +
                   png_chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) +
                   png_chunk(b'IDAT', zlib.compress(pixels)) + png_chunk(b'IEND', b''))
            ini = (f'[Scheme_H1]\npic=bg.png\nlayout_horizontal={mode},5,7\n'
                   f'layout_vertical={mode},3,5\n')
            with zipfile.ZipFile(fixtures / f'flat-{alpha}-{mode}.ssf', 'w', zipfile.ZIP_DEFLATED) as archive:
                archive.writestr('skin.ini', ini)
                archive.writestr('bg.png', png)
    if args.probe:
        subprocess.run([str(args.probe.resolve()), str(fixtures.resolve()),
                        str((args.work / 'output').resolve())], check=True, timeout=60)


if __name__ == '__main__':
    main()
