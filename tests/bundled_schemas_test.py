"""Verify bundled source bytes and import both real schemas without a model.

Run build_native.ps1 first on Windows, then pass --platform x64 or Win32.
WSL can use the matching Windows executables. --verify-only also works on Linux.
All imported dictionaries live in a temporary directory; no registration occurs.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / 'resources' / 'DefaultData'


def verify_sources():
    manifest = json.loads((ROOT / 'resources/default-data-manifest.json').read_text(encoding='utf-8'))
    files = {p.relative_to(DATA).as_posix(): p for p in DATA.rglob('*') if p.is_file()}
    assert files.keys() == manifest['files'].keys(), 'Bundled file inventory changed'
    for name, path in files.items():
        data = path.read_bytes()
        expected = manifest['files'][name]
        assert len(data) == expected['bytes'], name
        assert hashlib.sha256(data).hexdigest() == expected['sha256'], name
    return manifest


def sections(path):
    data = path.read_bytes()
    magic, version, count, size = struct.unpack_from('<8sIIQ', data)
    assert (magic, version, count, size) == (b'TIGERD02', 2, 6, len(data)), path
    def text(offset, length):
        assert offset + length * 2 <= len(data), path
        return data[offset:offset + length * 2].decode('utf-16-le', errors='surrogatepass')
    result = {}
    for i in range(count):
        section, rows, offset = struct.unpack_from('<IIQ', data, 32 + i * 16)
        entries = {}
        for j in range(rows):
            key_at, key_len, flags, values_at, values, reserved = struct.unpack_from('<QIIQII', data, offset + j * 32)
            key = text(key_at, key_len)
            assert key not in entries, (path, key)
            entries[key] = [text(*struct.unpack_from('<QI', data, values_at + k * 16)) for k in range(values)]
        result[section] = entries
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['ARM64', 'x64', 'Win32'], default='x64')
    parser.add_argument('--verify-only', action='store_true')
    args = parser.parse_args()
    manifest = verify_sources()
    if args.verify_only:
        print(json.dumps({'source_files': len(manifest['files']), 'status': 'passed'}))
        return
    exe = ROOT / 'build/tests' / args.platform / 'Tigirl.Import.exe'
    if not exe.is_file():
        parser.error(f'Build the native tools first: {exe}')
    if os.name == 'nt':
        native_path = lambda p: str(p)
        run = subprocess.run
    else:
        from windows_process import run_windows
        native_path = lambda p: subprocess.check_output(['wslpath', '-w', str(p)], text=True).strip()
        run = run_windows
    # Inside the checkout so WSL outputs reside on the Windows-accessible drive.
    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='bundled-schemas-', dir=ROOT / 'build') as temp:
        for i, scheme in enumerate(manifest['schemes']):
            output = Path(temp) / f'schema-{i}.tcd'
            completed = run([str(exe), native_path(DATA / '码表' / scheme),
                             native_path(DATA / '拼音反查码表'), native_path(output), 'zh-CN'],
                            capture_output=True, text=True, timeout=180)
            assert completed.returncode == 0, (scheme, completed.stdout, completed.stderr)
            imported = json.loads(completed.stdout)
            assert imported['published'] and imported['main_records'] > 1000 and imported['pinyin_records'] > 1000, imported
            ordinary = sections(output)
            assert '交' in ordinary[1]['ab'], (scheme, 'ab')
            assert '你' in ordinary[2]['ni'], (scheme, 'ni')
            if scheme == '虎码字词':
                assert ordinary[3].get('你') and ordinary[4].get('你'), 'Missing comments/splits'
            sentence = sections(Path(str(output) + '.sentence.tcd'))
            sections(Path(str(output) + '.supplement.tcd'))
            assert sentence[1], 'Sentence lexicon is empty'
            print(json.dumps({'scheme': scheme, 'status': 'passed', **imported,
                              'reverse_lookup': True, 'sentence_sidecars': True,
                              'model_required': False}))
    verify_sources()


if __name__ == '__main__':
    main()
