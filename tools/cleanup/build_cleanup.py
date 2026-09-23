"""Build a standalone BAT with a UTF-8 PowerShell payload; no runtime sidecars."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
loader="& ([scriptblock]::Create(([IO.File]::ReadAllText($env:TIGIRL_CLEAN_SELF,[Text.Encoding]::UTF8) -split '(?m)^###POWERSHELL###\\r?$',2)[1])) -CheckOnly:($env:TIGIRL_CLEAN_CHECK -eq '/check')"
# Elevation reopens this same file with a quoted literal path, rather than trusting
# environment inheritance across credential/UAC boundaries.
bootstrap="$ErrorActionPreference='Stop'; $p=$env:TIGIRL_CLEAN_SELF; $check=$env:TIGIRL_CLEAN_CHECK -eq '/check'; $admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator); if($admin -or $check){ "+loader+" } else { $literal=$p.Replace(\"'\",\"''\"); $code=\"`$env:TIGIRL_CLEAN_SELF='$literal'; `$env:TIGIRL_CLEAN_CHECK=''; \" + '"+loader.replace("'","''")+"'; $encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($code)); try{$child=Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -Verb RunAs -ArgumentList ('-NoProfile -OutputFormat Text -ExecutionPolicy Bypass -EncodedCommand '+$encoded) -Wait -PassThru; exit $child.ExitCode}catch{Write-Host $_;exit 1} }"
# Keep the command ASCII and avoid nested cmd double quotes by base64 encoding.
import base64
encoded=base64.b64encode(bootstrap.encode('utf-16-le')).decode()
header=f'''@echo off
setlocal DisableDelayedExpansion
set "TIGIRL_CLEAN_SELF=%~f0"
set "TIGIRL_CLEAN_CHECK=%~1"
set "TIGIRL_CLEAN_PS=%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"
if exist "%SystemRoot%\\Sysnative\\WindowsPowerShell\\v1.0\\powershell.exe" set "TIGIRL_CLEAN_PS=%SystemRoot%\\Sysnative\\WindowsPowerShell\\v1.0\\powershell.exe"
"%TIGIRL_CLEAN_PS%" -NoProfile -OutputFormat Text -ExecutionPolicy Bypass -EncodedCommand {encoded}
set "TIGIRL_CLEAN_RESULT=%errorlevel%"
echo.
echo Cleanup exit code: %TIGIRL_CLEAN_RESULT% (0=done, 2=cancelled, 3010=restart required, 1=incomplete).
echo Logs: %%TEMP%%\\Tigirl-Cleanup-*.log (account used for elevation).
if /i not "%~1"=="/check" pause
exit /b %TIGIRL_CLEAN_RESULT%
###POWERSHELL###
'''
payload=(Path(__file__).parent/'cleanup.ps1').read_text(encoding='utf-8-sig')
payload=payload.replace('# LEGACY_IDENTITY_HELPER', (ROOT/'packaging/legacy_identity.ps1').read_text(encoding='utf-8-sig'))
output=ROOT/'Tigirl-Cleanup.bat'
output.write_bytes((header+payload).replace('\r\n','\n').replace('\n','\r\n').encode('utf-8'))
print(output)
