"""Four continuously pumping native TSF readers observe one shared user root."""
import hashlib
import json
import os
from pathlib import Path
import selectors
import shutil
import subprocess
import tempfile
import time
import sys
from tsf_architectures import variants

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
HOST = BUILD / 'tests/ARM64/tsf_host.exe'
WRITER = BUILD / 'tests/ARM64/user_store_probe.exe'
SAVER = BUILD / 'tests/ARM64/config_store_probe.exe'
DLL = BUILD / 'ARM64/Release/Tigirl.dll'
ARM64X = '--arm64x' in sys.argv
DLL, HOSTS = variants(ARM64X)
PS = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'


def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def registration():
    command = "(Get-Item 'Registry::HKEY_CLASSES_ROOT\\CLSID\\{D2291A80-84D8-4641-9AB2-BDD1472C846B}\\InprocServer32').GetValue('')"
    return subprocess.check_output([PS, '-NoProfile', '-Command', command]).strip()


def publish(path, text):
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(text, encoding='utf-8')
    deadline = time.monotonic() + 3
    while True:
        try:
            os.replace(temporary, path)
            break
        except PermissionError:
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.01)


def receive(process):
    with selectors.DefaultSelector() as selector:
        selector.register(process.stdout, selectors.EVENT_READ)
        assert selector.select(15), f'No response from reader {process.pid}'
        line = process.stdout.readline().strip()
    assert line, (process.poll(), process.stderr.read())
    return line


before = registration()
observations = []
with tempfile.TemporaryDirectory(prefix='tsf-live-readers-', dir=BUILD) as temporary:
    base = Path(temporary)
    user = base / 'user'
    control = base / 'control'
    user.mkdir()
    control.mkdir()
    (user / '.tsf-live-test').touch()
    schema = user / 'schemas/SharedTest'
    schema.mkdir(parents=True)
    shutil.copyfile(DLL.parent / 'tiger-v2.tcd', schema / 'tiger-v2.tcd')
    readers = []
    try:
        for i in range(4):
            platform, host, manifest = HOSTS[i % len(HOSTS)]
            process = subprocess.Popen([str(host), win(DLL), '-', win(manifest), '--live-reader', win(user)],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
            readers.append(process)
            assert receive(process) == 'ready'
        for stage in range(1, 9):
            generation = '-'
            if stage in (1, 3, 4):
                name = 'SharedTest' if stage == 4 else '虎码字词'
                size = 1 if stage == 1 else 2
                publish(user / 'config.txt', f'当前码表 {name}\n每页候选个数 {size}\n')
            elif stage in (2, 5):
                dictionary = DLL.parent / 'tiger-v2.tcd' if stage == 2 else schema / 'tiger-v2.tcd'
                journal = user / 'user/tiger-words.tcu' if stage == 2 else schema / 'user.tcu'
                subprocess.run([str(WRITER), win(dictionary), win(journal), 'write', 'ab', 'live', '1'],
                               check=True, capture_output=True, timeout=10)
            elif stage == 6:
                generation = 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'
                target = schema / 'generations' / generation
                target.mkdir(parents=True)
                shutil.copyfile(schema / 'tiger-v2.tcd', target / 'tiger-v2.tcd')
                publish(schema / 'current.txt', f'generation\t{generation}\n')
            elif stage == 7:
                subprocess.run([str(SAVER), win(user / 'config.txt'), '1', 'save-english'],
                               check=True, capture_output=True, timeout=10)
            else:
                publish(user / 'config.txt', (user / 'config.txt').read_text(encoding='utf-8-sig'))
                publish(user / 'unrelated.txt', 'duplicate notification')
            count, pages = {1: (4, 4), 2: (5, 5), 3: (5, 3), 4: (4, 2), 5: (5, 3), 6: (5, 3), 7: (0, 0), 8: (5, 3)}[stage]
            publish(control / f'expected-{stage}.txt', f'{stage} {count} {pages} {generation}\n')
            received = [json.loads(receive(process)) for process in readers]
            assert len({result['pid'] for result in received}) == 4
            for result in received:
                assert (result['stage'], result['count'], result['pages'], result['raw_preserved']) == (stage, count, pages, stage != 7), result
                if stage == 7:
                    assert result['reload_cancelled'] and result['default_english_applied'], result
            observations.extend(received)
            print(f'Stage {stage}: all four live TSF readers updated', flush=True)
        (control / 'stop').touch()
        for process in readers:
            assert process.wait(timeout=10) == 0, process.stderr.read()
    finally:
        (control / 'stop').touch()
        for process in readers:
            if process.poll() is None:
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()

after = registration()
assert before == after, 'Private reader test changed installed registration'
report = dict(status='passed', actual_tsf_readers=4, continuously_pumping=True,
              user_word_labels_verified=True,
              stages=8, observations=observations, physical_keyboard_or_rendering_measured=False,
              explicit_save_cancels_and_applies_default=True, duplicate_save_preserves_new_input=True,
              saver_sha256=hashlib.sha256(SAVER.read_bytes()).hexdigest(),
              registration_unchanged=True,
              dll_sha256=hashlib.sha256(DLL.read_bytes()).hexdigest(),
              host_sha256=hashlib.sha256(HOST.read_bytes()).hexdigest(),
              writer_sha256=hashlib.sha256(WRITER.read_bytes()).hexdigest())
report['architecture_mix'] = [HOSTS[i % len(HOSTS)][0] for i in range(4)]
report['host_hashes'] = {platform: hashlib.sha256(host.read_bytes()).hexdigest() for platform, host, _ in HOSTS}
(BUILD / ('tsf-live-readers-arm64x.json' if ARM64X else 'tsf-live-readers-arm64.json')).write_text(json.dumps(report, indent=2) + '\n')
print('Four-reader live settings/journal/schema/generation propagation passed.')
