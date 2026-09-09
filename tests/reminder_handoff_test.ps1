$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'reminder_window.ps1')
$root = Split-Path $PSScriptRoot -Parent
$helper = Join-Path $root 'build\tests\ARM64\Tigirl.Reminder.exe'
$ledgerProbe = Join-Path $root 'build\tests\ARM64\reminder_ledger_probe.exe'
$temporary = Join-Path $root ('build\reminder-handoff-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
$owned = New-Object 'Collections.Generic.List[Diagnostics.Process]'
$events = New-Object 'Collections.Generic.List[Threading.EventWaitHandle]'
function Require($condition,$message) {if (!$condition) {throw $message}}
try {
    $parentScript = Join-Path $temporary 'parent.ps1'
    @'
param($Helper,$LedgerProbe,$Ledger,$Epoch,$Sequence,$Prefix,$Fired,$Proceed,$PidFile,$Mode,$Deadline)
$ErrorActionPreference='Stop'
$ready=New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,"$Prefix.ready")
$go=New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,"$Prefix.go")
$cancel=New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,"$Prefix.cancel")
$proceedEvent=[Threading.EventWaitHandle]::OpenExisting($Proceed)
try {
    $creation=[Diagnostics.Process]::GetCurrentProcess().StartTime.ToFileTimeUtc()
    $arguments=@('--armed-test',$Ledger,$Epoch,$Sequence,"$PID","$creation","$Prefix.ready","$Prefix.go","$Prefix.cancel",$Fired) | ForEach-Object {'"'+$_+'"'}
    $child=Start-Process -FilePath $Helper -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $child.Id | Set-Content -LiteralPath $PidFile
    if (!$ready.WaitOne(5000)) {throw 'Child not ready'}
    if (!$proceedEvent.WaitOne(15000)) {throw 'Controller did not release parent'}
    if ($Mode -eq 'accepted') {
        $result=& $LedgerProbe $Ledger accept $Epoch $Sequence $Deadline
        if ($LASTEXITCODE -ne 0 -or "$result".Trim() -ne '1') {throw 'Cannot publish request'}
    }
    # Intentionally exit without ever signalling GO or CANCEL.
} finally {
    $ready.Dispose();$go.Dispose();$cancel.Dispose();$proceedEvent.Dispose()
}
'@ | Set-Content -LiteralPath $parentScript -Encoding UTF8
    foreach ($mode in @('unpublished','accepted')) {
        $ledger=Join-Path $temporary "$mode.txt"
        $ticket=& $ledgerProbe $ledger reserve 1
        Require ($LASTEXITCODE -eq 0) 'Cannot reserve handoff ticket'
        $parts=$ticket.Trim().Split(' ')
        $prefix='Local\NativeTiger.HandoffTest.'+[Guid]::NewGuid().ToString('N')
        $fired=New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,"$prefix.fired")
        $proceed=New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,"$prefix.proceed")
        $events.Add($fired);$events.Add($proceed)
        $pidFile=Join-Path $temporary "$mode.pid"
        $deadline=[ReminderClock]::GetTickCount64()+3000
        $info=New-Object Diagnostics.ProcessStartInfo
        $info.FileName=Join-Path $PSHOME 'powershell.exe'
        $info.Arguments=(@('-NoProfile','-ExecutionPolicy','Bypass','-File',$parentScript,$helper,$ledgerProbe,$ledger,$parts[0],$parts[1],$prefix,"$prefix.fired","$prefix.proceed",$pidFile,$mode,"$deadline") | ForEach-Object {'"'+$_+'"'}) -join ' '
        $info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardError=$true
        $parent=New-Object Diagnostics.Process;$parent.StartInfo=$info
        Require ($parent.Start()) 'Cannot start handoff parent';$owned.Add($parent)
        [int]$childId=0;$limit=[ReminderClock]::GetTickCount64()+5000
        while (!$childId -and [ReminderClock]::GetTickCount64() -lt $limit) {
            if (Test-Path -LiteralPath $pidFile) {
                $value=Get-Content -LiteralPath $pidFile -Raw -ErrorAction SilentlyContinue
                [void][int]::TryParse($value,[ref]$childId)
            }
            if (!$childId) {Start-Sleep -Milliseconds 10}
        }
        Require ($childId -gt 0) 'Parent did not identify child'
        $child=[Diagnostics.Process]::GetProcessById($childId);[void]$child.Handle;$owned.Add($child)
        Require (!$child.HasExited -and $child.MainModule.FileName -ieq $helper) 'Unexpected handoff child'
        [void]$proceed.Set()
        Require ($parent.WaitForExit(5000)) 'Handoff parent did not exit'
        Require ($parent.ExitCode -eq 0) ('Handoff parent failed: '+$parent.StandardError.ReadToEnd())
        if ($mode -eq 'accepted') {
            Require (!$fired.WaitOne(0)) 'Accepted fixture expired before parent exit'
            Require ($fired.WaitOne(5000)) 'Published request was lost when parent exited before GO'
        }
        Require ($child.WaitForExit(5000) -and $child.ExitCode -eq 0) 'Handoff helper did not exit cleanly'
        if ($mode -eq 'unpublished') {Require (!$fired.WaitOne(0)) 'Unpublished request fired after parent exit'}
    }
    [ordered]@{status='passed'; parent_exit_before_publication_cancels=$true
        parent_exit_after_publication_before_go_survives=$true; helper_sha256=(Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\reminder-handoff-arm64.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $root 'build\reminder-handoff-arm64.json')
} finally {
    foreach ($process in $owned) {if (!$process.HasExited) {$process.Kill();$process.WaitForExit()};$process.Dispose()}
    foreach ($testEvent in $events) {$testEvent.Dispose()}
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
