$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root 'build\tests\ARM64\timer_reminder.exe'
$ledgerProbe = Join-Path $root 'build\tests\ARM64\reminder_ledger_probe.exe'
. (Join-Path $PSScriptRoot 'reminder_window.ps1')
$owned = New-Object 'Collections.Generic.List[Diagnostics.Process]'
$events = New-Object 'Collections.Generic.List[Threading.EventWaitHandle]'
$temporary = Join-Path $root ('build\reminder-test-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
function Require($condition, $message) { if (!$condition) { throw $message } }
function New-EventPair {
    $name = 'Local\NativeTiger.ReminderTest.' + [Guid]::NewGuid().ToString('N')
    $ready = New-Object Threading.EventWaitHandle($false, [Threading.EventResetMode]::ManualReset, "$name.ready")
    $fired = New-Object Threading.EventWaitHandle($false, [Threading.EventResetMode]::ManualReset, "$name.fired")
    $events.Add($ready); $events.Add($fired)
    return @{ ready=$ready; fired=$fired; readyName="$name.ready"; firedName="$name.fired" }
}
function Start-Owned($application, $arguments) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $application
    $info.Arguments = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    Require ($process.Start()) 'Cannot start owned reminder process'
    $owned.Add($process)
    return $process
}
function Start-Reminder($scope, $stamp, $deadline, $pair) {
    $process = Start-Owned $exe @('--test', $scope, "$stamp", "$deadline", $pair.readyName, $pair.firedName)
    Require ($pair.ready.WaitOne(5000)) 'Reminder did not acknowledge startup'
    return $process
}
function Reserve-Reminder($path) {
    $line = & $ledgerProbe $path reserve 1
    Require ($LASTEXITCODE -eq 0) 'Cannot reserve reminder ticket'
    $parts = $line.Trim().Split(' ')
    return @{ epoch=$parts[0]; sequence=$parts[1] }
}
function Start-LedgerReminder($path, $ticket, $deadline, $pair) {
    $process = Start-Owned $exe @('--ledger-test',$path,$ticket.epoch,$ticket.sequence,"$deadline",$pair.readyName,$pair.firedName)
    Require ($pair.ready.WaitOne(5000)) 'Ledger reminder did not acknowledge startup'
    return $process
}
try {
    $scope = [Guid]::NewGuid().ToString('B')
    $first = New-EventPair; $second = New-EventPair; $stale = New-EventPair
    $old = Start-Reminder $scope 10 ([ReminderClock]::GetTickCount64()+5000) $first
    $deadline = [ReminderClock]::GetTickCount64()+1000
    $latest = Start-Reminder $scope 20 $deadline $second
    $invalidReadyName = 'Local\NativeTiger.ReminderTest.missing.'+[Guid]::NewGuid().ToString('N')
    $failed = Start-Owned $exe @('--timer',$scope,'40','0',$invalidReadyName)
    Require ($failed.WaitForExit(5000) -and $failed.ExitCode -eq 2) 'Invalid startup acknowledgement was accepted'
    $outdated = Start-Reminder $scope 15 0 $stale
    Require ($outdated.WaitForExit(5000) -and $outdated.ExitCode -eq 0) 'Stale request did not exit'
    Require (!$stale.fired.WaitOne(0)) 'Stale request fired'
    Require ($old.WaitForExit(5000) -and $old.ExitCode -eq 0) 'Replaced reminder did not exit'
    Require (!$first.fired.WaitOne(0)) 'Replaced reminder fired'
    Require ($second.fired.WaitOne(5000)) 'Latest reminder did not fire'
    Require ([ReminderClock]::GetTickCount64() -ge $deadline) 'Reminder fired early'
    Require ($latest.WaitForExit(5000) -and $latest.ExitCode -eq 0) 'Completed reminder stayed resident'

    $ledgerPath = Join-Path $temporary 'timer.txt'
    $ticketA = Reserve-Reminder $ledgerPath
    $ledgerFirst = New-EventPair; $ledgerSecond = New-EventPair
    $ledgerOld = Start-LedgerReminder $ledgerPath $ticketA ([ReminderClock]::GetTickCount64()+5000) $ledgerFirst
    $ticketB = Reserve-Reminder $ledgerPath
    $ledgerFailed = Start-Owned $exe @('--ledger',$ledgerPath,$ticketB.epoch,$ticketB.sequence,'0',$invalidReadyName)
    Require ($ledgerFailed.WaitForExit(5000) -and $ledgerFailed.ExitCode -eq 2) 'Invalid ledger startup acknowledgement was accepted'
    Start-Sleep -Milliseconds 200
    Require (!$ledgerOld.HasExited) 'Reserving a ticket cancelled the running reminder'
    $ledgerLatest = Start-LedgerReminder $ledgerPath $ticketB ([ReminderClock]::GetTickCount64()+500) $ledgerSecond
    Require ($ledgerOld.WaitForExit(5000) -and $ledgerOld.ExitCode -eq 0) 'Ledger replacement did not stop old helper'
    Require (!$ledgerFirst.fired.WaitOne(0)) 'Replaced ledger reminder fired'
    Require ($ledgerSecond.fired.WaitOne(5000)) 'Latest ledger reminder did not fire'
    Require ($ledgerLatest.WaitForExit(5000) -and $ledgerLatest.ExitCode -eq 0) 'Completed ledger helper stayed resident'
    foreach ($ticket in @($ticketA,$ticketB)) {
        $lateResult = New-EventPair
        $late = Start-LedgerReminder $ledgerPath $ticket 0 $lateResult
        Require ($late.WaitForExit(5000) -and $late.ExitCode -eq 0) 'Late ledger helper did not exit'
        Require (!$lateResult.fired.WaitOne(0)) 'Old or duplicate request revived after all helpers exited'
    }
    Remove-Item -LiteralPath $ledgerPath
    $newEpochTicket = Reserve-Reminder $ledgerPath
    Require ($newEpochTicket.epoch -ne $ticketB.epoch) 'Recreated ledger retained old epoch'
    $wrongEpochResult = New-EventPair
    $wrongEpoch = Start-LedgerReminder $ledgerPath $ticketB 0 $wrongEpochResult
    Require ($wrongEpoch.WaitForExit(5000) -and !$wrongEpochResult.fired.WaitOne(0)) 'Old epoch replaced recreated ledger'

    $survivor = New-EventPair
    $survivorLedger = Join-Path $temporary 'survivor.txt'
    $survivorTicket = Reserve-Reminder $survivorLedger
    $launcherScript = Join-Path $temporary 'launch.ps1'
    $pidFile = Join-Path $temporary 'child.txt'
    @'
param($Exe,$Ledger,$Epoch,$Sequence,$Deadline,$Ready,$Fired,$PidFile)
$arguments = @('--ledger-test',$Ledger,$Epoch,$Sequence,$Deadline,$Ready,$Fired) | ForEach-Object { '"'+$_+'"' }
$child = Start-Process -FilePath $Exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
$child.Id | Set-Content -LiteralPath $PidFile
'@ | Set-Content -LiteralPath $launcherScript -Encoding UTF8
    $launcher = Start-Owned (Join-Path $PSHOME 'powershell.exe') @('-NoProfile','-ExecutionPolicy','Bypass','-File', $launcherScript,
        $exe,$survivorLedger,$survivorTicket.epoch,$survivorTicket.sequence,([ReminderClock]::GetTickCount64()+4000),$survivor.readyName,$survivor.firedName,$pidFile)
    Require ($launcher.WaitForExit(5000) -and $launcher.ExitCode -eq 0) 'Launcher failed to exit'
    $childId = [int](Get-Content -LiteralPath $pidFile)
    $survivingProcess = [Diagnostics.Process]::GetProcessById($childId); $owned.Add($survivingProcess)
    Require ($survivor.ready.WaitOne(5000)) 'Surviving reminder did not start'
    Require (!$survivor.fired.WaitOne(0)) 'Survival fixture expired before parent exit'
    Require ($survivor.fired.WaitOne(5000)) 'Reminder was lost after parent exit'
    Require ($survivingProcess.WaitForExit(5000)) 'Surviving reminder did not exit after completion'
    $popupDeadline = [ReminderClock]::GetTickCount64()+400
    $popupReady = New-EventPair
    $popupTicket = Reserve-Reminder $ledgerPath
    $popup = Start-Owned $exe @('--ledger',$ledgerPath,$popupTicket.epoch,$popupTicket.sequence,"$popupDeadline",$popupReady.readyName)
    Require ($popupReady.ready.WaitOne(5000)) 'Actual reminder did not acknowledge startup'
    $popupWindow = [IntPtr]::Zero
    $popupTimeout = [ReminderClock]::GetTickCount64()+5000
    while ($popupWindow -eq [IntPtr]::Zero -and [ReminderClock]::GetTickCount64() -lt $popupTimeout) {
        $popupWindow = [ReminderWindow]::Find($popup.Id)
        if ($popupWindow -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 10 }
    }
    Require ($popupWindow -ne [IntPtr]::Zero) 'Actual reminder popup did not appear'
    Require ([ReminderWindow]::HasExpectedText($popupWindow)) 'Actual reminder text differs'
    $popupButton = [ReminderWindow]::Button($popupWindow)
    Require ($popupButton -ne [IntPtr]::Zero) 'Reminder confirmation button missing'
    Require ([ReminderWindow]::PostMessage($popupButton,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)) 'Cannot click own reminder button'
    Require ($popup.WaitForExit(5000) -and $popup.ExitCode -eq 0) 'Actual reminder did not exit after dismissal'
    Require ([ReminderWindow]::Find($popup.Id) -eq [IntPtr]::Zero) 'Reminder window survived process completion'
    [ordered]@{
        status='passed'; cross_process_replacement=$true; stale_request_rejected=$true
        completed_process_exits=$true; survives_launcher_exit=$true
        actual_popup_tested=$true; popup_text_and_button_verified=$true; tsf_integrated=$false
        startup_acknowledged=$true; invalid_acknowledgement_preserves_existing_timer=$true
        durable_ledger_integrated=$true; late_after_all_helpers_exit_rejected=$true; old_epoch_rejected=$true
        ledger_startup_failure_preserves_timer=$true
        exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\reminder-process-arm64.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $root 'build\reminder-process-arm64.json')
} finally {
    foreach ($process in $owned) {
        if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
    foreach ($testEvent in $events) { $testEvent.Dispose() }
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
