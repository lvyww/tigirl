param([Parameter(Mandatory)][string]$Package)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
. "$repo\packaging\user_identity.ps1"
function Check($ok,$message){if(!$ok){throw $message}}
# Actual Windows account lookup plus elevated/same-account and other-account policy.
$sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$sessionSid=Get-TigirlSessionUserSid
Check ($sid -eq $sessionSid) 'Actual interactive session identity mismatch'
Check (Test-TigirlInitializationIdentity $false $sid '') 'Standard user rejected'
Check (Test-TigirlInitializationIdentity $true $sid $sessionSid) 'Elevated interactive user rejected'
Check (!(Test-TigirlInitializationIdentity $true 'S-1-5-21-1-2-3-500' $sessionSid)) 'Different administrator admitted'
Check (!(Test-TigirlInitializationIdentity $true $sid '')) 'Unknown elevated identity admitted'
$fixture=Join-Path $env:TEMP ('Tigirl-first-install-'+[guid]::NewGuid().ToString('N'))
$saved=$env:NATIVE_TIGER_USER_ROOT
$env:NATIVE_TIGER_USER_ROOT=Join-Path $fixture 'Tigirl'
$user=$env:NATIVE_TIGER_USER_ROOT
New-Item $user -ItemType Directory -Force|Out-Null
$manifest=Get-Content "$Package\manifest.json" -Raw|ConvertFrom-Json
$data=@($manifest.files|Where-Object {$_.path -like 'DefaultData\*'})
function Run-Repair {
    $p=Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$repo+'\packaging\repair-user-data.ps1"'),'-Package',('"'+$Package+'"')) -NoNewWindow -RedirectStandardOutput "$fixture\repair.out" -RedirectStandardError "$fixture\repair.err" -Wait -PassThru
    Check ($p.ExitCode -eq 0) ("Repair failed: "+(Get-Content "$fixture\repair.out" -Raw)+(Get-Content "$fixture\repair.err" -Raw))
}
try {
    # Completely absent user directory (no settings, data or completion marker).
    $env:NATIVE_TIGER_USER_ROOT=Join-Path $fixture 'blank\Tigirl'
    Run-Repair
    foreach($entry in $data){Check (Test-Path -LiteralPath (Join-Path $env:NATIVE_TIGER_USER_ROOT $entry.path.Substring('DefaultData\'.Length))) 'Blank-user resource missing'}
    Check (Test-Path "$env:NATIVE_TIGER_USER_ROOT\config.txt") 'Blank-user config missing'
    $env:NATIVE_TIGER_USER_ROOT=$user
    # A clean user with a stale success marker is the fallback-only failure state.
    $manifest.version|Set-Content "$user\installed-data-version.txt"
    $config="# keep my settings`r`n当前码表`t虎码字词`r`n最近码表对`t虎码字词`r`n字体大小`t16.94999999999999`r`n自定义键`t保留`r`n"
    [IO.File]::WriteAllText("$user\config.txt",$config,[Text.UTF8Encoding]::new($true))
    $configHash=(Get-FileHash "$user\config.txt").Hash
    Run-Repair
    foreach($entry in $data){
        $target=Join-Path $user $entry.path.Substring('DefaultData\'.Length)
        Check ((Get-FileHash -LiteralPath $target).Hash -eq $entry.sha256) "Missing/different bundled file: $($entry.path)"
    }
    # Selecting the current schema moves its two bookkeeping keys to the end.
    # All user values, precision, comments and unknown keys must remain identical.
    Check (@(Compare-Object ($config -split "`r`n") ([IO.File]::ReadAllText("$user\config.txt") -split "`r`n")).Count -eq 0) 'Existing settings changed'
    $configHash=(Get-FileHash "$user\config.txt").Hash
    Check (!(Test-Path "$user\.setup-user-transaction.json")) 'Repair left pending transaction'
    $skin=Get-ChildItem "$user\皮肤" -Filter '*.ssf'|Select-Object -First 1
    Remove-Item -LiteralPath $skin.FullName
    $table=Join-Path $user '码表\虎整句\虎整句.txt'
    Add-Content -LiteralPath $table -Value "zzzz 用户保留"
    $tableHash=(Get-FileHash $table).Hash
    Run-Repair
    Check (Test-Path -LiteralPath $skin.FullName) 'Missing skin not repaired with same-version marker'
    Check ((Get-FileHash $table).Hash -eq $tableHash) 'User dictionary edit overwritten'
    Check ((Get-FileHash "$user\config.txt").Hash -eq $configHash) 'Config changed on second repair'
    # The new initializer must reject missing shipped sources even when its real
    # compiler could return success using only its embedded fallback dictionary.
    $broken=Join-Path $fixture 'broken-package';New-Item $broken -ItemType Directory|Out-Null
    foreach($name in @('data.ps1','user_transaction.ps1','initialize.ps1','user_identity.ps1')){Copy-Item "$repo\packaging\$name" $broken}
    Copy-Item "$repo\appcontainer_data.ps1" $broken
    Copy-Item "$Package\manifest.json" $broken
    $env:NATIVE_TIGER_USER_ROOT=Join-Path $fixture 'broken-user\Tigirl'
    $p=Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$broken+'\initialize.ps1"'),'-Quiet','-NoEnable','-NoDialogs','-RequireStandardUser') -NoNewWindow -RedirectStandardOutput "$fixture\broken.out" -RedirectStandardError "$fixture\broken.err" -Wait -PassThru
    Check ($p.ExitCode -ne 0) 'Missing sources reported success'
    Check (!(Test-Path "$env:NATIVE_TIGER_USER_ROOT\installed-data-version.txt")) 'Missing sources published success marker'
    Check (Test-Path "$env:NATIVE_TIGER_USER_ROOT\setup-initialize.log") 'Missing sources left no diagnostic log'
    [ordered]@{status='passed';resource_files=$data.Count;skins=@(Get-ChildItem "$user\皮肤" -Filter '*.ssf').Count;schemas=@(Get-ChildItem "$user\schemas" -Directory).Count;actual_session_identity=$true;same_account_elevation_allowed=$true;different_admin_blocked=$true;blank_user_initialized=$true;stale_marker_repaired=$true;user_changes_preserved=$true;missing_payload_rejected=$true}|ConvertTo-Json
}finally{$env:NATIVE_TIGER_USER_ROOT=$saved;Remove-Item -LiteralPath $fixture -Recurse -Force}
