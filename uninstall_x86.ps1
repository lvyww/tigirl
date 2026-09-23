param([switch]$CheckOnly, [switch]$Elevated)
$ErrorActionPreference = 'Stop'
$record = Get-Content -Raw "$PSScriptRoot\build\native-install-x86.json" | ConvertFrom-Json
$clsid = '{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}'
$base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::ClassesRoot,[Microsoft.Win32.RegistryView]::Registry32)
$subkey = "CLSID\$clsid"
$key = $base.OpenSubKey("$subkey\InprocServer32")
$current = if ($key) { $key.GetValue('') } else { $null }
if ($current -and $current -ne $record.dll) { throw 'Another x86 generation is registered.' }
if ($current -and (Get-FileHash -LiteralPath $current).Hash -ne $record.dll_hash) { throw 'Registered x86 DLL hash differs from the record.' }
if ($CheckOnly) {
    @{status='validated';registered=!!$current;dll=$record.dll;preserve_primary_profile=$true;preserve_user_data=$true;preserve_binary_files=$true} | ConvertTo-Json
    return
}
if (!$current) { return }
$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    if ($Elevated) { throw 'Administrator privileges were not granted.' }
    $child = Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -Wait -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$PSCommandPath+'"'),'-Elevated')
    exit $child.ExitCode
}
# Do not call DllUnregisterServer: the TSF profile/categories are shared with
# the primary installation. Remove only this matching 32-bit COM server.
$base.DeleteSubKeyTree($subkey)
if ($base.OpenSubKey($subkey)) { throw 'x86 COM registration removal failed.' }
Write-Output 'Removed x86 COM registration; primary profile and all data retained.'
