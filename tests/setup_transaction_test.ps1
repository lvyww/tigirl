$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\packaging\data.ps1"
. "$PSScriptRoot\..\packaging\user_transaction.ps1"
function Check($Value,$Message){if(!$Value){throw $Message}}
$fixture=Join-Path $env:TEMP ('Tigirl-transaction-'+[guid]::NewGuid().ToString('N'))
$root=Join-Path $fixture 'Tigirl';$source=Join-Path $fixture 'source';$backup=Join-Path $root 'backups\test'
New-Item $root,$source -ItemType Directory -Force|Out-Null
try{
 [IO.File]::WriteAllText("$source\table.txt",'new')
 [IO.File]::WriteAllText("$source\second.txt",'new second')
 [IO.File]::WriteAllText("$root\table.txt",'old')
 [IO.File]::WriteAllText("$root\config.txt",'old setting')
 $snapshot=@([pscustomobject]@{Path="$root\config.txt";Bytes=[IO.File]::ReadAllBytes("$root\config.txt")})
 Save-UserJournal @{id='test';backup=$backup;snapshots=$snapshot} "$root\.setup-user-transaction.json"
 $plan=@(Get-DataMerge $source $root);foreach($item in $plan){$item.Choice='Copy'}
 Invoke-DataMerge $plan $backup|Out-Null
 [IO.File]::WriteAllText("$root\config.txt",'new setting')
 New-Item "$root\schemas\new" -ItemType Directory -Force|Out-Null
 [IO.File]::WriteAllText("$root\schemas\new\current.txt",'newgeneration')
 # Reload durable journal, as a fresh process would after interruption.
 Restore-UserJournal $root
 Check ([IO.File]::ReadAllText("$root\table.txt") -eq 'old') 'Interrupted overwrite was not recovered'
 Check ([IO.File]::ReadAllText("$root\config.txt") -eq 'old setting') 'Configuration snapshot was not recovered'
 Check (!(Test-Path "$root\schemas\new\current.txt")) 'New descriptor survived rollback'
 Check (!(Test-Path "$root\second.txt")) 'Second file was not rolled back'
 Check (!(Test-Path "$root\.setup-user-transaction.json")) 'Journal survived recovery'
 Restore-UserJournal $root
 # A forged recovery target must not escape the user's root.
 Save-UserJournal @{id='bad';backup=$backup;snapshots=@(@{Path="$fixture\outside.txt";Bytes=@(65)})} "$root\.setup-user-transaction.json"
 $blocked=$false;try{Restore-UserJournal $root}catch{$blocked=$true}
 Check $blocked 'Recovery accepted an external target'
 Check (!(Test-Path "$fixture\outside.txt")) 'Recovery wrote outside user root'
 'PASS: persistent overwrite/config/descriptor recovery, idempotence, recovery path containment.'
}finally{Remove-Item $fixture -Recurse -Force}
