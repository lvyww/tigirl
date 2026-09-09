param([switch]$DirectActions, [switch]$Arm64X, [switch]$X64Host, [switch]$Win32Host)
$ErrorActionPreference = 'Stop'
if ($X64Host -and !$Arm64X) { throw '-X64Host requires -Arm64X' }
if (!$DirectActions) {
    Add-Type @'
using System; using System.Text; using System.Runtime.InteropServices;
public static class ManagementDesktop {
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern IntPtr OpenInputDesktop(uint flags,bool inherit,uint access);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool GetUserObjectInformation(IntPtr handle,int index,StringBuilder name,int length,out int needed);
 [DllImport("user32.dll")] public static extern bool CloseDesktop(IntPtr handle);
}
'@
    $desktop = [ManagementDesktop]::OpenInputDesktop(0,$false,1)
    $name = New-Object Text.StringBuilder 256
    $needed = 0
    try {
        if ($desktop -eq [IntPtr]::Zero -or ![ManagementDesktop]::GetUserObjectInformation($desktop,2,$name,512,[ref]$needed)) { throw 'Cannot inspect the input desktop; popup test remains unverified' }
        if ($name.ToString() -ne 'Default' -or [ManagementDesktop]::GetForegroundWindow() -eq [IntPtr]::Zero) { throw ('Popup test remains unverified: input desktop is ' + $name.ToString() + '; an unlocked interactive desktop is required') }
    } finally { if ($desktop -ne [IntPtr]::Zero) { [void][ManagementDesktop]::CloseDesktop($desktop) } }
}
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'build'
$isolated = Join-Path $build ('management-menu-' + [Guid]::NewGuid().ToString('N'))
$hostPlatform = if ($Win32Host) { 'Win32' } elseif ($X64Host) { 'x64' } else { 'ARM64' }
$exe = Join-Path $build "tests\$hostPlatform\tsf_host.exe"
$dll = Join-Path $build $(if ($Arm64X) { 'ARM64X\ARM64EC\Release\SampleIME.dll' } else { 'ARM64\Release\SampleIME.dll' })
$manager = Join-Path (Split-Path $dll -Parent) 'schema_manager.exe'
$manifest = Join-Path (Split-Path $dll -Parent) 'NativeTiger.Test.manifest'
$registration = 'Registry::HKEY_CLASSES_ROOT\CLSID\{D2291A80-84D8-4641-9AB2-BDD1472C846B}\InprocServer32'
$before = (Get-Item $registration).GetValue('')
New-Item -ItemType Directory -Path $isolated | Out-Null
New-Item -ItemType File -Path (Join-Path $isolated '.tsf-management-test') | Out-Null
if ($Win32Host) {
    $package = Join-Path $isolated 'package'
    New-Item -ItemType Directory -Path $package | Out-Null
    Copy-Item -LiteralPath (Join-Path $build 'Win32\Release\SampleIME.dll') -Destination $package
    foreach ($file in @('schema_manager.exe','schema_select.exe','lexicon_import.exe','tiger-v2.tcd')) {
        Copy-Item -LiteralPath (Join-Path $build ('ARM64X\ARM64EC\Release\' + $file)) -Destination $package
    }
    $dll = Join-Path $package 'SampleIME.dll'
    $manager = Join-Path $package 'schema_manager.exe'
    $manifest = Join-Path $package 'NativeTiger.Test.manifest'
}
$config = Join-Path $isolated 'config.txt'
'最大码长 4' | Set-Content -LiteralPath $config -Encoding UTF8
$configBefore = (Get-FileHash $config).Hash
$manifestText = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'NativeTiger.Test.manifest') -Raw
if ($Win32Host) { $manifestText = $manifestText.Replace('processorArchitecture="arm64"', 'processorArchitecture="x86"') }
if ($X64Host) { $manifestText = $manifestText.Replace('processorArchitecture="arm64"', 'processorArchitecture="amd64"') }
$manifestText | Set-Content -LiteralPath $manifest -Encoding UTF8
$stdout = Join-Path $isolated 'host.json'
$stderr = Join-Path $isolated 'host.stderr.txt'
$mode = if ($DirectActions) { '--management-launch' } else { '--management-menu' }
$arguments = '"' + $dll + '" - "' + $manifest + '" ' + $mode + ' "' + $isolated + '"'
$process = Start-Process -FilePath $exe -ArgumentList $arguments -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
$processHandle = $process.Handle # Retain the native handle before the fast child can exit.
try {
    if (!$process.WaitForExit(45000)) { throw 'Management menu fixture timed out' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw ('Management menu fixture failed (exit ' + $process.ExitCode + '): ' + (Get-Content -LiteralPath $stderr -Raw)) }
    if ((Get-Item $registration).GetValue('') -ne $before) { throw 'Profile registration changed' }
    if ((Get-FileHash $config).Hash -ne $configBefore) { throw 'Closing management changed configuration' }
    $report = [ordered]@{
        result = (Get-Content -LiteralPath $stdout -Raw | ConvertFrom-Json)
        dll_sha256 = (Get-FileHash $dll).Hash
        host_sha256 = (Get-FileHash $exe).Hash
        manager_sha256 = (Get-FileHash $manager).Hash
        configuration_unchanged = $true
        registration_unchanged = $true
        artifacts = $isolated
    }
    $reportName = if ($DirectActions) { 'management-actions-arm64.json' } else { 'management-menu-arm64.json' }
    if ($Win32Host) { $reportName = $reportName.Replace('-arm64.json', '-Win32.json') }
    elseif ($Arm64X) { $reportName = $reportName.Replace('-arm64.json', "-arm64x-$hostPlatform.json") }
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build $reportName) -Encoding UTF8
    $report | ConvertTo-Json -Depth 8
} finally {
    if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $process.Dispose()
}
