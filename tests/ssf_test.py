#!/usr/bin/env python3
"""Exercise the native, data-only SSF parser with self-created fixtures.
Optional --samples DIR --oracle ZIP compares uploaded v3 archives with their
independently extracted assets. No third-party artwork is bundled in tests.
"""
import argparse, io, json, os, pathlib, random, struct, subprocess, tempfile, zipfile, zlib


def png_chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def png(animated=False, sequence=1, frame_count=2, x=0):
    header = png_chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 6, 0, 0, 0))
    pixels = zlib.compress((b'\0' + bytes([100, 150, 200, 255]) * 2) * 2)
    if not animated:
        body = png_chunk(b'IDAT', pixels)
    else:
        def frame(seq, offset, numerator, denominator, dispose, blend):
            return png_chunk(b'fcTL', struct.pack('>IIIIIHHBB', seq, 2, 2, offset, 0, numerator, denominator, dispose, blend))
        body = png_chunk(b'acTL', struct.pack('>II', frame_count, 1))
        body += frame(0, 0, 1, 0, 0, 0) + png_chunk(b'IDAT', pixels)
        body += frame(sequence, x, 3, 10, 2, 1) + png_chunk(b'fdAT', struct.pack('>I', sequence + 1) + pixels)
    return b'\x89PNG\r\n\x1a\n' + header + body + png_chunk(b'IEND', b'')


