param([switch]$Elevated, [switch]$CheckOnly, [switch]$Arm64X)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'shortcut_arm64.ps1')
. (Join-Path $PSScriptRoot 'packaging\legacy_identity.ps1')
. (Join-Path $PSScriptRoot 'sentence_package.ps1')
. (Join-Path $PSScriptRoot 'appcontainer_data.ps1')
Assert-NativeTigerShortcutAvailable ([Environment]::GetFolderPath('CommonPrograms')) "$env:ProgramFiles\Tigirl"
$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
$transcribing = $false
function Test-NativeTigerDeployment([string]$Directory, $Hashes, [switch]$Arm64X) {
    # Validate the exact bytes and loader path before changing shared registration.
    foreach ($file in $Hashes.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $Directory $file) -Algorithm SHA256).Hash -ne $Hashes[$file]) {
            throw "Deployment hash mismatch: $file"
        }
    }
    $checks = [ordered]@{}
    if ($Arm64X) {
        foreach ($variant in @('arm64', 'x64')) {
            $output = & (Join-Path $Directory "verify_load_$variant.exe") (Join-Path $Directory 'Tigirl.dll')
            if ($LASTEXITCODE -ne 0) { throw "ARM64X $variant loader verification failed: $LASTEXITCODE" }
            $loaded = $output | ConvertFrom-Json
            if (!$loaded.loaded -or !$loaded.class_instance) { throw "ARM64X $variant loader rejected DLL (Windows error $($loaded.load_error))." }
            $checks[$variant] = $loaded
        }
    }
    return $checks
}
try {
    if ([Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString() -ne 'Arm64') {
        throw 'Run this script with native ARM64 Windows PowerShell.'
    }
    $source = if ($Arm64X) { "$PSScriptRoot\build\ARM64X\ARM64EC\Release" } else { "$PSScriptRoot\build\ARM64\Release" }
    $architecture = if ($Arm64X) { 'ARM64X' } else { 'ARM64' }
    $clsid = '{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}'
    $profile = '{43201C7B-F615-469D-9D54-906D9270975E}'
    $tip = "0804:$clsid$profile"
    $fontFolder = [string][char]0x5b57 + [char]0x4f53
    $fontRoot = Join-Path $source $fontFolder
    foreach ($requiredFont in @('LXGWWenKaiGBScreen.ttf', 'OFL.txt', 'provenance.json')) {
        if (!(Test-Path -LiteralPath (Join-Path $fontRoot $requiredFont) -PathType Leaf)) { throw "Missing font artifact: $requiredFont" }
    }
    $executables = @('Tigirl.dll', 'Tigirl.SchemaSelect.exe', 'Tigirl.Import.exe', 'Tigirl.exe', 'Tigirl.Reminder.exe')
    if ($Arm64X) { $executables += @('verify_load_arm64.exe', 'verify_load_x64.exe') }
    Assert-NativeTigerSentenceModel (Join-Path $source 'Models\sentence-fivegram-mobile.bin')
    Assert-NativeTigerSentenceLexicalPrior (Join-Path $source 'Models\sentence-lexical-v1.bin')
    $artifacts = $executables + @('tiger-v2.tcd', 'Models\sentence-fivegram-mobile.bin', 'Models\sentence-lexical-v1.bin', 'Models\provenance.json') + @(Get-ChildItem -LiteralPath $fontRoot -File | Sort-Object Name | ForEach-Object { "$fontFolder\$($_.Name)" })
    foreach ($sidecar in @('tiger-v2.tcd.sentence.tcd', 'tiger-v2.tcd.supplement.tcd')) {
        if (Test-Path -LiteralPath (Join-Path $source $sidecar)) { $artifacts += $sidecar }
    }
    $hashes = [ordered]@{}
    foreach ($file in $artifacts) {
        if (!(Test-Path "$source\$file")) { throw "Missing build artifact: $file" }
        $hashes[$file] = (Get-FileHash -LiteralPath "$source\$file" -Algorithm SHA256).Hash
    }
    $packageText = ($hashes.GetEnumerator() | ForEach-Object { "$($_.Key) $($_.Value)" }) -join "`n"
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $packageHash = [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($packageText))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
    $generation = $packageHash.Substring(0, 16)
    $destination = "$env:ProgramFiles\Tigirl\versions\$generation"
    $dll = "$destination\Tigirl.dll"

    foreach ($executable in $executables) {
        $bytes = [IO.File]::ReadAllBytes((Join-Path $source $executable))
        if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) { throw "Invalid executable: $executable" }
        $pe = [BitConverter]::ToInt32($bytes, 0x3c)
        if ($pe -lt 0 -or $pe -gt $bytes.Length - 6 -or [BitConverter]::ToUInt32($bytes, $pe) -ne 0x4550) { throw "Invalid PE header: $executable" }
        $expectedMachine = if ($executable -eq 'verify_load_x64.exe') { 0x8664 } else { 0xaa64 }
        if ([BitConverter]::ToUInt16($bytes, $pe + 4) -ne $expectedMachine) { throw "Executable is not $(if ($expectedMachine -eq 0xaa64) { 'ARM64' } else { 'x64' }): $executable" }
    }
    $loadChecks = Test-NativeTigerDeployment $source $hashes -Arm64X:$Arm64X
    if ($CheckOnly) {
        @{ generation = $generation; package_hash = $packageHash; destination = $destination; artifacts = $hashes;
           architecture = $architecture; load_checks = $loadChecks;
           manager_shortcut = (Get-NativeTigerShortcutPath ([Environment]::GetFolderPath('CommonPrograms'))); status = 'validated' } | ConvertTo-Json -Depth 5
        return
    }
    # Complete package/loader preflight before asking Windows for elevation.
    if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        if ($Elevated) { throw 'Administrator privileges were not granted.' }
        $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"' + $PSCommandPath + '"'), '-Elevated')
        if ($Arm64X) { $arguments += '-Arm64X' }
        $process = Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -PassThru -Wait -ArgumentList $arguments
        exit $process.ExitCode
    }
    Start-Transcript -Path "$PSScriptRoot\install-arm64.log" -Force
    $transcribing = $true
    New-Item -ItemType Directory -Force $destination | Out-Null
    $previousKey = "Registry::HKEY_CLASSES_ROOT\CLSID\$clsid\InprocServer32"
    $previousRecord = "$PSScriptRoot\build\previous-install-$generation.json"
    if ((Test-Path $previousKey) -and !(Test-Path $previousRecord)) {
        $previousDll = (Get-Item $previousKey).GetValue('')
        if ($previousDll -ne $dll) {
            $previousHash = if (Test-Path $previousDll) { (Get-FileHash $previousDll).Hash } else { $null }
            @{ dll = $previousDll; hash = $previousHash; clsid = $clsid; profile = $profile; captured = (Get-Date).ToString('o') } |
                ConvertTo-Json | Set-Content $previousRecord -Encoding UTF8
        }
    }
    foreach ($file in $artifacts) {
        if (Test-Path "$destination\$file") {
            if ((Get-FileHash "$source\$file").Hash -ne (Get-FileHash "$destination\$file").Hash) {
                throw "Immutable deployment collision: $file"
            }
        } else {
            New-Item -ItemType Directory -Force (Split-Path "$destination\$file" -Parent) | Out-Null
            Copy-Item "$source\$file" "$destination\$file"
        }
    }
    $installedLoadChecks = Test-NativeTigerDeployment $destination $hashes -Arm64X:$Arm64X
    Set-NativeTigerAppContainerAccess (Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Tigirl')
    $registration = Start-Process "$env:windir\System32\regsvr32.exe" -ArgumentList @('/s', ('"' + $dll + '"')) -Wait -PassThru
    if ($registration.ExitCode -ne 0) { throw "regsvr32 failed: $($registration.ExitCode)" }
    $registered = (Get-Item "Registry::HKEY_CLASSES_ROOT\CLSID\$clsid\InprocServer32").GetValue('')
    if ($registered -ne $dll) { throw "Unexpected COM registration: $registered" }
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class SampleImeInstall {
    [DllImport("input.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool InstallLayoutOrTip(string profile, uint flags);
    [DllImport("ole32.dll")]
    public static extern int CoCreateInstance(ref Guid clsid, IntPtr outer, uint context, ref Guid iid, out IntPtr instance);
}
'@
    if (![SampleImeInstall]::InstallLayoutOrTip($tip, 0)) { throw 'Enabling input profile failed.' }
    $classId = [guid]$clsid
    $interfaceId = [guid]'6e4e2102-f9cd-433d-b496-303ce03a6507'
    $instance = [IntPtr]::Zero
    $result = [SampleImeInstall]::CoCreateInstance([ref]$classId, [IntPtr]::Zero, 1, [ref]$interfaceId, [ref]$instance)
    if ($result -lt 0) { [Runtime.InteropServices.Marshal]::ThrowExceptionForHR($result) }
    if ($instance -eq [IntPtr]::Zero) { throw 'COM returned a null input processor.' }
    [void][Runtime.InteropServices.Marshal]::Release($instance)
    foreach ($file in $artifacts) {
        if ((Get-FileHash -LiteralPath "$destination\$file").Hash -ne $hashes[$file]) { throw "Installed hash mismatch: $file" }
    }
    Remove-TigirlLegacyIdentity $PSScriptRoot
    $shortcut = Set-NativeTigerShortcut ([Environment]::GetFolderPath('CommonPrograms')) "$env:ProgramFiles\Tigirl" (Join-Path $destination 'Tigirl.exe')
    @{ dll = $dll; generation = $generation; dll_hash = (Get-FileHash $dll).Hash;
       dictionary_hash = (Get-FileHash "$destination\tiger-v2.tcd").Hash; package_hash = $packageHash; artifacts = $hashes; tip = $tip; manager_shortcut = $shortcut; architecture = $architecture; load_checks = $loadChecks; installed_load_checks = $installedLoadChecks } |
        ConvertTo-Json -Depth 6 | Set-Content "$PSScriptRoot\build\native-install.json" -Encoding UTF8
    Write-Output "SUCCESS: $architecture COM activation and input profile installation verified."
    Write-Output "DLL: $dll"
    Write-Output "Input profile: $tip"
    Get-WinUserLanguageList | Format-List LanguageTag, InputMethodTips
} catch {
    Write-Error $_ -ErrorAction Continue
    if ($transcribing) { Stop-Transcript }
    exit 1
}
if ($transcribing) { Stop-Transcript }
