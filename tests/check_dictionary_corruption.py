"""Malformed data must be rejected before the host can dereference file offsets."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

probe = str(Path(sys.argv[1]).resolve())
data = bytearray(Path(sys.argv[2]).read_bytes())
record = struct.unpack_from("<Q", data, 40)[0]
values = struct.unpack_from("<Q", data, record + 16)[0]
cases = []

def mutation(name, offset, fmt, value):
    modified = data.copy()
    struct.pack_into(fmt, modified, offset, value)
    cases.append((name, modified))

mutation("magic", 0, "<Q", 0)
mutation("version", 8, "<I", 999)
mutation("section count", 12, "<I", 0xffffffff)
mutation("file size", 16, "<Q", len(data) + 4096)
mutation("section offset overflow", 40, "<Q", 0xffffffffffffffff)
mutation("section count overflow", 36, "<I", 0xffffffff)
mutation("key offset overflow", record, "<Q", 0xffffffffffffffff)
mutation("key length overflow", record + 8, "<I", 0xffffffff)
mutation("key points into header", record, "<Q", 0)
mutation("unknown flags", record + 12, "<I", 0xffffffff)
mutation("value offset overflow", record + 16, "<Q", 0xffffffffffffffff)
mutation("value count overflow", record + 24, "<I", 0xffffffff)
mutation("reserved record", record + 28, "<I", 1)
mutation("value string offset", values, "<Q", 0xffffffffffffffff)
mutation("value string length", values + 8, "<I", 0xffffffff)
cases += [("empty", b""), ("truncated header", data[:30]), ("truncated pool", data[:-2])]
with tempfile.TemporaryDirectory(prefix="native-tiger-corruption-") as directory:
    for index, (name, contents) in enumerate(cases):
        path = Path(directory) / f"{index}.tcd"
        path.write_bytes(contents)
        result = subprocess.run([probe, str(path)], capture_output=True, timeout=15)
        assert result.returncode == 1, f"{name}: unexpected exit {result.returncode}: {result.stdout!r} {result.stderr!r}"
print(f"Rejected {len(cases)} malformed dictionaries without a crash")
