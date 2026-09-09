"""Real-table hidden TSF key/commit timing, with original-engine output checks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import tempfile
from windows_process import run_windows, PS
from tsf_architectures import ROOT, variants

BUILD = ROOT / 'build'
REFERENCE = Path('/mnt/c/Users/yc/Desktop/bime_codex_src_20260513/release_arm64')


def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def run(args):
    result = run_windows(args, capture_output=True, text=True, timeout=180)
    assert result.returncode == 0, (result.stdout, result.stderr)
    return result.stdout


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['ARM64', 'x64', 'Win32'])
    args = parser.parse_args()
    for name in ['upstream-sha256.json', 'engine-dependencies-sha256.json']:
        data = json.loads((ROOT / 'tools/SentenceOracle' / name).read_text())
        for source_name, digest in data.get('files', data).items():
            assert sha(ROOT / 'tools/SentenceOracle/upstream' / source_name) == digest, source_name
    stage = Path(tempfile.mkdtemp(prefix='sentence-tsf-measure-', dir=BUILD))
    (stage / '.native-tiger-staging').touch()
    source = stage / '码表/测试整句'
    source.mkdir(parents=True)
    table_hashes = {}
    for p in (REFERENCE / '码表/虎整句').glob('*.txt'):
        shutil.copyfile(p, source / p.name)
        table_hashes[p.name] = sha(p)
    codes = {}
    for line in (source / '虎整句.txt').read_text(encoding='utf-8-sig').splitlines():
        parts = line.split()
        if len(parts) >= 2 and len(parts[0]) == 1 and parts[1].isascii() and parts[1].isalpha():
            codes.setdefault(parts[0], parts[1])
    phrase = '中国人民使用中文输入法今天我们一起学习新的知识这个问题需要进一步分析和解决'
    raw = ''.join(codes[c] for c in phrase)
    raw = (raw * (128 // len(raw) + 1))[:128]
    samples = [raw[:n] for n in [32, 64, 128]]
    settings = '当前码表\t测试整句\n默认中文\t是\n整句自动提前上屏\t否\n高频字仅使用最优码组句\t1500\n开机自动启动\t否\n'
    (stage / 'config.txt').write_text(settings, encoding='utf-8-sig')
    (stage / 'Models').mkdir()
    os.link(REFERENCE / 'Models/sentence-ngram-v2.bin', stage / 'Models/sentence-ngram-v2.bin')
    keys = []
    for sample in samples:
        for i, code in enumerate(sample + ' '):
            for action in ['down', 'up']:
                keys.append(dict(vk=32 if code == ' ' else ord(code.upper()), action=action,
                                 reset=i == 0 and action == 'down'))
    trace = stage / 'oracle-keys.jsonl'
    trace.write_text('\n'.join(json.dumps(k) for k in keys))
    oracle = ROOT / 'tools/SentenceEngineOracle/bin/Release/net10.0-windows/SentenceEngineOracle.dll'
    output = run(['/mnt/c/Program Files/dotnet/dotnet.exe', win(oracle), win(stage), win(trace)])
    (stage / 'oracle-output.jsonl').write_text(output)
    expected = [json.loads(row)['result']['TextToOutput'] for key, row in zip(keys, output.splitlines())
                if key['vk'] == 32 and key['action'] == 'down']
    assert len(expected) == 3 and all(expected), expected
    user = stage / 'native'
    run([BUILD / 'tests/ARM64/lexicon_import.exe', '--schema', win(source), win(stage / 'pinyin'), win(user), '测试整句', 'zh-CN'])
    (user / 'config.txt').write_text(settings, encoding='utf-8-sig')
    cases = [(pause, sample) for _ in range(3) for sample in samples for pause in [0, 30]]
    (user / '.sentence-measure.tsv').write_text('\n'.join(f'{pause} {sample}' for pause, sample in cases))
    dll, hosts = variants(True)
    selected = [(platform, host, manifest, dll) for platform, host, manifest in hosts]
    x86 = stage / 'x86-package'
    x86.mkdir()
    package = json.loads((BUILD / 'native-package-arm64x-preflight.json').read_text())['package']
    for name, digest in package['artifacts'].items():
        relative = Path(name.replace('\\', '/'))
        original = dll.parent / relative
        assert sha(original) == digest.lower(), name
        if name != 'SampleIME.dll':
            target = x86 / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            os.link(original, target)
    shutil.copyfile(BUILD / 'Win32/Release/SampleIME.dll', x86 / 'SampleIME.dll')
    manifest = x86 / 'NativeTiger.x86.manifest'
    manifest.write_text((ROOT / 'tests/NativeTiger.Test.manifest').read_text().replace('processorArchitecture="arm64"', 'processorArchitecture="x86"'))
    selected.append(('Win32', BUILD / 'tests/Win32/tsf_host.exe', manifest, x86 / 'SampleIME.dll'))
    results = []
    quote = lambda value: "'" + str(value).replace("'", "''") + "'"
    for platform, host, manifest, binary in selected:
        if args.platform and platform != args.platform:
            continue
        command = '$env:NATIVE_TIGER_USER_ROOT=' + quote(win(user)) + '; & ' + ' '.join(quote(win(p)) for p in [host, binary]) + " '-' " + quote(win(manifest)) + " '--sentence-measure' | Out-String -Stream; exit $LASTEXITCODE"
        output = run([PS, '-NoProfile', '-Command', command])
        (stage / f'{platform}.jsonl').write_text(output)
        rows = [json.loads(line) for line in output.splitlines()]
        assert rows.pop()['status'] == 'sentence-tsf-measured'
        assert len(rows) == len(cases)
        for (pause, sample), row in zip(cases, rows):
            assert row['raw_length'] == len(sample) and row['pause_ms'] == pause
            actual = b''.join(n.to_bytes(2, 'little') for n in row['committed_utf16']).decode('utf-16-le')
            assert actual == expected[samples.index(sample)], (platform, sample, actual, expected)
        medians = []
        for length in [32, 64, 128]:
            for pause in [0, 30]:
                group = [r for r in rows if r['raw_length'] == length and r['pause_ms'] == pause]
                medians.append(dict(raw_length=length, pause_ms=pause,
                                    **{key: statistics.median(r[key] for r in group) for key in ['key_p95_ms', 'key_max_ms', 'commit_ms', 'private_active', 'private_after_commit']}))
        results.append(dict(platform=platform, dll_sha256=sha(binary), host_sha256=sha(host), medians=medians))
        print(json.dumps(results[-1]), flush=True)
    report = dict(status='measured', installed=False, physical_input=False, stage=str(stage),
                  scope='Hidden real TSF edit sessions, original-engine checked output; three repeats, automatic off, warm cache; each timed key includes two preview callbacks plus down/up.',
                  limitations=['No real Word/WeChat application scheduling or physical input.', 'Private bytes include process overhead and allocator retention.', 'Three fixed raw prefixes, not general typing latency.'],
                  source_tables=table_hashes, oracle_sha256=sha(oracle), results=results,
                  model_sha256=sha(REFERENCE / 'Models/sentence-ngram-v2.bin'),
                  input_codes='First encountered single-character code in supplied source table; repeated phrase prefixes of 32/64/128 codes.',
                  sources={name: sha(ROOT / name) for name in ['native/SentenceDecoder.cpp', 'native/tsf/SentenceService.cpp', 'tests/sentence_tsf_fixture.h', 'tests/sentence_tsf_measure_test.py']})
    (BUILD / ('sentence-tsf-measure-' + (args.platform or 'all') + '.json')).write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
