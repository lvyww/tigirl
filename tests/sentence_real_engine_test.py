"""Run and audit real-table full-engine parity; never install or inject input."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
CASES = [
    ('ARM64', False, False, 3),
    ('ARM64', True, False, 3),
    ('ARM64', True, True, 3),
    ('ARM64', True, True, 0),
    ('x64', False, False, 3),
    ('x64', True, True, 3),
    ('Win32', False, False, 3),
    ('Win32', True, True, 3),
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--summarize-only', action='store_true',
                        help='Audit existing reports against current sources and binaries.')
    args = parser.parse_args()
    reports = []
    for platform, automatic, burst, retained in CASES:
        if not args.summarize_only:
            env = {k: v for k, v in os.environ.items() if not k.startswith('SENTENCE_TRACE_')}
            env.update(SENTENCE_TRACE_REAL='1', SENTENCE_TRACE_PLATFORM=platform,
                       SENTENCE_TRACE_RETAIN=str(retained))
            if automatic:
                env['SENTENCE_TRACE_AUTO'] = '1'
            if burst:
                env['SENTENCE_TRACE_BURST'] = '1'
            subprocess.run([sys.executable, str(ROOT / 'tests/sentence_engine_parity.py')],
                           env=env, cwd=ROOT, check=True)
        tag = ('real-' + ('burst-' if burst else '') +
               ('async-auto-' if automatic else '') +
               (f'retain{retained}-' if retained != 3 else '') +
               (platform + '-' if platform != 'ARM64' else ''))
        path = BUILD / f'sentence-engine-{tag}parity.json'
        report = json.loads(path.read_text())
        assert report['status'] == 'passed' and report['mismatches'] == 0, path
        assert report['real_scheme'] and report['common_character_limit'] == 1500, path
        assert report['platform'] == platform and report['controlled_bursts'] == burst, path
        assert report['minimum_retained_raw'] == retained, path
        assert not automatic or report['automatic_commit_events'] > 0, path
        for group in ['sources', 'binaries']:
            for name, digest in report[group].items():
                assert sha(ROOT / name) == digest, (path, name)
        reference = Path('/mnt/c/Users/yc/Desktop/bime_codex_src_20260513/release_arm64/码表/虎整句')
        for name, digest in report['source_tables'].items():
            assert sha(reference / name) == digest, (path, name)
        reports.append(dict(report=path.name, sha256=sha(path), platform=platform,
                            automatic=automatic, burst=burst, retained=retained,
                            events=report['events'], automatic_outputs=report['automatic_commit_events']))
    summary = dict(status='passed', cases=reports, installed=False,
                   scope='Full original engine with supplied real main/quick-symbol/supplement tables; 12 phrases, short/full codes, selection, edit, punctuation, cancellation. Reverse lookup uses a small ni fixture.',
                   limitations=['Same input fixtures replayed across configurations, not independent corpora.',
                                'Burst scheduling delays worker capture, not computed results in transit.',
                                'No installed TSF application or physical input acceptance.'])
    path = BUILD / 'sentence-real-engine-matrix.json'
    path.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
