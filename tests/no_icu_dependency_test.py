"""Audit the release payload's PE imports; requires GNU objdump in WSL."""
import argparse
import json
import re
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('package', type=Path)
args = parser.parse_args()
files = ['x64/Tigirl.dll', 'x86/Tigirl.dll', 'x64/Tigirl.exe',
         'x64/Tigirl.Import.exe', 'x64/Tigirl.Reminder.exe']
result = {}
for name in files:
    output = subprocess.check_output(['objdump', '-p', str(args.package / name)], text=True)
    imports = re.findall(r'DLL Name:\s*(\S+)', output)
    assert imports, f'No PE import table: {name}'
    assert not any('icu' in dll.lower() for dll in imports), (name, imports)
    result[name] = imports
print(json.dumps({'status': 'passed', 'imports': result}, indent=2))
