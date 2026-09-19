# Managed version retirement. Function definitions only; loaded by deploy.ps1.
function Get-RetirementPath([string]$Directory){
 Assert-VersionPath $Directory
 return Join-Path $InstallRoot ('retired\'+(Split-Path $Directory -Leaf)+'.json')
}
function Get-CleanupManifest([string]$Directory){
 $saved=Get-RetirementPath $Directory
 $path=if(Test-Path -LiteralPath $saved){$saved}else{Join-Path $Directory 'manifest.json'}
 $m=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json
 if($m.version -notmatch '^\d+\.\d+\.\d+\.\d+$'){throw 'Invalid cleanup inventory version.'}
 foreach($entry in $m.files){
  if(!$entry.path -or $entry.path -match '(^[\\/]|:|(^|[\\/])\.\.?([\\/]|$))'){throw 'Invalid cleanup inventory path.'}
 }
 return $m
}
function Get-CleanupFiles($Manifest){
 $files=@{}
 foreach($entry in $Manifest.files){$files[$entry.path]=$entry}
 foreach($entry in $Manifest.files){
  if($entry.path -like 'x64\*' -and $entry.path -ne 'x64\Tigirl.dll'){
   $alias='x86\'+$entry.path.Substring(4)
   if(!$files.ContainsKey($alias)){$files[$alias]=[pscustomobject]@{path=$alias;sha256=$entry.sha256}}
  }
 }
 return @($files.Values|Sort-Object path)
}
function Preserve-ChangedPackageFile([string]$Directory,[string]$Relative,[string]$Hash){
 # Older Inno logs may track the original. Keep an independent untracked backup.
 $file=Join-Path $Directory $Relative
 $backup=Join-Path $InstallRoot ('preserved\'+(Split-Path $Directory -Leaf)+'\'+$Relative)
 Assert-PlainTree (Split-Path $backup -Parent)
 if(Test-Path -LiteralPath $backup){
  if((Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -eq $Hash){return}
  $backup+='.'+[guid]::NewGuid().ToString('N')+'.bak'
 }
 New-Item (Split-Path $backup -Parent) -ItemType Directory -Force|Out-Null
 Copy-Item -LiteralPath $file -Destination $backup
 if((Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ne $Hash){throw "Changed file backup verification failed: $file"}
 Write-SetupLog "Preserved modified package file: $file; backup=$backup"
}
function Remove-Version([string]$Directory,[switch]$KeepControlFiles,[switch]$AtReboot){
 Assert-VersionPath $Directory
 $saved=Get-RetirementPath $Directory
 if(!(Test-Path -LiteralPath $Directory)){
  if(Test-Path -LiteralPath $saved){Remove-Item -LiteralPath $saved -Force}
  return
 }
 $m=Get-CleanupManifest $Directory
 # Persist outside the retiring version before scheduling any file deletion.
 if(!(Test-Path -LiteralPath $saved)){
  New-Item (Split-Path $saved -Parent) -ItemType Directory -Force|Out-Null
  Save-Json $m $saved
 }
 foreach($entry in Get-CleanupFiles $m){
  $relative=$entry.path
  if($KeepControlFiles -and $relative -in @('common.ps1','data.ps1','retirement.ps1','setup\deploy.ps1')){continue}
  $file=Join-Path $Directory $relative
  if(!(Test-Path -LiteralPath $file -PathType Leaf)){continue}
  if($entry.sha256 -notmatch '^[a-fA-F0-9]{64}$'){Write-SetupLog "Retaining file without a trusted hash: $file";continue}
  try{$hash=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash}catch{Write-SetupLog ("Cannot verify file; retained for retry: $file :: "+$_.Exception.Message);continue}
  if($hash -ne $entry.sha256){Preserve-ChangedPackageFile $Directory $relative $hash;continue}
  if($AtReboot){Queue-Deletion $file}else{try{Remove-Item -LiteralPath $file -Force}catch{Queue-Deletion $file}}
 }
 if($KeepControlFiles){return}
 $manifest=Join-Path $Directory 'manifest.json'
 if(Test-Path -LiteralPath $manifest){
  if($AtReboot){Queue-Deletion $manifest}else{try{Remove-Item -LiteralPath $manifest -Force}catch{Queue-Deletion $manifest}}
 }
 # Neither this function nor Windows recursively deletes unknown files.
 foreach($dir in @(Get-ChildItem -LiteralPath $Directory -Directory -Recurse|Sort-Object {$_.FullName.Length} -Descending)+@(Get-Item -LiteralPath $Directory)){
  if($AtReboot){Queue-Deletion $dir.FullName}
  elseif(!(Get-ChildItem -LiteralPath $dir.FullName -Force|Select-Object -First 1)){Remove-Item -LiteralPath $dir.FullName}
  else{Queue-Deletion $dir.FullName}
 }
 if(!(Test-Path -LiteralPath $Directory) -and (Test-Path -LiteralPath $saved)){Remove-Item -LiteralPath $saved -Force}
}
function Invoke-RetiredVersionCleanup([string]$CurrentDirectory){
 Assert-VersionPath $CurrentDirectory
 foreach($old in Get-ChildItem (Join-Path $InstallRoot 'versions') -Directory -ErrorAction SilentlyContinue){
  if($old.FullName -eq $CurrentDirectory -or $old.Name -notmatch '^(?:[a-f0-9]{16}|[a-f0-9]{32})$'){continue}
  if(!(Test-Path (Join-Path $old.FullName 'manifest.json')) -and !(Test-Path (Get-RetirementPath $old.FullName))){continue}
  try{Remove-Version $old.FullName -AtReboot}catch{Write-SetupLog ('Old version cleanup retained for retry: '+$old.FullName+' :: '+$_.Exception.Message)}
 }
 Remove-CompletedRetirementRecords
}
function Remove-CompletedRetirementRecords {
 foreach($inventory in Get-ChildItem (Join-Path $InstallRoot 'retired') -Filter '*.json' -File -ErrorAction SilentlyContinue){
  if($inventory.BaseName -match '^(?:[a-f0-9]{16}|[a-f0-9]{32})$' -and !(Test-Path (Join-Path $InstallRoot ('versions\'+$inventory.BaseName)))){Remove-Item -LiteralPath $inventory.FullName -Force}
 }
}
