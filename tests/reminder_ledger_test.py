"""Durable reminder ordering with independent native ARM64 processes."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import uuid

root = Path(__file__).resolve().parents[1]
exe = root / 'build/tests/ARM64/reminder_ledger_probe.exe'

def run(path, *args, check=True):
    target = subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()
    result = subprocess.run([str(exe), target, *map(str, args)], text=True, capture_output=True, timeout=15)
    if check:
        assert result.returncode == 0, result.stderr
    return result

with tempfile.TemporaryDirectory(prefix='reminder-ledger-', dir=root / 'build') as temporary:
    base = Path(temporary)
    path = base / 'timer.txt'
    first = run(path, 'reserve', 1).stdout.split()
    epoch = first[0]
    uuid.UUID(epoch)
    assert first[1] == '1'
    assert run(path, 'accept', epoch, 1, 12345).stdout.strip() == '1'
    target = subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()
    jobs = []
    try:
        for _ in range(4):
            jobs.append(subprocess.Popen([str(exe), target, 'reserve', '25'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True))
        tickets = []
        for job in jobs:
            out, err = job.communicate(timeout=30)
            assert job.returncode == 0, err
            tickets.extend(row.split() for row in out.splitlines())
    finally:
        for job in jobs:
            if job.poll() is None:
                job.kill(); job.wait()
    assert len(tickets) == 100 and {int(row[1]) for row in tickets} == set(range(2, 102))
    assert {row[0] for row in tickets} == {epoch}
    assert run(path, 'read').stdout.split() == [epoch, '101', '1', '12345'], 'Reservation cancelled accepted timer'
    assert run(path, 'accept', epoch, 101, 54321).stdout.strip() == '1'
    before = path.read_bytes()
    # Each call starts a new process, after all prior owners have exited.
    for ticket in [1, 50, 101, 102]:
        assert run(path, 'accept', epoch, ticket, 0).stdout.strip() == '0'
        assert path.read_bytes() == before
    assert run(path, 'deny-reserve').stdout.strip() == 'blocked'
    assert path.read_bytes() == before and not list(base.glob('timer.txt.tmp.*'))
    for invalid in [b'', b'version 999\n', before.replace(b'allocated\t101', b'allocated\t18446744073709551615'),
                    before.replace(b'accepted\t101', b'accepted\t102')]:
        path.write_bytes(invalid)
        assert run(path, 'reserve', 1, check=False).returncode != 0
        assert path.read_bytes() == invalid
    path.unlink()
    replacement = run(path, 'reserve', 1).stdout.split()
    assert replacement[0] != epoch and replacement[1] == '1'
    before = path.read_bytes()
    assert run(path, 'accept', epoch, 101, 0).stdout.strip() == '0'
    assert path.read_bytes() == before, 'Old epoch replaced recreated ledger'

report = dict(status='passed', concurrent_reservations=100, writers=4,
              reservations_preserve_active_timer=True, stale_after_all_processes_exit_rejected=True,
              duplicate_and_unreserved_ticket_rejected=True, denied_replace_preserves_counter=True,
              malformed_and_overflow_rejected=True, recreated_epoch_rejects_old_ticket=True,
              helper_and_tsf_integrated=False, exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
(root / 'build/reminder-ledger-arm64.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
