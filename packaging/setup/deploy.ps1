# Machine-only transaction coordinator. Installed by Inno; never accepts user data paths.
param([Parameter(Mandatory)][ValidateSet('Preflight','Begin','Apply','Commit','Recover','UninstallCheck','Uninstall')][string]$Action,
 [Parameter(Mandatory)][string]$InstallRoot,[string]$Generation,[string]$Payload)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\common.ps1"
. "$PSScriptRoot\..\data.ps1"
Assert-X64System
if(!(Test-Administrator)){throw 'Administrator privileges are required.'}
$allowed=@((Join-Path $env:ProgramFiles 'Tigirl'),(Join-Path $env:ProgramFiles 'NativeTiger'))
$InstallRoot=[IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
if($InstallRoot -notin $allowed){throw 'Invalid installation root.'}
Assert-PlainTree $InstallRoot
$NativeTigerInstallRoot=$InstallRoot;$NativeTigerRecord=Join-Path $InstallRoot 'install.json'
$journal=Join-Path $InstallRoot 'setup-transaction.json'
$log=Join-Path $InstallRoot 'setup.log'
$mutex=[Threading.Mutex]::new($false,'Global\Tigirl.Deployment')
if(!$mutex.WaitOne(0)){throw 'Another deployment is running.'}
function Save-Json($Value,[string]$Path){
 $tmp=$Path+'.tmp';$Value|ConvertTo-Json -Depth 15|Set-Content $tmp -Encoding UTF8
 if(Test-Path $Path){[IO.File]::Replace($tmp,$Path,[NullString]::Value)}else{[IO.File]::Move($tmp,$Path)}
}
function Read-Record {if(Test-Path $NativeTigerRecord){Get-Content $NativeTigerRecord -Raw|ConvertFrom-Json}}
function Assert-Owned($Record){
 foreach($pair in @(@('Registry64','x64'),@('Registry32','x86'))){
  $actual=Get-ComPath $pair[0]
  if($actual -and (!$Record -or $actual -ne (Get-PackageDll $Record.directory $pair[1]))){throw 'Input method registration belongs to another installation.'}
 }
 if($Record){Assert-VersionPath $Record.directory}
 $uri='HKLM:\SOFTWARE\Classes\nativetiger\shell\open\command'
 if(Test-Path $uri){if(!$Record -or (Get-Item $uri).GetValue('') -ne ('"'+(Get-PackageTool $Record.directory)+'" --uri "%1"')){throw 'URI protocol belongs to another installation.'}}
}
function Assert-VersionPath([string]$Directory){
 $p=[IO.Path]::GetFullPath($Directory)
 if((Split-Path $p -Parent) -ne (Join-Path $InstallRoot 'versions') -or (Split-Path $p -Leaf) -notmatch '^[a-f0-9]{16}$'){throw 'Invalid managed version directory.'}
 Assert-PlainTree $p
}
function Restore-Transaction {
 if(!(Test-Path $journal)){return}
 $j=Get-Content $journal -Raw|ConvertFrom-Json
 Assert-VersionPath $j.directory
 # Accept only the interrupted transaction's old/new COM paths, including a half registration.
 foreach($pair in @(@('Registry64','x64'),@('Registry32','x86'))){
  $v=Get-ComPath $pair[0];$valid=@((Get-PackageDll $j.directory $pair[1]))
  if($j.previous){$valid+=(Get-PackageDll $j.previous.directory $pair[1])}
  if($v -and $v -notin $valid){throw 'Registration changed after interruption; recovery stopped.'}
 }
 if($j.previous){
  Invoke-Registration $j.previous.directory
  Set-MachineEntries $j.previous.directory $j.previous.version
  Save-Json $j.previous $NativeTigerRecord
 }else{
  if((Get-ComPath 'Registry64') -or (Get-ComPath 'Registry32')){Invoke-Registration $j.directory -Remove}
  Remove-MachineEntries
  if(Test-Path $NativeTigerRecord){Remove-Item $NativeTigerRecord}
  $arp='HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\NativeTiger_is1'
  if(Test-Path $arp){Remove-Item $arp -Recurse}
  $shortcut=Join-Path ([Environment]::GetFolderPath('CommonPrograms')) '虎娘\输入设置.lnk'
  if(Test-Path $shortcut){Remove-Item $shortcut}
 }
 Move-Item $journal (Join-Path $InstallRoot ('recovered-'+$j.id+'.json')) -Force
}
function Set-GuiEntries($Record){
 Set-MachineEntries $Record.directory $Record.version
 $arp='HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\NativeTiger_is1'
 if(Test-Path $arp){Set-ItemProperty $arp UninstallString ('"'+$InstallRoot+'\Tigirl.Maintenance.exe"');Set-ItemProperty $arp QuietUninstallString ('"'+$InstallRoot+'\unins000.exe" /VERYSILENT /NORESTART');Set-ItemProperty $arp DisplayVersion $Record.version}
 $legacy='HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\NativeTiger'
 if(Test-Path $legacy){Remove-Item $legacy -Recurse}
 $shell=New-Object -ComObject WScript.Shell
 try{
  $dir=Join-Path ([Environment]::GetFolderPath('CommonPrograms')) '虎娘';New-Item $dir -ItemType Directory -Force|Out-Null
  $link=$shell.CreateShortcut((Join-Path $dir '输入设置.lnk'));$link.TargetPath=Get-PackageTool $Record.directory;$link.IconLocation=$link.TargetPath+',0';$link.Save()
 }finally{[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)}
}
function Remove-Version([string]$Directory){
 if(!('TigirlMove' -as [type])){Add-Type 'using System.Runtime.InteropServices; public static class TigirlMove {[DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool MoveFileEx(string a,string b,int f);}'}
 Assert-VersionPath $Directory
 # Delete only manifest-listed package files and explicitly created x86 hard links.
 $m=Get-Content (Join-Path $Directory 'manifest.json') -Raw|ConvertFrom-Json
 $paths=@('manifest.json')+@($m.files|ForEach-Object path)
 foreach($entry in $m.files){if($entry.path -like 'x64\*' -and $entry.path -ne 'x64\Tigirl.dll'){$paths+=('x86\'+$entry.path.Substring(4))}}
 foreach($relative in $paths|Select-Object -Unique){
  if($relative -match '(^[\\/]|:|(^|[\\/])\.\.([\\/]|$))'){throw 'Invalid manifest path.'}
  $file=Join-Path $Directory $relative
  if(Test-Path -LiteralPath $file -PathType Leaf){try{Remove-Item -LiteralPath $file -Force}catch{if(![TigirlMove]::MoveFileEx($file,$null,4)){throw};$script:restart=$true}}
 }
 foreach($dir in @(Get-ChildItem $Directory -Directory -Recurse|Sort-Object {$_.FullName.Length} -Descending)+@(Get-Item $Directory)){
  if(!(Get-ChildItem $dir.FullName -Force|Select-Object -First 1)){Remove-Item $dir.FullName}else{[void][TigirlMove]::MoveFileEx($dir.FullName,$null,4)}
 }
}
try {
 if($Action -in @('Preflight','Begin','Recover')){Restore-Transaction}
 $record=Read-Record
 if($Action -eq 'Recover'){
  if($record -and (Test-Path (Join-Path $record.directory 'setup\gui.txt'))){Set-GuiEntries $record}
  elseif($record){
   # A failed upgrade from the old ZIP package must not leave a second GUI uninstall entry.
   $arp='HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\NativeTiger_is1'
   if(Test-Path $arp){Remove-Item $arp -Recurse}
   $shortcut=Join-Path ([Environment]::GetFolderPath('CommonPrograms')) '虎娘\输入设置.lnk'
   if(Test-Path $shortcut){Remove-Item $shortcut}
  }
  exit 0
 }
 if($Action -eq 'Commit'){$pending=Get-Content $journal -Raw|ConvertFrom-Json;Assert-Owned ([pscustomobject]@{directory=$pending.directory})}else{Assert-Owned $record}
 if($Action -in @('Preflight','Begin')){
  if($Generation -notmatch '^[a-f0-9]{16}$'){throw 'Invalid generation.'}
  $manifest=Assert-Package $Payload
  New-Item $InstallRoot -ItemType Directory -Force|Out-Null
  ('Stage '+$Action+' '+(Get-Date).ToString('o'))|Add-Content $log -Encoding UTF8
  if($record -and [version]$manifest.version -lt [version]$record.version){throw 'Downgrading is not supported.'}
  $destination=Join-Path $InstallRoot "versions\$Generation"
  if(Test-Path $destination){
   # Inno must never replace an occupied version's bytes, even during repair.
   foreach($file in $manifest.files){$target=Join-Path $destination $file.path;if((Test-Path $target) -and (Get-FileHash $target).Hash -ne $file.sha256){throw 'Installed immutable version changed. Uninstall before replacing it.'}}
  }
  if($Action -eq 'Begin'){
   New-Item $InstallRoot -ItemType Directory -Force|Out-Null
   $tx=[guid]::NewGuid().ToString('N')
   Save-Json @{id=$tx;directory=$destination;previous=$record;state='prepared';version=$manifest.version} $journal
   [IO.File]::WriteAllText((Join-Path $InstallRoot 'setup-transaction.id'),$tx)
  }
 }elseif($Action -eq 'Apply'){
  $j=Get-Content $journal -Raw|ConvertFrom-Json
  Assert-Package $j.directory|Out-Null
  foreach($file in Get-ChildItem "$($j.directory)\x64" -File -Recurse){
   $relative=$file.FullName.Substring(("$($j.directory)\x64\").Length);if($relative -eq 'Tigirl.dll'){continue}
   $target=Join-Path "$($j.directory)\x86" $relative
   New-Item (Split-Path $target) -ItemType Directory -Force|Out-Null
   if(!(Test-Path $target)){New-Item $target -ItemType HardLink -Target $file.FullName|Out-Null}
  }
  $j.state='registering';Save-Json $j $journal
  Invoke-Registration $j.directory
  Set-MachineEntries $j.directory $j.version
  $j.state='registered';Save-Json $j $journal
 }elseif($Action -eq 'Commit'){
  $j=Get-Content $journal -Raw|ConvertFrom-Json
  # At this point the new registration owns the COM paths (Assert-Owned below is bypassed above).
  $previous=if($j.previous -and $j.previous.directory -ne $j.directory){$j.previous.directory}elseif($j.previous){$j.previous.previous}else{$null}
  $next=@{directory=$j.directory;version=$j.version;generation=(Split-Path $j.directory -Leaf);previous=$previous;transaction=$j.id}
  Save-Json $next $NativeTigerRecord
  Set-GuiEntries ([pscustomobject]$next)
  Move-Item $journal (Join-Path $InstallRoot ('committed-'+$j.id+'.json')) -Force
  # Cleanup is best effort after commit; never roll back a committed version for a locked file.
  foreach($old in Get-ChildItem (Join-Path $InstallRoot 'versions') -Directory){
   if($old.FullName -ne $j.directory -and $old.FullName -ne $previous -and $old.Name -match '^[a-f0-9]{16}$' -and (Test-Path (Join-Path $old.FullName 'manifest.json'))){try{Remove-Version $old.FullName}catch{($_|Out-String)|Add-Content $log -Encoding UTF8}}
  }
 }elseif($Action -in @('UninstallCheck','Uninstall')){
  if(!$record){throw 'Installation record is missing.'}
  Assert-Package $record.directory|Out-Null
  if($Action -eq 'Uninstall'){
   Invoke-Registration $record.directory -Remove
   Remove-MachineEntries
   Add-Type 'using System.Runtime.InteropServices; public static class TigirlMove {[DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool MoveFileEx(string a,string b,int f);}'
   $script:restart=$false
   # All adopted historical versions are constrained to this owned installation root.
   foreach($dir in Get-ChildItem (Join-Path $InstallRoot 'versions') -Directory){if($dir.Name -match '^[a-f0-9]{16}$' -and (Test-Path (Join-Path $dir.FullName 'manifest.json'))){Remove-Version $dir.FullName}}
   Remove-Item $NativeTigerRecord
   if($script:restart){exit 3010}
  }
 }
}catch{
 if(Test-Path $InstallRoot){($_|Out-String)|Add-Content $log -Encoding UTF8}
 Write-Error $_ -ErrorAction Continue;exit 1
}finally{$mutex.ReleaseMutex();$mutex.Dispose()}
