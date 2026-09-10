param([string]$Record = "$PSScriptRoot\build\native-install.json", [switch]$CheckOnly, [switch]$Elevated)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'shortcut_arm64.ps1')
if ([Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString() -ne 'Arm64') { throw 'Use native ARM64 PowerShell.' }
$Record = [IO.Path]::GetFullPath($Record)
$installed = Get-Content -LiteralPath $Record -Raw | ConvertFrom-Json
$clsid = '{D2291A80-84D8-4641-9AB2-BDD1472C846B}'
$x86Base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::ClassesRoot,[Microsoft.Win32.RegistryView]::Registry32)
if ($x86Base.OpenSubKey("CLSID\$clsid\InprocServer32")) { throw 'Remove the x86 supplement with uninstall_x86.ps1 before uninstalling the shared profile.' }
$profile = '{83955C0E-2C09-47A5-BCF3-F2B98E11EE8B}'
$tip = "0804:$clsid$profile"
if ($installed.tip -ne $tip) { throw 'Installation record belongs to another input profile.' }
$installRoot = [IO.Path]::GetFullPath("$env:ProgramFiles\Tigirl")
$dll = [IO.Path]::GetFullPath($installed.dll)
$versions = $installRoot.TrimEnd('\') + '\versions\'
$generation = Split-Path (Split-Path $dll -Parent) -Leaf
if (!$dll.StartsWith($versions, [StringComparison]::OrdinalIgnoreCase) -or
    [IO.Path]::GetFileName($dll) -ne 'Tigirl.dll' -or $generation -notmatch '^[0-9a-f]{16}$' -or
    [IO.Path]::GetFullPath((Join-Path $versions ("$generation\"+[IO.Path]::GetFileName($dll)))) -ne $dll) {
    throw 'Installation record does not identify a native immutable generation.'
}
if (!(Test-Path -LiteralPath $dll -PathType Leaf) -or !$installed.dll_hash -or
    (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash -ne $installed.dll_hash) { throw 'Installed DLL hash mismatch.' }
$key = "Registry::HKEY_CLASSES_ROOT\CLSID\$clsid\InprocServer32"
$registered = if (Test-Path -LiteralPath $key) { (Get-Item -LiteralPath $key).GetValue('') } else { $null }
if ($registered -and [IO.Path]::GetFullPath($registered) -ne $dll) { throw 'Another generation is registered; use its installation record.' }
$profileKeys = @('HKEY_LOCAL_MACHINE', 'HKEY_CURRENT_USER') | ForEach-Object {
    "Registry::$_\SOFTWARE\Microsoft\CTF\TIP\$clsid\LanguageProfile\0x00000804\$profile"
}
$profilePresent = [bool]($profileKeys | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1)
$programs = [Environment]::GetFolderPath('CommonPrograms')
$shortcut = Get-NativeTigerShortcutPath $programs
$ownedShortcut = $false
if (Test-Path -LiteralPath $shortcut) {
    $shell = New-Object -ComObject WScript.Shell
    $link = $null
    try { $link = $shell.CreateShortcut($shortcut); $ownedShortcut = Test-NativeTigerShortcutOwner $link $installRoot }
    finally {
        if ($link) { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($link) }
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
    }
}
if ($CheckOnly) {
    @{ status='validated'; dll=$dll; registered=[bool]$registered; tip=$tip;
       remove_owned_shortcut=$ownedShortcut; preserve_user_data=$true; preserve_binary_generations=$true;
       unregister_required=([bool]$registered -or $profilePresent); profile_registered=$profilePresent } | ConvertTo-Json
    exit 0
}
$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    if ($Elevated) { throw 'Administrator privileges were not granted.' }
    $process = Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -PassThru -Wait -ArgumentList @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"' + $PSCommandPath + '"'), '-Record', ('"' + $Record + '"'), '-Elevated')
    exit $process.ExitCode
}
# Recheck after the elevation boundary before unregistering a shared CLSID.
$now = if (Test-Path -LiteralPath $key) { (Get-Item -LiteralPath $key).GetValue('') } else { $null }
if ($now -and [IO.Path]::GetFullPath($now) -ne $dll) { throw 'Registration changed during uninstall preparation.' }
if ($now -or $profilePresent) {
    $process = Start-Process "$env:windir\System32\regsvr32.exe" -ArgumentList @('/s', '/u', ('"' + $dll + '"')) -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Unregistration failed: $($process.ExitCode)" }
    if (Test-Path -LiteralPath $key) { throw 'COM registration remains after unregistration.' }
    foreach ($profileKey in $profileKeys) {
        if (Test-Path -LiteralPath $profileKey) { throw 'Input profile remains after unregistration.' }
    }
}
if ($ownedShortcut) { Remove-NativeTigerShortcut $programs $installRoot | Out-Null }
@{ status='unregistered'; dll=$dll; tip=$tip; user_data_preserved=$true; binary_generations_preserved=$true } |
    ConvertTo-Json | Set-Content -LiteralPath "$PSScriptRoot\build\native-uninstall.json" -Encoding UTF8
Write-Output 'Native input method registration removed. User data and binary generations were retained. Restart applications to unload the old DLL.'
