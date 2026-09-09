$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'reminder_window.ps1')
$root = Split-Path $PSScriptRoot -Parent
$hostExe = Join-Path $root 'build\tests\ARM64\tsf_host.exe'
$dll = Join-Path $root 'build\ARM64\Release\Tigirl.dll'
$helper = Join-Path $root 'build\ARM64\Release\Tigirl.Reminder.exe'
$manifest = Join-Path (Split-Path $dll -Parent) 'NativeTiger.Test.manifest'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'NativeTiger.Test.manifest') -Destination $manifest -Force
$registration = 'Registry::HKEY_CLASSES_ROOT\CLSID\{D2291A80-84D8-4641-9AB2-BDD1472C846B}\InprocServer32'
$before = (Get-Item $registration).GetValue('')
$temporary = Join-Path $root ('build\timer-host-exit-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
New-Item -ItemType File -Path (Join-Path $temporary '.tsf-timer-test') | Out-Null
# Unicode escapes keep this fixture independent of PowerShell source encoding.
([string][char]0x6700+[char]0x5927+[char]0x7801+[char]0x957f+' 16') | Set-Content -LiteralPath (Join-Path $temporary 'config.txt') -Encoding UTF8
$owned = New-Object 'Collections.Generic.List[Diagnostics.Process]'
function Require($condition,$message) {if (!$condition) {throw $message}}
function Run-TimerHost {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName=$hostExe
    $info.Arguments=(@($dll,'-',$manifest,'--timer-detach',$temporary) | ForEach-Object { '"'+$_+'"' }) -join ' '
    $info.UseShellExecute=$false; $info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
    $process = New-Object Diagnostics.Process; $process.StartInfo=$info
    Require ($process.Start()) 'Cannot start TSF timer host'; $owned.Add($process)
    Require ($process.WaitForExit(20000)) 'TSF timer host failed to exit'
    $output=$process.StandardOutput.ReadToEnd(); $errorText=$process.StandardError.ReadToEnd()
    Require ($process.ExitCode -eq 0) ('TSF timer host failed: '+$errorText)
    $result=$output | ConvertFrom-Json
    Require ($result.tsf_deactivated -and $result.host_pid -eq $process.Id) 'Host did not verify TSF deactivation'
    $child=[Diagnostics.Process]::GetProcessById($result.helper_pid); $owned.Add($child)
    Require (!$child.HasExited -and $child.MainModule.FileName -ieq $helper) 'Wrong or terminated reminder helper'
    return @{ result=$result; child=$child }
}
try {
    $first=Run-TimerHost
    $second=Run-TimerHost
    Require ($first.result.host_pid -ne $second.result.host_pid) 'Replacement did not use distinct TSF host processes'
    Require ($first.child.WaitForExit(5000)) 'Second TSF process did not replace first reminder'
    Require ([ReminderWindow]::Find($first.result.helper_pid) -eq [IntPtr]::Zero) 'Replaced reminder left a popup'
    $window=[IntPtr]::Zero; $deadline=[uint64]$second.result.started_ms+70000
    while ($window -eq [IntPtr]::Zero -and [ReminderClock]::GetTickCount64() -lt $deadline) {
        Require (!$second.child.HasExited) 'Reminder exited before expiry after its TSF host ended'
        $window=[ReminderWindow]::Find($second.result.helper_pid)
        if ($window -eq [IntPtr]::Zero) {Start-Sleep -Milliseconds 10}
    }
    $elapsed=[ReminderClock]::GetTickCount64()-[uint64]$second.result.started_ms
    Require ($window -ne [IntPtr]::Zero -and $elapsed -ge 59000) 'Reminder missing or early after actual TSF host exit'
    Require ([ReminderWindow]::HasExpectedText($window)) 'Detached reminder text differs'
    $button=[ReminderWindow]::Button($window)
    Require ($button -ne [IntPtr]::Zero -and [ReminderWindow]::PostMessage($button,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)) 'Cannot dismiss own detached reminder'
    Require ($second.child.WaitForExit(5000)) 'Dismissed detached reminder stayed resident'
    Require ((Get-Item $registration).GetValue('') -eq $before) 'Timer test changed installed registration'
    [ordered]@{
        status='passed'; tsf_hosts=@($first.result,$second.result)
        actual_tsf_deactivation=$true; actual_tsf_host_exit=$true; cross_process_tsf_replacement=$true
        popup_text_and_dismissal=$true; expiry_ms=$elapsed; registration_unchanged=$true
        physical_keyboard_or_application_matrix=$false
        dll_sha256=(Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
        host_sha256=(Get-FileHash -LiteralPath $hostExe -Algorithm SHA256).Hash
        helper_sha256=(Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $root 'build\timer-host-exit-arm64.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $root 'build\timer-host-exit-arm64.json')
} finally {
    foreach ($process in $owned) {
        if (!$process.HasExited) {$process.Kill();$process.WaitForExit()}
        $process.Dispose()
    }
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
