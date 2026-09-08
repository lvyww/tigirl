"""Exhaustive comparison with the original C# loader, not a sample of keys."""
import itertools
import json
from pathlib import Path
import subprocess
import sys

probe, dictionary = sys.argv[1:3]
process = subprocess.Popen([probe, dictionary, "dump"], stdout=subprocess.PIPE, text=True, encoding="utf-8")
count = 0
try:
    with open(dictionary + ".expected.jsonl", encoding="utf-8-sig") as expected:
        for wanted, actual in itertools.zip_longest(expected, process.stdout):
            assert wanted is not None and actual is not None, f"Record count differs after {count}"
            a, b = json.loads(wanted), json.loads(actual)
            assert a == b, f"Record {count} differs: {a['section']} / {a['key']!r}"
            count += 1
    assert process.wait() == 0, "Native dictionary reader failed"
finally:
    if process.poll() is None:
        process.terminate()
        process.wait()
print(json.dumps({"matched_records": count, "dictionary_bytes": Path(dictionary).stat().st_size}))
