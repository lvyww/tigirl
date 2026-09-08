"""Select real host binaries and matching private manifests for multi-host tests."""
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]

def variants(arm64x):
    dll = ROOT / ('build/ARM64X/ARM64EC/Release/SampleIME.dll' if arm64x else 'build/ARM64/Release/SampleIME.dll')
    template = (ROOT / 'tests/NativeTiger.Test.manifest').read_text()
    result = []
    for platform, architecture, machine in ([('ARM64', 'arm64', 0xaa64), ('x64', 'amd64', 0x8664)] if arm64x else [('ARM64', 'arm64', 0xaa64)]):
        host = ROOT / 'build/tests' / platform / 'tsf_host.exe'
        binary = host.read_bytes()
        assert struct.unpack_from('<H', binary, struct.unpack_from('<I', binary, 0x3c)[0] + 4)[0] == machine
        manifest = dll.parent / ('NativeTiger.' + architecture + '.manifest')
        manifest.write_text(template.replace('processorArchitecture="arm64"', 'processorArchitecture="' + architecture + '"'))
        result.append((platform, host, manifest))
    return dll, result
