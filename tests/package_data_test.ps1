$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\packaging\data.ps1"
$fixture=Join-Path $env:TEMP ('NativeTiger-package-test-'+[guid]::NewGuid().ToString('N'))
$source=Join-Path $fixture 'source';$target=Join-Path $fixture 'NativeTiger';$backup=Join-Path $fixture 'backup'
New-Item -ItemType Directory -Force $source,$target|Out-Null
function Put($Path,$Value){[IO.File]::WriteAllText($Path,$Value)}
function Check($Condition,$Message){if(!$Condition){throw $Message}}
try{
    Put "$source\same.txt" 'same';Put "$target\same.txt" 'same'
    Put "$source\conflict.txt" 'new';Put "$target\conflict.txt" 'old'
    Put "$source\added.txt" 'added';Put "$target\custom.txt" 'custom'
    $plan=@(Get-DataMerge $source $target)
    Check (@($plan|Where-Object Conflict).Count -eq 1) 'Conflict classification failed'
    $changes=@(Invoke-DataMerge $plan $backup)
    Check ([IO.File]::ReadAllText("$target\conflict.txt") -eq 'old') 'Default skip overwrote custom data'
    Check (Test-Path "$target\added.txt") 'Missing new file'
    Undo-DataMerge $changes
    Check (!(Test-Path "$target\added.txt")) 'New file rollback failed'
    $plan=@(Get-DataMerge $source $target);foreach($item in $plan){if($item.Conflict){$item.Choice='Copy'}}
    $changes=@(Invoke-DataMerge $plan $backup)
    Check ([IO.File]::ReadAllText("$target\conflict.txt") -eq 'new') 'Overwrite failed'
    Check ([IO.File]::ReadAllText("$target\custom.txt") -eq 'custom') 'Unrelated user file changed'
    Undo-DataMerge $changes
    Check ([IO.File]::ReadAllText("$target\conflict.txt") -eq 'old') 'Backup restore failed'
    $plan=@(Get-DataMerge $source $target);foreach($item in $plan){if($item.Conflict){$item.Choice='Copy'}}
    Put "$target\conflict.txt" 'concurrent edit'
    $rejected=$false;try{Invoke-DataMerge $plan $backup|Out-Null}catch{$rejected=$true}
    Check $rejected 'Concurrent edit was overwritten'
    Check ([IO.File]::ReadAllText("$target\conflict.txt") -eq 'concurrent edit') 'Concurrent data was lost'
    Check (!(Test-Path "$target\added.txt")) 'Partial merge rollback failed'
    'PASS: classification, default skip, overwrite, backup, rollback, concurrent changes.'
}finally{Remove-Item -LiteralPath $fixture -Recurse -Force}
