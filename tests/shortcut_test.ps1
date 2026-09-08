$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'shortcut_arm64.ps1')
$temporary = Join-Path $root ('build\shortcut-test-' + [Guid]::NewGuid().ToString('N'))
try {
    $programs = Join-Path $temporary '模拟 开始菜单'
    $install = Join-Path $temporary '安装目录'
    $first = Join-Path $install 'versions\first\schema_manager.exe'
    $second = Join-Path $install 'versions\second\schema_manager.exe'
    foreach ($target in @($first, $second)) {
        New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
        [IO.File]::WriteAllText($target, 'fixture; never executed')
    }
    Assert-NativeTigerShortcutAvailable $programs $install
    if (Test-Path -LiteralPath $programs) { throw 'Read-only preflight created a directory' }
    $path = Set-NativeTigerShortcut $programs $install $first
    $savedHash = (Get-FileHash -LiteralPath $path).Hash
    Assert-NativeTigerShortcutAvailable $programs $install
    if ((Get-FileHash -LiteralPath $path).Hash -ne $savedHash) { throw 'Preflight changed an owned shortcut' }
    $shell = New-Object -ComObject WScript.Shell
    try {
        $link = $shell.CreateShortcut($path)
        if ($link.TargetPath -ne $first -or $link.Arguments -ne '') { throw 'Initial shortcut mismatch' }
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($link)
        Set-NativeTigerShortcut $programs $install $second | Out-Null
        $link = $shell.CreateShortcut($path)
        if ($link.TargetPath -ne $second) { throw 'Shortcut did not follow generation' }
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($link)
        $other = Join-Path (Split-Path $path -Parent) 'keep.txt'
        [IO.File]::WriteAllText($other, 'preserve')
        if (!(Remove-NativeTigerShortcut $programs $install)) { throw 'Owned shortcut not removed' }
        if (!(Test-Path -LiteralPath $other)) { throw 'Unrelated file removed' }
        if (Remove-NativeTigerShortcut $programs $install) { throw 'Missing shortcut reported removed' }
        $link = $shell.CreateShortcut($path)
        $link.TargetPath = $first
        $link.Description = 'User shortcut'
        $link.Save()
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($link)
        $before = (Get-FileHash -LiteralPath $path).Hash
        $rejected = 0
        try { Assert-NativeTigerShortcutAvailable $programs $install } catch { $rejected++ }
        try { Set-NativeTigerShortcut $programs $install $second | Out-Null } catch { $rejected++ }
        try { Remove-NativeTigerShortcut $programs $install | Out-Null } catch { $rejected++ }
        if ($rejected -ne 3 -or (Get-FileHash -LiteralPath $path).Hash -ne $before) { throw 'Unowned shortcut changed' }
    } finally { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell) }
    $blockedPrograms = Join-Path $temporary 'blocked-programs'
    New-Item -ItemType Directory -Path $blockedPrograms | Out-Null
    [IO.File]::WriteAllText((Join-Path $blockedPrograms '原生虎码'), 'preserve')
    $blocked = $false
    try { Assert-NativeTigerShortcutAvailable $blockedPrograms $install } catch { $blocked = $true }
    if (!$blocked) { throw 'Parent file conflict was accepted' }
    @{ preflight_read_only=$true; parent_conflict_rejected=$true; created=$true; retargeted=$true; owned_removed=$true; unowned_preserved=$true; unrelated_files_preserved=$true; real_start_menu_changed=$false } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\shortcut-arm64.json') -Encoding UTF8
    Write-Output 'Isolated shortcut create/update/removal checks passed.'
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
