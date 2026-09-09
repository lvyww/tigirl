# Durable current-user initialization recovery. Never used with administrator supplied user paths.
function Save-UserJournal($Value,[string]$Path){
 $tmp=$Path+'.tmp';$Value|ConvertTo-Json -Depth 8|Set-Content $tmp -Encoding UTF8
 if(Test-Path $Path){[IO.File]::Replace($tmp,$Path,[NullString]::Value)}else{[IO.File]::Move($tmp,$Path)}
}
function Restore-UserJournal([string]$Root){
 Assert-PlainTree $Root
 $path=Join-Path $Root '.setup-user-transaction.json'
 if(!(Test-Path $path)){return}
 $j=Get-Content $path -Raw|ConvertFrom-Json
 $prefix=[IO.Path]::GetFullPath($Root).TrimEnd('\')+'\'
 foreach($s in $j.snapshots){if(!([IO.Path]::GetFullPath($s.Path)).StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Invalid user recovery path.'}}
 if(!([IO.Path]::GetFullPath($j.backup)).StartsWith($prefix+'backups\',[StringComparison]::OrdinalIgnoreCase)){throw 'Invalid backup path.'}
 $files=Join-Path $j.backup 'files.json'
 if(Test-Path $files){
  # Windows PowerShell 5.1 emits a JSON array as one pipeline object.
  # Assign it directly; wrapping the pipeline in @() would nest multi-file journals.
  $changes=Get-Content $files -Raw|ConvertFrom-Json
  foreach($c in $changes){if(!([IO.Path]::GetFullPath($c.Target)).StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Invalid recovery target.'};if($c.Backup -and !([IO.Path]::GetFullPath($c.Backup)).StartsWith($prefix+'backups\',[StringComparison]::OrdinalIgnoreCase)){throw 'Invalid backup file.'}}
  Undo-DataMerge $changes
 }
 $known=@{}
 foreach($s in $j.snapshots){$known[$s.Path]=$true;if($null -ne $s.Bytes){[IO.File]::WriteAllBytes($s.Path,[byte[]]$s.Bytes)}elseif(Test-Path $s.Path){Remove-Item $s.Path}}
 foreach($p in @(Get-ChildItem (Join-Path $Root 'schemas') -Filter current.txt -Recurse -ErrorAction SilentlyContinue|ForEach-Object FullName)){if(!$known.ContainsKey($p)){Remove-Item $p}}
 Remove-Item $path
}
