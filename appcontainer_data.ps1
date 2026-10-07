# Shared input-method data only. Never apply these permissions to a profile,
# installation directory, external scheme source, or executable directory.
function Set-NativeTigerAppContainerAccess([string]$Root) {
    $Root = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    if ([IO.Path]::GetFileName($Root) -ne 'Tigirl') { throw 'Expected a Tigirl data directory.' }
    New-Item -ItemType Directory -Force -Path $Root | Out-Null
    $items = @((Get-Item -LiteralPath $Root)) + @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
    foreach ($item in $items) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing data reparse point: $($item.FullName)" }
    }
    # Modify is needed for atomic config replacement and journal locking.
    # AppContainer's low integrity token also needs a matching mandatory label.
    # Launch the executable explicitly; PowerShell's command dispatch can treat
    # .exe files as documents when PATHEXT is missing from the caller's environment.
    foreach ($operation in @(
        '/grant "*S-1-15-2-1:(OI)(CI)(M)" "*S-1-15-2-2:(OI)(CI)(M)" /T /Q',
        '/setintegritylevel "(OI)(CI)L" /T /Q'
    )) {
        $process = [Diagnostics.Process]::new()
        try {
            $process.StartInfo.FileName = Join-Path ([Environment]::SystemDirectory) 'icacls.exe'
            $process.StartInfo.Arguments = '"' + $Root + '" ' + $operation
            $process.StartInfo.UseShellExecute = $false
            $process.StartInfo.CreateNoWindow = $true
            $process.StartInfo.RedirectStandardOutput = $true
            if (!$process.Start()) { throw 'Cannot start input-method data permission update.' }
            $output = $process.StandardOutput.ReadToEnd()
            $process.WaitForExit()
            if ($process.ExitCode -ne 0) { throw "Cannot update input-method data permissions ($($process.ExitCode)): $output" }
        } finally { $process.Dispose() }
    }
}
