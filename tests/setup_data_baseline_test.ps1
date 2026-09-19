$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\packaging\data.ps1"
. "$PSScriptRoot\..\packaging\user_transaction.ps1"
function Check($Condition,$Message){if(!$Condition){throw $Message}}
function Put($Path,$Text){[IO.File]::WriteAllText($Path,$Text)}
function Hash($Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
$fixture=Join-Path $env:TEMP ('Tigirl-baseline-test-'+[guid]::NewGuid().ToString('N'))
$source=Join-Path $fixture 'source';$root=Join-Path $fixture 'user'
New-Item $source,$root -ItemType Directory -Force|Out-Null
$baseline=Join-Path $root 'installed-data-baseline.json'
$journal=Join-Path $root '.setup-user-transaction.json'
try{
 Put "$source\plain.txt" 'official-A';Put "$source\edited.txt" 'official-B'
 $plan=@(Get-DataMerge $source $root)
 Check (@($plan|Where-Object Choice -eq 'Copy').Count -eq 2) 'Fresh files not installed'
 $backup=Join-Path $root 'backups\first'
 Invoke-DataMerge $plan $backup|Out-Null;Save-DataBaseline $plan $baseline
 $initial=Read-DataBaseline $baseline
 Check ($initial['plain.txt'] -eq (Hash "$root\plain.txt")) 'First-install baseline missing'
 Put "$source\plain.txt" 'official-A2';Put "$source\edited.txt" 'official-B2'
 Put "$root\edited.txt" 'personal-edit';Put "$root\custom.txt" 'custom'
 $plan=@(Get-DataMerge $source $root)
 Check (($plan|Where-Object RelativePath -eq 'plain.txt').Choice -eq 'Copy') 'Unmodified file not updated'
 Check (($plan|Where-Object RelativePath -eq 'edited.txt').Choice -eq 'Skip') 'Modified file not preserved'
 Invoke-DataMerge $plan (Join-Path $root 'backups\second')|Out-Null
 Save-DataBaseline $plan $baseline
 Check ([IO.File]::ReadAllText("$root\plain.txt") -eq 'official-A2') 'Official update not installed'
 Check ([IO.File]::ReadAllText("$root\edited.txt") -eq 'personal-edit') 'Personal edit lost'
 Check ((Read-DataBaseline $baseline)['edited.txt'] -eq $initial['edited.txt']) 'Skipped baseline advanced'
 # Equal new bytes do not magically adopt an unknown or user-modified file.
 Put "$source\edited.txt" 'personal-edit'
 Put "$source\unknown.txt" 'same';Put "$root\unknown.txt" 'same'
 $plan=@(Get-DataMerge $source $root)
 Check (($plan|Where-Object RelativePath -eq 'plain.txt').Choice -eq 'Copy') 'Identical managed bytes should remain installable'
 Check (($plan|Where-Object RelativePath -eq 'edited.txt').Choice -eq 'Skip') 'User edit matching new package was adopted'
 Check (($plan|Where-Object RelativePath -eq 'unknown.txt').Choice -eq 'Skip') 'Unknown equal file was adopted'
 Invoke-DataMerge $plan (Join-Path $root 'backups\third')|Out-Null;Save-DataBaseline $plan $baseline
 Check (!(Read-DataBaseline $baseline).ContainsKey('unknown.txt')) 'Unknown file gained a baseline'
 # Deliberate deletions are restored under the documented missing-file policy.
 Remove-Item "$root\plain.txt"
 $plan=@(Get-DataMerge $source $root)
 Check (($plan|Where-Object RelativePath -eq 'plain.txt').Reason -eq 'Missing') 'Missing file not recognized'
 Invoke-DataMerge $plan (Join-Path $root 'backups\missing')|Out-Null;Save-DataBaseline $plan $baseline
 # A failed initialization restores both bytes and baseline from one journal.
 $oldBaseline=[IO.File]::ReadAllBytes($baseline)
 $oldText=[IO.File]::ReadAllText("$root\plain.txt")
 Put "$source\plain.txt" 'official-A3';Put "$source\new.txt" 'new'
 $plan=@(Get-DataMerge $source $root);$backup=Join-Path $root 'backups\rollback'
 Save-UserJournal @{id='fixture';state='prepared';backup=$backup;snapshots=@(@{Path=$baseline;Bytes=$oldBaseline})} $journal
 Invoke-DataMerge $plan $backup|Out-Null;Save-DataBaseline $plan $baseline
 Restore-UserJournal $root
 Check ([IO.File]::ReadAllText("$root\plain.txt") -eq $oldText) 'Rollback lost official old data'
 Check (!(Test-Path "$root\new.txt")) 'Rollback kept newly installed file'
 Check ([Convert]::ToBase64String([IO.File]::ReadAllBytes($baseline)) -eq [Convert]::ToBase64String($oldBaseline)) 'Rollback did not restore baseline'
 Check ([IO.File]::ReadAllText("$root\custom.txt") -eq 'custom') 'Unrelated custom file changed'
 # No/invalid baseline is conservative; it never blesses existing user content.
 Put $baseline '{damaged'
 $plan=@(Get-DataMerge $source $root)
 Check (@($plan|Where-Object { $_.OldHash -and $_.Choice -eq 'Copy' }).Count -eq 0) 'Damaged baseline caused overwrite'
 Put $baseline '{"schemaVersion":1,"files":[{"path":"..\\outside","sha256":"bad"}]}'
 Check ((Read-DataBaseline $baseline).Count -eq 0) 'Invalid baseline accepted'
 Remove-Item $baseline
 $plan=@(Get-DataMerge $source $root)
 Check (($plan|Where-Object RelativePath -eq 'plain.txt').Reason -eq 'Unknown') 'Legacy file was silently adopted'
 'PASS: first install, automatic official update, repeat writes, user edits, unknown files, missing files, durable baseline rollback, damaged metadata.'
}finally{if(Test-Path $fixture){Remove-Item -LiteralPath $fixture -Recurse -Force}}
