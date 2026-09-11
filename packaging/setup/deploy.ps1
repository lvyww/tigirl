# Machine-only transaction coordinator. Installed by Inno; never accepts user data paths.
param([Parameter(Mandatory)][ValidateSet('Preflight','Begin','Apply','Commit','Recover','UninstallCheck','Uninstall')][string]$Action,
 [Parameter(Mandatory)][string]$InstallRoot,[string]$Generation,[string]$Payload)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\common.ps1"
. "$PSScriptRoot\..\data.ps1"
Assert-X64System
if(!(Test-Administrator)){throw 'Administrator privileges are required.'}
$allowed=@((Join-Path $env:ProgramFiles 'Tigirl'))
$InstallRoot=[IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
if($InstallRoot -notin $allowed){throw 'Invalid installation root.'}
Assert-PlainTree $InstallRoot
$NativeTigerInstallRoot=$InstallRoot;$NativeTigerRecord=Join-Path $InstallRoot 'install.json'
$journal=Join-Path $InstallRoot 'setup-transaction.json'
$backendDirectory=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$logDir=Join-Path $env:ProgramData 'Tigirl\Logs'
New-Item $logDir -ItemType Directory -Force|Out-Null
$log=Join-Path $logDir 'setup.log'
$mutex=[Threading.Mutex]::new($false,'Global\Tigirl.Deployment')
if(!$mutex.WaitOne(0)){throw 'Another deployment is running.'}
function Write-SetupLog([string]$Message){if($log){('['+(Get-Date).ToString('o')+'] '+$Message)|Add-Content -LiteralPath $log -Encoding UTF8}}
function Save-Json($Value,[string]$Path){
 $tmp=$Path+'.tmp';$Value|ConvertTo-Json -Depth 15|Set-Content $tmp -Encoding UTF8
 if(Test-Path $Path){[IO.File]::Replace($tmp,$Path,[NullString]::Value)}else{[IO.File]::Move($tmp,$Path)}
}
function Assert-VersionPath([string]$Directory){
 $p=[IO.Path]::GetFullPath($Directory).TrimEnd('\')
 $versions=[IO.Path]::GetFullPath((Join-Path $InstallRoot 'versions')).TrimEnd('\')
 $parent=[IO.Path]::GetFullPath((Split-Path $p -Parent)).TrimEnd('\')
 if($parent -ne $versions -or (Split-Path $p -Leaf) -notmatch '^[a-f0-9]{16}$'){throw "Invalid managed version directory: $p"}
 Assert-PlainTree $p
}
function Read-Record {
 if(!(Test-Path $NativeTigerRecord)){return $null}
 try{
  $record=Get-Content $NativeTigerRecord -Raw|ConvertFrom-Json
  if(!$record.directory){throw 'Installation record has no directory.'}
  Assert-VersionPath $record.directory
  return $record
 }catch{Write-SetupLog ('Ignoring invalid install.json during repair: '+$_.Exception.Message);return $null}
}
function Get-ManagedRegistrationDirectory([string]$Dll,[string]$Architecture){
 if(!$Dll){return $null}
 try{$full=[IO.Path]::GetFullPath($Dll)}catch{return $null}
 if((Split-Path $full -Leaf) -ne 'Tigirl.dll'){return $null}
 $archDir=Split-Path $full -Parent
 if((Split-Path $archDir -Leaf) -ne $Architecture){return $null}
 $directory=Split-Path $archDir -Parent
 try{Assert-VersionPath $directory}catch{return $null}
 return $directory
}
function Get-ManagedToolDirectory([string]$Tool){
 if(!$Tool){return $null}
 try{$full=[IO.Path]::GetFullPath($Tool)}catch{return $null}
 if((Split-Path $full -Leaf) -ne 'Tigirl.exe'){return $null}
 $archDir=Split-Path $full -Parent
 if((Split-Path $archDir -Leaf) -ne 'x64'){return $null}
 $directory=Split-Path $archDir -Parent
 try{Assert-VersionPath $directory}catch{return $null}
 return $directory
}
function Get-DirectoryVersion([string]$Directory,[string]$Fallback='0.0.0.0'){
 try{
  $manifest=Get-Content -LiteralPath (Join-Path $Directory 'manifest.json') -Raw|ConvertFrom-Json
  if($manifest.version -match '^\d+\.\d+\.\d+\.\d+$'){return $manifest.version}
 }catch{}
 if($Fallback -match '^\d+\.\d+\.\d+\.\d+$'){return $Fallback}
 return '0.0.0.0'
}
function Assert-NoForeignMachineState($Record){
 foreach($pair in @(@('Registry64','x64'),@('Registry32','x86'))){
  $actual=Get-ComPath $pair[0]
  if($actual -and !(Get-ManagedRegistrationDirectory $actual $pair[1])){throw 'Input method registration belongs to another installation.'}
 }
 $uri='HKLM:\SOFTWARE\Classes\nativetiger\shell\open\command'
 if(Test-Path $uri){
  $command=(Get-Item $uri).GetValue('')
  $tool=$null
  if($command -match '^"([^"]+)" --uri "%1"$'){$tool=$matches[1]}
  if(!$tool -or !(Get-ManagedToolDirectory $tool)){throw 'URI protocol belongs to another installation.'}
 }
 if($Record){Assert-VersionPath $Record.directory}
}
function Resolve-OwnedRecord {
 $record=Read-Record
 Assert-NoForeignMachineState $record
 $d64=Get-ManagedRegistrationDirectory (Get-ComPath 'Registry64') 'x64'
 $d32=Get-ManagedRegistrationDirectory (Get-ComPath 'Registry32') 'x86'
 if($d64 -and $d32 -and $d64 -ne $d32){Write-SetupLog "Repairing split x64/x86 registration: x64=$d64 x86=$d32"}
 $directory=if($d64){$d64}elseif($d32){$d32}elseif($record){$record.directory}else{$null}
 if(!$directory){return $null}
 if($record -and $record.directory -eq $directory){return $record}
 $fallback=if($record -and $record.directory -eq $directory){$record.version}else{'0.0.0.0'}
 $version=Get-DirectoryVersion $directory $fallback
 Write-SetupLog "Adopting managed registration without a usable matching install record: $directory"
 return [pscustomobject]@{directory=$directory;version=$version;generation=(Split-Path $directory -Leaf);previous=$(if($record){$record.previous}else{$null});adopted=$true}
}
function Test-RollbackableRecord($Record){
 if(!$Record){return $false}
 try{Assert-VersionPath $Record.directory}catch{return $false}
 return (Test-Path -LiteralPath (Get-PackageDll $Record.directory 'x64') -PathType Leaf) -and (Test-Path -LiteralPath (Get-PackageDll $Record.directory 'x86') -PathType Leaf)
}
function Assert-RegisteredDirectory([string]$Directory){
 if((Get-ComPath 'Registry64') -ne (Get-PackageDll $Directory 'x64') -or (Get-ComPath 'Registry32') -ne (Get-PackageDll $Directory 'x86')){throw 'COM registration verification failed.'}
}
function Remove-RegistrationKeys {
 foreach($view in @('Registry64','Registry32')){
  $base=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,[Microsoft.Win32.RegistryView]::$view)
  try{$base.DeleteSubKeyTree("SOFTWARE\Classes\CLSID\$NativeTigerClsid",$false)}finally{$base.Dispose()}
 }
 $tip="HKLM:\SOFTWARE\Microsoft\CTF\TIP\$NativeTigerClsid"
 if(Test-Path $tip){Remove-Item -LiteralPath $tip -Recurse -Force}
}
function Restore-Transaction {
 if(!(Test-Path $journal)){return}
 $j=Get-Content $journal -Raw|ConvertFrom-Json
 Assert-VersionPath $j.directory
 if($j.previous){Assert-VersionPath $j.previous.directory}
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
  if((Get-ComPath 'Registry64') -or (Get-ComPath 'Registry32')){
   try{Invoke-Registration $j.directory -Remove}catch{Write-SetupLog ('Normal transaction cleanup failed; removing owned registration keys: '+$_.Exception.Message);Remove-RegistrationKeys}
  }
  Remove-MachineEntries
  if(Test-Path $NativeTigerRecord){Remove-Item $NativeTigerRecord -Force}
  $arp='HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Tigirl_is1'
  if(Test-Path $arp){Remove-Item $arp -Recurse -Force}
  $shortcut=Join-Path ([Environment]::GetFolderPath('CommonPrograms')) '虎娘\输入设置.lnk'
  if(Test-Path $shortcut){Remove-Item $shortcut -Force}
 }
 Move-Item $journal (Join-Path $InstallRoot ('recovered-'+$j.id+'.json')) -Force
 Write-SetupLog ('Recovered interrupted transaction '+$j.id)
}
function Set-GuiEntries($Record){
 Set-MachineEntries $Record.directory $Record.version
 $arp='HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Tigirl_is1'
 if(Test-Path $arp){Set-ItemProperty $arp UninstallString ('"'+$InstallRoot+'\Tigirl.Maintenance.exe"');Set-ItemProperty $arp QuietUninstallString ('"'+$InstallRoot+'\unins000.exe" /VERYSILENT /NORESTART');Set-ItemProperty $arp DisplayVersion $Record.version}
 $legacy='HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Tigirl'
 if(Test-Path $legacy){Remove-Item $legacy -Recurse -Force}
 $shell=New-Object -ComObject WScript.Shell
 try{
  $dir=Join-Path ([Environment]::GetFolderPath('CommonPrograms')) '虎娘';New-Item $dir -ItemType Directory -Force|Out-Null
  $link=$shell.CreateShortcut((Join-Path $dir '输入设置.lnk'));$link.TargetPath=Get-PackageTool $Record.directory;$link.IconLocation=$link.TargetPath+',0';$link.Save()
 }finally{[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)}
}
function Queue-Deletion([string]$Path){
 if(!('TigirlMove' -as [type])){Add-Type 'using System.Runtime.InteropServices; public static class TigirlMove {[DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool MoveFileEx(string a,string b,int f);}'}
 if(![TigirlMove]::MoveFileEx($Path,[NullString]::Value,4)){
  $errorCode=[Runtime.InteropServices.Marshal]::GetLastWin32Error()
  throw [ComponentModel.Win32Exception]::new($errorCode,"Cannot schedule deletion: $Path")
 }
 $script:restart=$true
}
function Assert-UninstallPackage([string]$Directory){
 Assert-VersionPath $Directory
 $m=Get-Content -LiteralPath (Join-Path $Directory 'manifest.json') -Raw|ConvertFrom-Json
 if($m.version -notmatch '^\d+\.\d+\.\d+\.\d+$'){throw 'Invalid package version.'}
 foreach($entry in $m.files){
  if(!$entry.path -or $entry.path -match '(^[\\/]|:|(^|[\\/])\.\.([\\/]|$))'){throw 'Invalid manifest path.'}
 }
 return $m
}
function Remove-Version([string]$Directory,[switch]$KeepControlFiles){
 Assert-VersionPath $Directory
 # Delete only manifest-listed package files and explicitly created x86 hard links.
 $m=Get-Content (Join-Path $Directory 'manifest.json') -Raw|ConvertFrom-Json
 $paths=@($m.files|ForEach-Object path)
 foreach($entry in $m.files){if($entry.path -like 'x64\*' -and $entry.path -ne 'x64\Tigirl.dll'){$paths+=('x86\'+$entry.path.Substring(4))}}
 foreach($relative in $paths|Select-Object -Unique){
  if($KeepControlFiles -and $relative -in @('common.ps1','data.ps1','setup\deploy.ps1')){continue}
  if($relative -match '(^[\\/]|:|(^|[\\/])\.\.([\\/]|$))'){throw 'Invalid manifest path.'}
  $file=Join-Path $Directory $relative
  if(Test-Path -LiteralPath $file -PathType Leaf){try{Remove-Item -LiteralPath $file -Force}catch{Queue-Deletion $file}}
 }
 # Inno removes the active backend and inventory only after cleanup succeeds.
 if($KeepControlFiles){return}
 Remove-Item -LiteralPath (Join-Path $Directory 'manifest.json') -Force
 foreach($dir in @(Get-ChildItem $Directory -Directory -Recurse|Sort-Object {$_.FullName.Length} -Descending)+@(Get-Item $Directory)){
  if(!(Get-ChildItem $dir.FullName -Force|Select-Object -First 1)){Remove-Item $dir.FullName}else{Queue-Deletion $dir.FullName}
 }
}
try {
 Write-SetupLog "Action $Action started. InstallRoot=$InstallRoot Generation=$Generation"
 if($Action -in @('Preflight','Begin','Recover','UninstallCheck','Uninstall')){Restore-Transaction}
 $record=Resolve-OwnedRecord
 if($Action -eq 'Recover'){
  if($record -and (Test-Path (Join-Path $record.directory 'setup\gui.txt'))){Set-GuiEntries $record}
  elseif($record){
   # A failed upgrade from the old ZIP package must not leave a second GUI uninstall entry.
   $arp='HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Tigirl_is1'
   if(Test-Path $arp){Remove-Item $arp -Recurse -Force}
   $shortcut=Join-Path ([Environment]::GetFolderPath('CommonPrograms')) '虎娘\输入设置.lnk'
   if(Test-Path $shortcut){Remove-Item $shortcut -Force}
  }
  exit 0
 }
 if($Action -in @('Preflight','Begin')){
  if($Generation -notmatch '^[a-f0-9]{16}$'){throw 'Invalid generation.'}
  $manifest=Assert-Package $Payload
  New-Item $InstallRoot -ItemType Directory -Force|Out-Null
  if($record -and $record.version -match '^\d+\.\d+\.\d+\.\d+$' -and [version]$manifest.version -lt [version]$record.version){throw 'Downgrading is not supported.'}
  $destination=Join-Path $InstallRoot "versions\$Generation"
  if(Test-Path $destination){
   # Inno must never replace an occupied version's bytes, even during repair.
   foreach($file in $manifest.files){$target=Join-Path $destination $file.path;if((Test-Path $target) -and (Get-FileHash $target).Hash -ne $file.sha256){throw 'Installed immutable version changed. Uninstall before replacing it.'}}
  }
  if($Action -eq 'Begin'){
   New-Item $InstallRoot -ItemType Directory -Force|Out-Null
   $tx=[guid]::NewGuid().ToString('N')
   $previous=if(Test-RollbackableRecord $record){$record}else{$null}
   if($record -and !$previous){Write-SetupLog ('Existing managed installation is not rollbackable; upgrade will repair forward only: '+$record.directory)}
   Save-Json @{id=$tx;directory=$destination;previous=$previous;state='prepared';version=$manifest.version} $journal
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
  $j.state='unregistering';Save-Json $j $journal
  if((Get-ComPath 'Registry64') -or (Get-ComPath 'Registry32')){
   Assert-NoForeignMachineState $j.previous
   if($j.previous){
    try{Invoke-Registration $j.previous.directory -Remove}catch{Write-SetupLog ('Old DLL unregistration failed; removing only Tigirl-owned registration keys before forward repair: '+$_.Exception.Message);Remove-RegistrationKeys}
   }else{Remove-RegistrationKeys}
  }
  $j.state='registering';Save-Json $j $journal
  Invoke-Registration $j.directory
  Set-MachineEntries $j.directory $j.version
  Assert-RegisteredDirectory $j.directory
  $j.state='registered';Save-Json $j $journal
 }elseif($Action -eq 'Commit'){
  $j=Get-Content $journal -Raw|ConvertFrom-Json
  Assert-RegisteredDirectory $j.directory
  $previous=if($j.previous -and $j.previous.directory -ne $j.directory){$j.previous.directory}elseif($j.previous){$j.previous.previous}else{$null}
  $next=@{directory=$j.directory;version=$j.version;generation=(Split-Path $j.directory -Leaf);previous=$previous;transaction=$j.id}
  Save-Json $next $NativeTigerRecord
  Set-GuiEntries ([pscustomobject]$next)
  Move-Item $journal (Join-Path $InstallRoot ('committed-'+$j.id+'.json')) -Force
  # Cleanup is best effort after commit; never roll back a committed version for a locked or damaged old file.
  foreach($old in Get-ChildItem (Join-Path $InstallRoot 'versions') -Directory -ErrorAction SilentlyContinue){
   if($old.FullName -ne $j.directory -and $old.FullName -ne $previous -and $old.Name -match '^[a-f0-9]{16}$' -and (Test-Path (Join-Path $old.FullName 'manifest.json'))){try{Remove-Version $old.FullName}catch{Write-SetupLog ('Old version cleanup deferred/retained: '+$old.FullName+' :: '+$_.Exception.Message)}}
  }
 }elseif($Action -in @('UninstallCheck','Uninstall')){
  # Metadata damage is not a reason to trap the user. Foreign registration is still fatal.
  Assert-NoForeignMachineState $record
  if($record){try{Assert-UninstallPackage $record.directory|Out-Null}catch{Write-SetupLog ('Uninstall inventory is incomplete; registration removal will continue and unknown files will be retained: '+$_.Exception.Message)}}
  if($Action -eq 'Uninstall'){
   Write-SetupLog 'Stage Unregister'
   $hasRegistration=(Get-ComPath 'Registry64') -or (Get-ComPath 'Registry32')
   if($hasRegistration){
    $unregisterDirectory=Get-ManagedRegistrationDirectory (Get-ComPath 'Registry64') 'x64'
    if(!$unregisterDirectory -and $record){$unregisterDirectory=$record.directory}
    if($unregisterDirectory -and (Test-Path -LiteralPath (Get-PackageDll $unregisterDirectory 'x64') -PathType Leaf)){
     try{Invoke-Registration $unregisterDirectory -Remove}catch{Write-SetupLog ('regsvr32 unregistration failed; removing Tigirl registration keys directly: '+$_.Exception.Message);Remove-RegistrationKeys}
    }else{
     Write-SetupLog 'Registered x64 DLL is unavailable; removing Tigirl registration keys directly.'
     Remove-RegistrationKeys
    }
   }
   # Idempotent final cleanup of this product's GUID-owned registry state.
   Remove-RegistrationKeys
   Remove-MachineEntries
   Write-SetupLog 'Stage FileCleanup'
   $script:restart=$false
   # File cleanup is best effort. Registration removal is the uninstall commit point.
   foreach($dir in Get-ChildItem (Join-Path $InstallRoot 'versions') -Directory -ErrorAction SilentlyContinue){
    if($dir.Name -match '^[a-f0-9]{16}$' -and (Test-Path (Join-Path $dir.FullName 'manifest.json'))){
     try{Remove-Version $dir.FullName -KeepControlFiles:($dir.FullName -eq $backendDirectory)}catch{Write-SetupLog ('Program files retained for retry/reboot: '+$dir.FullName+' :: '+$_.Exception.Message)}
    }
   }
   if(Test-Path $NativeTigerRecord){Remove-Item $NativeTigerRecord -Force}
   if($script:restart){exit 3010}
  }
 }
}catch{
 try{Write-SetupLog ($_|Out-String)}catch{}
 Write-Error $_ -ErrorAction Continue;exit 1
}finally{$mutex.ReleaseMutex();$mutex.Dispose()}
