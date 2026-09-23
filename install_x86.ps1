param([switch]$CheckOnly, [switch]$Elevated)
$ErrorActionPreference = 'Stop'
# Supplemental x86 in-process server. The ARM64X installation remains primary.
$record = Get-Content -Raw "$PSScriptRoot\build\native-install.json" | ConvertFrom-Json
$clsid = '{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}'
$key = "Registry::HKEY_CLASSES_ROOT\CLSID\$clsid\InprocServer32"
if ((Get-Item $key).GetValue('') -ne $record.dll) { throw 'Primary installation record is stale.' }
$primary = Split-Path $record.dll -Parent
foreach ($entry in $record.artifacts.PSObject.Properties) {
    if ((Get-FileHash -LiteralPath (Join-Path $primary $entry.Name)).Hash -ne $entry.Value) { throw "Primary artifact changed: $($entry.Name)" }
}
$source = "$PSScriptRoot\build\Win32\Release\Tigirl.dll"
$probe = "$PSScriptRoot\build\tests\Win32\architecture_load_probe.exe"
foreach ($file in @($source, $probe)) {
    $bytes = [IO.File]::ReadAllBytes($file)
    $pe = [BitConverter]::ToInt32($bytes, 60)
    if ([BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x14c) { throw "Not x86: $file" }
}
$loaded = (& $probe $source) | ConvertFrom-Json
if ($LASTEXITCODE -ne 0 -or !$loaded.loaded -or !$loaded.class_instance) { throw 'x86 loader/class check failed.' }
$hash = (Get-FileHash $source).Hash
$destination = "$env:ProgramFiles\Tigirl\versions\x86-$($record.generation)-$($hash.Substring(0,16).ToLowerInvariant())"
$dll = Join-Path $destination 'Tigirl.dll'
$base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::ClassesRoot,[Microsoft.Win32.RegistryView]::Registry32)
$old = $base.OpenSubKey("CLSID\$clsid\InprocServer32")
$previousDll = if ($old) { $old.GetValue('') } else { $null }
if ($previousDll -and $previousDll -ne $dll) {
    $previous = Get-Content -Raw "$PSScriptRoot\build\native-install-x86.json" | ConvertFrom-Json
    if ($previous.dll -ne $previousDll -or (Get-FileHash -LiteralPath $previousDll).Hash -ne $previous.dll_hash) { throw 'Existing x86 registration does not match the owned installation record.' }
}
if ($CheckOnly) {
    @{status='validated'; dll=$dll; dll_hash=$hash; primary_generation=$record.generation; previous_dll=$previousDll; migration_validated=$true; loader=$loaded} | ConvertTo-Json -Depth 5
    return
}
$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    if ($Elevated) { throw 'Administrator privileges were not granted.' }
    $child = Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -Wait -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$PSCommandPath+'"'),'-Elevated')
    exit $child.ExitCode
}
New-Item -ItemType Directory -Force $destination | Out-Null
if (Test-Path $dll) {
    if ((Get-FileHash $dll).Hash -ne $hash) { throw 'Immutable x86 DLL collision.' }
} else { Copy-Item $source $dll }
# Hard links retain one backing file for the dictionary across x86/ARM64/x64.
foreach ($entry in $record.artifacts.PSObject.Properties) {
    if ($entry.Name -eq 'Tigirl.dll') { continue }
    $target = Join-Path $destination $entry.Name
    if (Test-Path -LiteralPath $target) {
        if ((Get-FileHash -LiteralPath $target).Hash -ne $entry.Value) { throw "Immutable companion collision: $($entry.Name)" }
    } else {
        New-Item -ItemType Directory -Force (Split-Path $target -Parent) | Out-Null
        New-Item -ItemType HardLink -Path $target -Target (Join-Path $primary $entry.Name) | Out-Null
    }
}
$nowKey = $base.OpenSubKey("CLSID\$clsid\InprocServer32")
$nowDll = if ($nowKey) { $nowKey.GetValue('') } else { $null }
if ($nowDll -ne $previousDll) { throw 'x86 registration changed during installation preparation.' }
if ($previousDll -and $previousDll -ne $dll) {
    $snapshot = "$PSScriptRoot\build\previous-install-x86-$($hash.Substring(0,16).ToLowerInvariant()).json"
    if (!(Test-Path $snapshot)) { $previous | ConvertTo-Json -Depth 6 | Set-Content $snapshot -Encoding UTF8 }
}
$process = Start-Process "$env:windir\SysWOW64\regsvr32.exe" -ArgumentList @('/s',('"'+$dll+'"')) -Wait -PassThru
if ($process.ExitCode -ne 0) { throw "x86 registration failed: $($process.ExitCode)" }
$registered = $base.OpenSubKey("CLSID\$clsid\InprocServer32").GetValue('')
if ($registered -ne $dll -or (Get-Item $key).GetValue('') -ne $record.dll) { throw 'Registration verification failed.' }
$installed = (& $probe $dll) | ConvertFrom-Json
if ($LASTEXITCODE -ne 0 -or !$installed.loaded -or !$installed.class_instance) { throw 'Installed x86 loader failed.' }
@{status='installed';dll=$dll;dll_hash=$hash;primary_generation=$record.generation;primary_dll=$record.dll;loader=$installed;shared_artifacts=$record.artifacts} | ConvertTo-Json -Depth 6 | Set-Content "$PSScriptRoot\build\native-install-x86.json" -Encoding UTF8
Write-Output "Installed x86 supplement: $dll"
