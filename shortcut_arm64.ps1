# Shared by install/rollback; tests pass an isolated Programs directory.
function Get-NativeTigerShortcutPath([string]$ProgramsDirectory) {
    return Join-Path $ProgramsDirectory '原生虎码\方案管理.lnk'
}
function Test-NativeTigerShortcutOwner($Link, [string]$InstallRoot) {
    if ($Link.Description -ne '原生虎码 · 方案管理') { return $false }
    $root = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\') + '\versions\'
    $target = [IO.Path]::GetFullPath($Link.TargetPath)
    return $target.StartsWith($root, [StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($target) -eq 'schema_manager.exe'
}
function Assert-NativeTigerShortcutAvailable([string]$ProgramsDirectory, [string]$InstallRoot) {
    $path = Get-NativeTigerShortcutPath $ProgramsDirectory
    $directory = Split-Path $path -Parent
    if ((Test-Path -LiteralPath $directory) -and !(Test-Path -LiteralPath $directory -PathType Container)) {
        throw 'The manager Start Menu directory is occupied by a file.'
    }
    if (!(Test-Path -LiteralPath $path)) { return }
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw 'The manager shortcut path is not a file.' }
    $shell = New-Object -ComObject WScript.Shell
    $link = $null
    try {
        $link = $shell.CreateShortcut($path)
        if (!(Test-NativeTigerShortcutOwner $link $InstallRoot)) {
            throw 'The manager shortcut path is occupied by an unowned shortcut.'
        }
    } finally {
        if ($link) { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($link) }
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
    }
}
function Set-NativeTigerShortcut([string]$ProgramsDirectory, [string]$InstallRoot, [string]$Manager) {
    $path = Get-NativeTigerShortcutPath $ProgramsDirectory
    $Manager = [IO.Path]::GetFullPath($Manager)
    if (!(Test-Path -LiteralPath $Manager -PathType Leaf)) { throw 'Manager executable is missing.' }
    $shell = New-Object -ComObject WScript.Shell
    $link = $null
    try {
        $link = $shell.CreateShortcut($path)
        if ((Test-Path -LiteralPath $path) -and !(Test-NativeTigerShortcutOwner $link $InstallRoot)) {
            throw 'The manager shortcut path is occupied by an unowned shortcut.'
        }
        # Check the proposed target with the same ownership rule before saving.
        $link.TargetPath = $Manager
        $link.Description = '原生虎码 · 方案管理'
        if (!(Test-NativeTigerShortcutOwner $link $InstallRoot)) { throw 'Manager target is outside the installed generations.' }
        $link.Arguments = ''
        $link.WorkingDirectory = Split-Path $Manager -Parent
        $link.IconLocation = $Manager + ',0'
        New-Item -ItemType Directory -Path (Split-Path $path -Parent) -Force | Out-Null
        $link.Save()
        return $path
    } finally {
        if ($link) { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($link) }
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
    }
}
function Remove-NativeTigerShortcut([string]$ProgramsDirectory, [string]$InstallRoot) {
    $path = Get-NativeTigerShortcutPath $ProgramsDirectory
    if (!(Test-Path -LiteralPath $path)) { return $false }
    $shell = New-Object -ComObject WScript.Shell
    $link = $null
    try {
        $link = $shell.CreateShortcut($path)
        if (!(Test-NativeTigerShortcutOwner $link $InstallRoot)) { throw 'Refusing to remove an unowned shortcut.' }
    } finally {
        if ($link) { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($link) }
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
    }
    Remove-Item -LiteralPath $path
    $directory = Split-Path $path -Parent
    if (!(Get-ChildItem -LiteralPath $directory -Force | Select-Object -First 1)) { Remove-Item -LiteralPath $directory }
    return $true
}
