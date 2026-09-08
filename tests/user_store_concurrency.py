"""Actual independent writers, restart recovery and corrupt-log refusal."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import hashlib
import itertools

ROOT = Path(__file__).resolve().parents[1]
mixed = "--mixed-arm64ec" in sys.argv
windows = "--windows" in sys.argv or mixed
def path(p):
    return subprocess.check_output(["wslpath", "-w", str(p)], text=True).strip() if windows else str(p)
probe = ROOT / ("build/tests/ARM64/user_store_probe.exe" if windows else "build/user_store_probe")
executables = [probe, ROOT / 'build/tests/ARM64EC/user_store_probe.exe'] if mixed else [probe]
round_robin = itertools.cycle(executables)
dictionary = path(ROOT / "data/tiger-v2.tcd")
with tempfile.TemporaryDirectory(prefix="user-store-", dir=ROOT / "build") as temp:
    journal = Path(temp) / "user.tcu"
    def command(*args):
        executable = next(round_robin)
        if windows:
            ps = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
            prefix = [ps] if Path('/proc/sys/fs/binfmt_misc/WSLInterop').exists() else ['/init', ps]
            def quote(value):
                return "'"+str(value).replace("'", "''")+"'"
            invocation = '& '+' '.join(quote(value) for value in [path(executable), dictionary, path(journal), *args])+'; exit $LASTEXITCODE'
            return [*prefix, '-NoProfile', '-Command', invocation]
        return [str(executable), dictionary, path(journal), *args]
    if not windows:
        import fcntl
        import time
        # Hold the stable coordination lock before the journal even exists.
        # A reader must wait without opening/creating the replaceable journal.
        with open(str(journal)+'.lock', 'wb') as coordination:
            fcntl.flock(coordination, fcntl.LOCK_EX)
            reader = subprocess.Popen(command('dump', 'nativeconcurrent'), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                deadline = time.monotonic()+3
                observed = False
                while time.monotonic()<deadline and reader.poll() is None:
                    for fd in (Path('/proc')/str(reader.pid)/'fd').iterdir():
                        try:
                            observed |= fd.resolve() == Path(str(journal)+'.lock')
                        except FileNotFoundError:
                            pass
                    if observed:
                        break
                    time.sleep(.01)
                assert observed and reader.poll() is None and not journal.exists(), 'Reader bypassed coordination lock'
            finally:
                fcntl.flock(coordination, fcntl.LOCK_UN)
                stdout, stderr = reader.communicate(timeout=10)
            assert reader.returncode == 0 and stdout == b'', stderr
    writers = [subprocess.Popen(command("write", "nativeconcurrent", f"writer{i}-", "12"), stdout=subprocess.DEVNULL, stderr=subprocess.PIPE) for i in range(4)]
    for writer in writers:
        _, error = writer.communicate(timeout=60)
        assert writer.returncode == 0, error
    def dump():
        output = subprocess.check_output(command("dump", "nativeconcurrent"), text=True)
        return [bytes.fromhex(x).decode("utf-16-be") for x in output.splitlines()]
    expected = {f"writer{i}-{j}" for i in range(4) for j in range(12)}
    values = dump()
    assert set(values) == expected and len(values) == len(expected), "Concurrent update lost or duplicated"
    pristine = journal.read_bytes()
    # Interrupt every byte position of a genuine final record, then restart and
    # append. Earlier durable records must survive each incomplete tail.
    final_start = 8
    while True:
        length = struct.unpack_from("<I", pristine, final_start)[0]
        end = final_start + 8 + length
        if end == len(pristine):
            break
        final_start = end
    final = pristine[final_start:]
    tail_checks = 0
    for cut in range(1, len(final)):
        journal.write_bytes(pristine + final[:cut])
        assert dump() == values, f"Interrupted tail affected snapshot at {cut}"
        subprocess.run(command("write", "nativeconcurrent", "recovery-", "1"), check=True, stdout=subprocess.DEVNULL)
        assert set(dump()) == expected | {"recovery-0"}
        tail_checks += 1
    corrupt = bytearray(pristine)
    corrupt[-1] ^= 1
    journal.write_bytes(corrupt)
    for _ in executables:
        rejected = subprocess.run(command("write", "nativeconcurrent", "must-not-write-", "1"), capture_output=True)
        assert rejected.returncode == 1 and b"checksum" in rejected.stderr
        assert journal.read_bytes() == corrupt, "Corruption was silently overwritten"
report = dict(platform="ARM64 + ARM64EC Windows" if mixed else "ARM64 Windows" if windows else "Linux", writers=4, retained_entries=48,
              interrupted_tail_positions=tail_checks, checksum_corruption_rejected=True)
report['executable_hashes'] = {str(exe.relative_to(ROOT)): hashlib.sha256(exe.read_bytes()).hexdigest() for exe in executables}
report['held_sidecar_reader_check'] = not windows
report['user_store_source_sha256'] = hashlib.sha256((ROOT/'native/UserStore.cpp').read_bytes()).hexdigest()
(ROOT / "build" / ("user-store-mixed-arm64ec.json" if mixed else "user-store-arm64.json" if windows else "user-store-linux.json")).write_text(json.dumps(report, indent=2))
print(json.dumps(report))
