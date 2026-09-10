$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$probe = Join-Path $root 'build\tests\ARM64\reminder_launch_probe.exe'
$helper = Join-Path $root 'build\tests\ARM64\Tigirl.Reminder.exe'
$wrongHelper = Join-Path $root 'build\tests\ARM64\reminder_ledger_probe.exe'
$stalledHelper = Join-Path $root 'build\tests\ARM64\reminder_stall_probe.exe'
$temporary = Join-Path $root ('build\reminder launch ' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
$ledger = Join-Path $temporary 'timer record.txt'
$name = 'Local\Tigirl.ReminderLaunchTest.'+[Guid]::NewGuid().ToString('N')
$fired = New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,$name)
$child = $null
function Require($condition,$message) { if (!$condition) { throw $message } }
function Read-Accepted {
    $text = Get-Content -LiteralPath $ledger -Raw
    return (($text -split "`r?`n") | Where-Object { $_ -match '^(accepted|deadline)\s' }) -join "`n"
}
try {
    # The probe calls the exact native launch API, then exits immediately.
    $childId = & $probe $helper $ledger 8000 $name
    Require ($LASTEXITCODE -eq 0) 'Native launcher failed'
    $child = [Diagnostics.Process]::GetProcessById([int]$childId)
    Require (!$fired.WaitOne(0)) 'Fixture expired before launcher exited'
    $before = Read-Accepted
    & $probe (Join-Path $temporary 'missing.exe') $ledger 0 $name | Out-Null
    Require ($LASTEXITCODE -eq 1 -and (Read-Accepted) -eq $before) 'Missing helper cancelled timer'
    & $probe $wrongHelper $ledger 0 $name | Out-Null
    Require ($LASTEXITCODE -eq 1 -and (Read-Accepted) -eq $before) 'Failed startup cancelled timer'
    $timeoutWatch=[Diagnostics.Stopwatch]::StartNew()
    & $probe $stalledHelper $ledger 0 $name | Out-Null
    $timeoutWatch.Stop()
    Require ($LASTEXITCODE -eq 1 -and $timeoutWatch.ElapsedMilliseconds -ge 4800) 'Startup timeout was not exercised'
    Require ((Read-Accepted) -eq $before -and !$fired.WaitOne(0)) 'Startup timeout changed or consumed old timer'
    Require ($fired.WaitOne(5000)) 'Acknowledged helper did not survive launcher exit'
    Require ($child.WaitForExit(5000)) 'Reminder helper stayed resident'
    [ordered]@{
        status='passed'; native_launch_api=$true; startup_gate=$true
        launcher_exit_survived=$true; missing_helper_preserves_timer=$true
        failed_startup_preserves_timer=$true; quoted_paths=$true; tsf_integrated=$false
        startup_timeout_preserves_timer=$true; timeout_observed_ms=$timeoutWatch.ElapsedMilliseconds
        helper_sha256=(Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash
        probe_sha256=(Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\reminder-launch-arm64.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $root 'build\reminder-launch-arm64.json')
} finally {
    if ($child) { if (!$child.HasExited) { $child.Kill(); $child.WaitForExit() }; $child.Dispose() }
    $fired.Dispose()
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
