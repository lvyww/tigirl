# Shared input-method data only. Never apply these permissions to a profile,
# installation directory, external scheme source, or executable directory.
function Set-NativeTigerAppContainerAccess([string]$Root) {
    $Root = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    if ([IO.Path]::GetFileName($Root) -ne 'NativeTiger') { throw 'Expected a NativeTiger data directory.' }
    New-Item -ItemType Directory -Force -Path $Root | Out-Null
    $items = @((Get-Item -LiteralPath $Root)) + @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
    foreach ($item in $items) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing data reparse point: $($item.FullName)" }
    }
    # Modify is needed for atomic config replacement and journal locking.
    # AppContainer's low integrity token also needs a matching mandatory label.
    & icacls.exe $Root /grant '*S-1-15-2-1:(OI)(CI)(M)' '*S-1-15-2-2:(OI)(CI)(M)' /T /Q | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot grant AppContainer data access.' }
    & icacls.exe $Root /setintegritylevel '(OI)(CI)L' /T /Q | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot set input-method data integrity level.' }
}
