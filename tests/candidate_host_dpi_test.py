"""Hidden windows on every attached monitor, including mixed DPI when available."""
from pathlib import Path
import subprocess
from work_directory import temporary_work_directory
ROOT = Path(__file__).resolve().parents[1]
with temporary_work_directory(prefix='tigirl-host-dpi-') as work:
    exe = work / 'candidate_host_dpi_probe.exe'
    subprocess.run(['cl', '/nologo', '/std:c++17', '/EHsc', '/utf-8',
                    str(ROOT / 'tests/candidate_host_dpi_probe.cpp'),
                    f'/Fe:{exe}', '/link', 'user32.lib'], cwd=work, check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
