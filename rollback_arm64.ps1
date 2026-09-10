param([Parameter(Mandatory=$true)][string]$Snapshot, [switch]$CheckOnly, [switch]$Elevated)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'shortcut_arm64.ps1')
$Snapshot = [IO.Path]::GetFullPath($Snapshot)
$record = Get-Content -LiteralPath $Snapshot -Raw | ConvertFrom-Json
$clsid = '{D2291A80-84D8-4641-9AB2-BDD1472C846B}'
if ($record.clsid -ne $clsid) { throw 'Snapshot belongs to another input method.' }
$dll = [IO.Path]::GetFullPath($record.dll)
$root = "$env:ProgramFiles\Tigirl\"
if (!$dll.StartsWith($root, [StringComparison]::OrdinalIgnoreCase) -or
    [IO.Path]::GetFileName($dll) -ne 'Tigirl.dll' -or !(Test-Path $dll)) {
    throw 'Snapshot does not point to an installed Tigirl generation.'
}
if (!$record.hash -or $record.hash -notmatch '^[0-9a-fA-F]{64}$') { throw 'Rollback snapshot requires a SHA256 DLL hash.' }
if ((Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash -ne $record.hash) { throw 'Rollback DLL hash mismatch.' }
Assert-NativeTigerShortcutAvailable ([Environment]::GetFolderPath('CommonPrograms')) "$env:ProgramFiles\Tigirl"
if ($CheckOnly) { Write-Output "Validated rollback DLL: $dll"; exit 0 }
$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    if ($Elevated) { throw 'Administrator privileges were not granted.' }
    $process = Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -PassThru -Wait -ArgumentList @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"' + $PSCommandPath + '"'), '-Snapshot', ('"' + $Snapshot + '"'), '-Elevated')
    exit $process.ExitCode
}
if ([Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString() -ne 'Arm64') { throw 'Use native ARM64 PowerShell.' }
$process = Start-Process "$env:windir\System32\regsvr32.exe" -ArgumentList @('/s', ('"' + $dll + '"')) -PassThru -Wait
if ($process.ExitCode -ne 0) { throw "Registration failed: $($process.ExitCode)" }
$registered = (Get-Item "Registry::HKEY_CLASSES_ROOT\CLSID\$clsid\InprocServer32").GetValue('')
if ($registered -ne $dll) { throw 'Rollback registration verification failed.' }
$manager = Join-Path (Split-Path $dll -Parent) 'Tigirl.exe'
if (Test-Path -LiteralPath $manager -PathType Leaf) {
    Set-NativeTigerShortcut ([Environment]::GetFolderPath('CommonPrograms')) "$env:ProgramFiles\Tigirl" $manager | Out-Null
} else {
    Remove-NativeTigerShortcut ([Environment]::GetFolderPath('CommonPrograms')) "$env:ProgramFiles\Tigirl" | Out-Null
}
Write-Output "Restored: $dll. Restart applications to unload the previous generation."
