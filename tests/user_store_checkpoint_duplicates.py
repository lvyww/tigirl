"""Retained duplicate commit identities must survive checkpoint fallback."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
probe = ROOT / 'build/user_store_probe'
data = bytearray((ROOT / 'data/tiger-v2.tcd').read_bytes())
count, offset = struct.unpack_from('<IQ', data, 36)
for index in range(count):
    record = offset + index * 32
    key_offset, key_length = struct.unpack_from('<QI', data, record)
    if data[key_offset:key_offset+key_length*2].decode('utf-16-le') == 'ab':
        values, value_count = struct.unpack_from('<QI', data, record+16)
        assert value_count >= 2
        # Change only a private, never-mapped fixture copy. Give the second
        # candidate a distinct display but the first candidate's commit text.
        text_offset, text_length = struct.unpack_from('<QI', data, values)
        first = data[text_offset:text_offset+text_length*2].decode('utf-16-le')
        alias = ('重复显示\x1e'+first.split('\x1e')[-1]).encode('utf-16-le')
        struct.pack_into('<QII', data, values+16, len(data), len(alias)//2, 0)
        data.extend(alias)
        struct.pack_into('<Q', data, 16, len(data))
        break
else:
    raise AssertionError('Missing ab fixture key')
with tempfile.TemporaryDirectory(prefix='checkpoint-duplicates-', dir=ROOT/'build') as temporary:
    dictionary = Path(temporary)/'duplicate.tcd'
    journal = Path(temporary)/'user.tcu'
    dictionary.write_bytes(data)
    def run(command, *arguments):
        return subprocess.check_output([str(probe), str(dictionary), str(journal), command, 'ab', *arguments], timeout=30)
    before = run('write', 'fixture', '1').splitlines()
    decoded = [bytes.fromhex(value.decode()).decode('utf-16-be') for value in before]
    assert len(decoded) >= 3 and decoded[0] != decoded[1] and decoded[0].split('\x1e')[-1] == decoded[1].split('\x1e')[-1], 'Fixture did not retain alias duplicates'
    original = journal.read_bytes()
    checkpoint = bytes.fromhex(run('checkpoint').decode())
    assert checkpoint == original, 'Fallback changed history'
    assert run('dump').splitlines() == before and journal.read_bytes() == original
    # Add a long redundant history on another code. The duplicate ab key must
    # retain its own history without preventing this independent compaction.
    code = 'independent'.encode('utf-16-le')
    value = 'word'.encode('utf-16-le')
    payload = struct.pack('<III', 0, len(code)//2, len(value)//2)+code+value
    record = struct.pack('<II', len(payload), zlib.crc32(payload))+payload
    journal.write_bytes(original+record*1000)
    expanded = journal.read_bytes()
    mixed = bytes.fromhex(run('checkpoint').decode())
    assert len(mixed) < len(expanded)//2, 'Duplicate code prevented independent compaction'
    assert journal.read_bytes() == expanded, 'Checkpoint modified live history'
    restored = Path(temporary)/'restored.tcu'
    restored.write_bytes(mixed)
    restored_values = subprocess.check_output([str(probe), str(dictionary), str(restored), 'dump', 'ab'], timeout=30)
    assert restored_values.splitlines() == before, 'Partial fallback lost duplicate candidates'
    other = subprocess.check_output([str(probe), str(dictionary), str(restored), 'dump', 'independent'], timeout=30)
    assert bytes.fromhex(other.decode()).decode('utf-16-be') == 'word'
report = {'status': 'passed', 'platform': 'Linux', 'duplicate_identity_fallback': True,
          'independent_code_compacted': True, 'expanded_bytes': len(expanded), 'checkpoint_bytes': len(mixed),
          'live_journal_unchanged': True, 'probe_sha256': hashlib.sha256(probe.read_bytes()).hexdigest(),
          'fixture_sha256': hashlib.sha256(data).hexdigest()}
(ROOT/'build/user-store-checkpoint-duplicates.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report))
