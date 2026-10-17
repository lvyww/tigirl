"""Generate aliasing/alpha fixtures and run the native SSF scaling regression.

No artwork dependencies. Optionally copy real skins for native render comparisons.
"""
import argparse
from pathlib import Path
import shutil
import struct
import subprocess
import zlib
from ssf_test import archive, png_chunk


def pattern(transparent=False, animated=False):
    width, height = 768, 512
    def pixels(alternate=False):
        rows = []
        for y in range(height):
            row = bytearray(b'\0')
            for x in range(width):
                on = (x if y < height // 2 else y) % 3 == 0
                if transparent:
                    row.extend((255, 0, 0, 255) if on else (0, 255, 0, 0))
                else:
                    value = 255 if on else 0
                    row.extend((value, value, value, 255) if not alternate else (0, value, 0, 255))
            rows.append(row)
        return zlib.compress(b''.join(rows))
    header = png_chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    first = pixels()
    if animated:
        def control(sequence):
            return png_chunk(b'fcTL', struct.pack('>IIIIIHHBB', sequence, width, height, 0, 0, 1, 10, 0, 0))
        body = png_chunk(b'acTL', struct.pack('>II', 2, 0))
        body += control(0) + png_chunk(b'IDAT', first)
        body += control(1) + png_chunk(b'fdAT', struct.pack('>I', 2) + pixels(True))
    else:
        body = png_chunk(b'IDAT', first)
    return b'\x89PNG\r\n\x1a\n' + header + body + png_chunk(b'IEND', b'')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, default=Path('build/ssf-scaling'))
    parser.add_argument('--probe', type=Path)
    parser.add_argument('--skins', type=Path)
    args = parser.parse_args()
    fixtures = args.work / 'fixtures'
    fixtures.mkdir(parents=True, exist_ok=True)
    ini = '[Display]\nfont_size=100\n[Scheme_H1]\npic=bg.png\n'
    for name in ('opaque', 'alpha', 'animated'):
        (fixtures / f'scaling-{name}.ssf').write_bytes(archive(ini, pattern(name == 'alpha', name == 'animated')))
    if args.skins:
        for path in args.skins.glob('*.ssf'):
            shutil.copyfile(path, fixtures / path.name)
    if args.probe:
        subprocess.run([str(args.probe.resolve()), str(fixtures.resolve()),
                        str((args.work / 'output').resolve())], check=True, timeout=300)


if __name__ == '__main__':
    main()