def archive(ini=None, image=None, entries=(), compression=zipfile.ZIP_DEFLATED):
    if ini is None:
        ini = '[General]\nskin_name=自制测试\n[Display]\npinyin_color=0x112233\n[Scheme_H1]\npic=bg.png\nlayout_horizontal=0,78,25\nlayout_vertical=1,10,10\npinyin_marge=1,1,1,1\nzhongwen_marge=1,1,1,1\n'
    if isinstance(ini, str):
        ini = ini.encode('utf-8')
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', compression=compression) as target:
        target.writestr('skin.ini', ini)
        target.writestr('bg.png', png() if image is None else image)
        for name, content in entries:
            target.writestr(name, content)
    return output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', required=True, type=pathlib.Path)
    parser.add_argument('--samples', type=pathlib.Path)
    parser.add_argument('--oracle', type=pathlib.Path)
    parser.add_argument('--output', type=pathlib.Path)
    parser.add_argument('--work', type=pathlib.Path, default=pathlib.Path('build/ssf'))
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    results = []
    windows_probe = os.name != 'nt' and args.probe.suffix.lower() == '.exe'
    def path_argument(path):
        path = str(path.resolve())
        return subprocess.check_output(['wslpath', '-w', path], text=True).strip() if windows_probe else path
    def run(path):
        process = subprocess.run([str(args.probe.resolve()), path_argument(path)], capture_output=True, timeout=15)
        if process.returncode not in (0, 1):
            raise AssertionError(f'Native parser crash: {process.returncode}: {process.stderr.decode(errors="replace")}')
        if b'AddressSanitizer' in process.stderr or b'runtime error:' in process.stderr:
            raise AssertionError(process.stderr.decode(errors='replace'))
        return process
    with tempfile.TemporaryDirectory(prefix='ssf-test-', dir=args.work.resolve()) as directory:
        root = pathlib.Path(directory)
        def check(name, data, accepted, contains=None):
            path = root / 'case.ssf'
            path.write_bytes(data)
            result = run(path)
            if accepted is not None:
                assert (result.returncode == 0) == accepted, (name, result.stdout.decode(errors='replace'), result.stderr.decode(errors='replace'))
            if contains is not None:
                assert contains.encode() in result.stdout, (name, result.stdout)
            results.append({'name': name, 'accepted': result.returncode == 0})
        good = archive()
        check('stored ZIP', archive(compression=zipfile.ZIP_STORED), True)
        check('deflated ZIP', good, True)
        check('UTF16 INI', archive(ini='[Scheme_H1]\npic=bg.png\n'.encode('utf-16')), True)
        check('UTF8 BOM INI', archive(ini=b'\xef\xbb\xbf[Scheme_H1]\npic=bg.png\n'), True)
        background = '[Scheme_H1]\npic=bg.png\n'
        check('unused statusbar trailing text', archive(ini=background+'[StatusBar]\npic=bar.png\n䨀\uf035;'), True)
        check('unused section duplicate fields', archive(ini=background+'[StatusBar]\npic=a\npic=b\n'), True)
        check('candidate after unused trailing text', archive(ini='[StatusBar]\n残留文本\n'+background), True)
        check('candidate malformed field remains rejected', archive(ini=background+'残留文本\n'), False)
        check('candidate after unused section still validated', archive(ini='[StatusBar]\n残留文本\n'+background+'broken'), False)
        check('authored typography', archive(ini='[Display]\nfont_size=23\ncandidate_spacing=5\nline_spacing=7\ncharacter_spacing=2\n'+background), True, 'metrics 23 5 7 2')
        check('APNG sequence/clock', archive(image=png(True)), True, '2 2 2 1 20 300')
        check('APNG wrong sequence', archive(image=png(True, sequence=2)), False)
        check('APNG wrong count', archive(image=png(True, frame_count=3)), False)
        check('APNG out-of-canvas', archive(image=png(True, x=1)), False)
        corrupt = bytearray(png()); corrupt[-5] ^= 1
        check('PNG CRC', archive(image=bytes(corrupt)), False)
        for name in ['../bad', '..\\bad', '/absolute', 'C:\\evil', 'bg.png:stream', 'nul.txt', 'COM1', 'lpt².txt', 'x/../b', 'x//b', 'x/./b', 'x./b', 'x /b', 'skin.ini/x', 'BG.PNG']:
            check('unsafe/duplicate '+name, archive(entries=[(name, b'x')]), False)
        check('nested ordinary asset', archive(entries=[('images/unused.png', png())]), True)
        for name, ini in [('missing section', 'pic=bg.png'), ('duplicate key', '[Scheme_H1]\npic=bg.png\npic=bg.png'), ('bad section', '[Scheme_H1\npic=bg.png'), ('missing image', '[Scheme_H1]\npic=missing.png'), ('no layout', '[General]\nskin_name=x'), ('nul', '[Scheme_H1]\npic=bg.png\0')]:
            check('INI '+name, archive(ini=ini), False)
        for offset in [0, 1, 3, 8, 22, len(good)-1, len(good)-10, len(good)-22, len(good)//2]:
            check('truncation '+str(offset), good[:offset], False)
        check('unknown Skin version', b'Skin'+struct.pack('<I', 4)+bytes(16), False)
        check('misaligned Skin-v3', b'Skin'+struct.pack('<I', 3)+bytes(15), False)
        check('bad Skin-v3 padding', b'Skin'+struct.pack('<I', 3)+bytes(16), False)
        central = good.index(b'PK\x01\x02')
        for name, local_offset, central_offset, value in [('encrypted',6,8,1), ('unknown compression',8,10,99)]:
            data=bytearray(good);struct.pack_into('<H',data,local_offset,value);struct.pack_into('<H',data,central+central_offset,value)
            check('ZIP '+name, bytes(data), False)
        data=bytearray(good); data[30] ^= 1
        check('local/central filename mismatch', bytes(data), False)
        data=bytearray(good); struct.pack_into('<I',data,central+24,0x40000001)
        check('ZIP expansion budget', bytes(data), False)
        rng=random.Random(20261005)
        for i in range(64):
            data=bytearray(good)
            for _ in range(rng.randrange(1,8)):
                at=rng.randrange(len(data));data[at]^=rng.randrange(1,256)
            check(f'mutated ZIP {i}', bytes(data), None)
        for i in range(32):
            check(f'random binary {i}', bytes(rng.randrange(256) for _ in range(rng.randrange(0,4096))), False)
    if bool(args.samples) != bool(args.oracle):
        parser.error('--samples and --oracle must be supplied together')
    if args.samples:
        with zipfile.ZipFile(args.oracle) as reference:
            for name in ['smile', '小喵风扇']:
                result=run(args.samples/(name+'.ssf'))
                assert result.returncode==0, result.stderr
                actual={}
                for line in result.stdout.decode('ascii').splitlines():
                    fields=line.split()
                    if fields[0]=='entry':
                        key=bytes.fromhex(fields[1]).decode('utf-16-be')
                        actual[key]=(int(fields[2]),int(fields[3]))
                expected={}
                marker='/examples/'+name+'/assets/'
                for member in reference.namelist():
                    if marker in member and not member.endswith('/'):
                        key=member.split(marker,1)[1].replace('\\','/').lower()
                        content=reference.read(member)
                        expected[key]=(len(content),zlib.crc32(content))
                assert expected and actual==expected, (name,actual.keys(),expected.keys())
                results.append({'name':'independent extracted assets: '+name,'entries':len(actual),'accepted':True})
    report={'passed':True,'probe':str(args.probe),'tests':len(results),'results':results}
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'passed':True,'tests':len(results)},ensure_ascii=False))


if __name__=='__main__':
    main()
