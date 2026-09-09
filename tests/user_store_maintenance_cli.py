"""Real ARM64 maintenance tool: isolated journal, preserved schema configuration."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib
import argparse
import shutil

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT/'build/tests/ARM64/Tigirl.SchemaSelect.exe'
PS = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
COMMAND = [PS] if Path('/proc/sys/fs/binfmt_misc/WSLInterop').exists() else ['/init', PS]
parser = argparse.ArgumentParser()
parser.add_argument('--manager', action='store_true', help='Use hidden manager command handlers for compaction and recovery')
parser.add_argument('--imported', action='store_true', help='Maintain an imported schema using its current generation')
args = parser.parse_args()
MANAGER = ROOT/'build/tests/ARM64/Tigirl.exe'


def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def quote(value):
    return "'"+value.replace("'", "''")+"'"


with tempfile.TemporaryDirectory(prefix='maintenance-cli-', dir=ROOT/'build') as temporary:
    root = Path(temporary)
    (root/'.schema-manager-test').touch()
    (root/'user').mkdir()
    config = root/'config.txt'
    config_bytes = '当前码表\t虎码字词\n最大码长\t4\n'.encode('utf-8-sig')
    config.write_bytes(config_bytes)
    journal = root/'user/tiger-words.tcu'
    schema = '虎码字词'
    protected = []
    if args.imported:
        schema = '维护测试MiXeD'
        directory = root/'schemas'/schema
        generation = directory/'generations'/('a'*32)
        generation.mkdir(parents=True)
        shutil.copyfile(ROOT/'data/tiger-v2.tcd', generation/'tiger-v2.tcd')
        (directory/'current.txt').write_text('generation\t'+'a'*32+'\n', encoding='utf-8-sig')
        (directory/'tiger-v2.tcd').write_bytes(b'Unused legacy generation must not be loaded')
        journal.write_bytes(b'TIGERU01')
        protected.append(journal)
        journal = directory/'user.tcu'
        other = root/'schemas/Unselected/user.tcu'
        other.parent.mkdir()
        other.write_bytes(b'TIGERU01')
        protected.extend([other, directory/'current.txt', directory/'tiger-v2.tcd'])
    protected_before = {path: path.read_bytes() for path in protected}
    code, text = 'fixture'.encode('utf-16-le'), 'word'.encode('utf-16-le')
    payload = struct.pack('<III', 0, len(code)//2, len(text)//2)+code+text
    record = struct.pack('<II', len(payload), zlib.crc32(payload))+payload
    original = b'TIGERU01'+record*1000
    journal.write_bytes(original)

    def run(operation, *extra):
        if args.manager:
            before = journal.read_bytes() if journal.exists() else None
            mode = '--test-compact' if operation == '--compact-user' else '--test-recover-user'
            arguments = [win(root), win(ROOT/'data/tiger-v2.tcd'), mode, schema]
            command = ('$p=Start-Process -FilePath '+quote(win(MANAGER))+' -ArgumentList '+
                       quote(subprocess.list2cmdline(arguments))+' -PassThru -Wait; exit $p.ExitCode')
            if operation == '--recover-user':
                command = '$env:NATIVE_TIGER_TEST_BACKUP='+quote(extra[0])+'; '+command
            subprocess.run([*COMMAND, '-NoProfile', '-Command', command], check=True, capture_output=True, timeout=45)
            return {'changed': journal.read_bytes() != before}
        arguments = [win(TOOL), win(root), win(ROOT/'data/tiger-v2.tcd'), operation, schema.lower(), *extra]
        command = '& '+' '.join(quote(value) for value in arguments)+'; exit $LASTEXITCODE'
        return json.loads(subprocess.check_output([*COMMAND, '-NoProfile', '-Command', command], text=True, timeout=30))

    assert run('--compact-user')['changed']
    compacted = journal.read_bytes()
    assert compacted == b'TIGERU01'+record
    backups = list(journal.parent.glob(journal.name+'.compact-*.old'))
    assert len(backups) == 1 and backups[0].read_bytes() == original
    assert not run('--compact-user')['changed']
    journal.rename(root/'displaced.tcu')
    assert run('--recover-user', win(backups[0]))['changed']
    assert journal.read_bytes() == original and backups[0].read_bytes() == original
    assert not run('--recover-user', win(backups[0]))['changed']
    assert config.read_bytes() == config_bytes
    assert all(path.read_bytes() == value for path, value in protected_before.items())
report = {'status': 'passed', 'platform': 'Windows ARM64', 'compact_and_recover': True,
          'hidden_manager_compaction': args.manager, 'physical_mouse_tested': False,
          'hidden_manager_recovery': args.manager, 'backup_picker_tested': False,
          'existing_live_preserved': True, 'config_unchanged': True,
          'imported_current_generation': args.imported, 'unselected_schema_files_preserved': len(protected),
          'original_bytes': len(original), 'compacted_bytes': len(compacted),
          'tool_sha256': hashlib.sha256(TOOL.read_bytes()).hexdigest()}
if args.manager:
    report['manager_sha256'] = hashlib.sha256(MANAGER.read_bytes()).hexdigest()
report_name = 'user-store-maintenance-'+('manager' if args.manager else 'cli')+('-imported' if args.imported else '')+'.json'
(ROOT/'build'/report_name).write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report))
