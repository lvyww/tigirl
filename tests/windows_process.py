"""Run native Windows test executables even when WSL binfmt is unavailable."""
import subprocess
from pathlib import Path
PS = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'

def run_windows(arguments, **kwargs):
    args = list(map(str, arguments))
    if args[0].startswith('/mnt/'):
        args[0] = subprocess.check_output(['wslpath', '-w', args[0]], text=True).strip()
    quote = lambda value: "'" + value.replace("'", "''") + "'"
    command = '& ' + ' '.join(map(quote, args)) + ' | Out-String -Stream; exit $LASTEXITCODE'
    return subprocess.run(['/init', PS, '-NoProfile', '-Command', command], **kwargs)
