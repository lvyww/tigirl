"""Exact-byte export compatibility and mapped-reader round trips, no publication."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
EXE = BUILD / 'tests/ARM64/lexicon_serialize_probe.exe'

def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()

def fixture(sections, quick):
    # Independent format fixture, including raw surrogate code units and NULs.
    sections = [sorted(rows, key=lambda row: row[0].encode('utf-16-be', 'surrogatepass')) for rows in sections]
    value_at = 128 + 32 * sum(map(len, sections))
    data = bytearray(value_at + 16 * sum(len(values) for rows in sections for _, _, values in rows))
    struct.pack_into('<8sIIQII', data, 0, b'TIGERD02', 2, 6, 0, quick, 0)
    pool = {}
    def intern(text):
        if text not in pool:
            pool[text] = len(data)
            data.extend(text.encode('utf-16-le', 'surrogatepass'))
        return pool[text]
    record_at = 128
    for i, rows in enumerate(sections):
        struct.pack_into('<IIQ', data, 32 + i * 16, i + 1, len(rows), record_at)
        for key, flags, values in rows:
            key_at = intern(key)
            struct.pack_into('<QIIQII', data, record_at, key_at, len(key.encode('utf-16-le', 'surrogatepass')) // 2, flags, value_at, len(values), 0)
            record_at += 32
            for value in values:
                offset = intern(value)
                struct.pack_into('<QII', data, value_at, offset, len(value.encode('utf-16-le', 'surrogatepass')) // 2, 0)
                value_at += 16
    struct.pack_into('<Q', data, 16, len(data))
    return bytes(data)

results = []
with tempfile.TemporaryDirectory(prefix='serialize-test-', dir=BUILD) as temporary:
    directory = Path(temporary)
    edge = directory / 'edge.tcd'
    edge.write_bytes(fixture([
        [('a', 10, ['共同', '', '显\0示\t提交']), ('ab', 9, ['共同']), ('x', 8, []), ('\ud800', 2, []), ('\ud800\udc00', 9, ['\udfff'])],
        [('z', 0, ['共同', '共同']), ('a', 0, ['\ud800', ''])],
        [('共同', 0, ['\n\t\0']), ('空', 0, [''])],
        [('共同', 0, ['共同'])], [('共同', 0, ['ab'])], [('共同', 0, ['ab'])]], 15))
    minimal = directory / 'minimal.tcd'
    minimal.write_bytes(fixture([[('a', 8, [])], [], [], [], [], []], 0))
    for i, source in enumerate([ROOT / 'data/tiger-v2.tcd', edge, minimal]):
        destination = directory / f'result-{i}.tcd'
        result = json.loads(subprocess.check_output([str(EXE), win(source), win(destination)], text=True))
        expected = source.read_bytes()
        assert destination.read_bytes() == expected, source
        result.update(source=source.name, sha256=hashlib.sha256(expected).hexdigest(), byte_identical=True)
        results.append(result)
report = dict(cases=len(results), results=results, probe_sha256=hashlib.sha256(EXE.read_bytes()).hexdigest())
(BUILD / 'lexicon-serialize-arm64.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
