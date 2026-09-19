$ErrorActionPreference='Stop'
function Check($Value,$Message){if(!$Value){throw $Message}}
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot\..\packaging\setup\deploy.ps1",[ref]$tokens,[ref]$errors)
if($errors){throw $errors}
foreach($fn in $ast.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)){Invoke-Expression $fn.Extent.Text}
. "$PSScriptRoot\..\packaging\data.ps1"
. "$PSScriptRoot\..\packaging\retirement.ps1"
# Intercept the native boundary: this test never changes pending reboot operations.
Add-Type @'
using System;
using System.Collections.Generic;
public static class TigirlMove {
 public static bool Fail;
 public static List<string> Paths=new List<string>();
 public static bool MoveFileEx(string source,string destination,int flags){
  if(destination != null) throw new Exception("Deletion destination must be native NULL.");
  if(flags != 4) throw new Exception("Unexpected flags.");
  Paths.Add(source);return !Fail;
 }
}
'@
$InstallRoot=Join-Path $env:TEMP ('Tigirl-locked-cleanup-'+[guid]::NewGuid().ToString('N'))
$dir=Join-Path $InstallRoot 'versions\1111111111111111'
New-Item "$dir\x64","$dir\setup" -ItemType Directory -Force|Out-Null
$files=@('common.ps1','data.ps1','setup\deploy.ps1','x64\Tigirl.dll','x64\data.bin')
foreach($file in $files){[IO.File]::WriteAllText((Join-Path $dir $file),'fixture')}
@{version='2026.9.10.2';files=@($files|ForEach-Object {@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $dir $_)).Hash}})}|ConvertTo-Json -Depth 4|Set-Content "$dir\manifest.json" -Encoding UTF8
$lock=[IO.File]::Open("$dir\x64\Tigirl.dll",[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try{
 $script:restart=$false
 Remove-Version $dir -KeepControlFiles
 Check $script:restart 'Locked DLL did not request restart'
 Check ([TigirlMove]::Paths.Contains("$dir\x64\Tigirl.dll")) 'Locked DLL was not queued'
 Check (!(Test-Path "$dir\x64\data.bin")) 'Unlocked file survived'
 foreach($file in @('manifest.json','common.ps1','data.ps1','setup\deploy.ps1')){Check (Test-Path (Join-Path $dir $file)) "Retry control file removed: $file"}
 Assert-UninstallPackage $dir
 # A second cleanup accepts missing files and retains its executable backend.
 Remove-Version $dir -KeepControlFiles
 [TigirlMove]::Fail=$true
 $blocked=$false;try{Remove-Version $dir -KeepControlFiles}catch{$blocked=$true;Check ($_.Exception.Message -like '*Cannot schedule deletion*') 'Lost native failure context'}
 Check $blocked 'Failed scheduling was ignored'
 Check (Test-Path "$dir\manifest.json") 'Failure removed inventory'
 [TigirlMove]::Fail=$false
 $lock.Dispose();$lock=$null
 Remove-Version $dir
 Check (!(Test-Path $dir)) 'Unlocked historical version was not removed'
 'PASS: locked-file fallback uses NULL, requests restart, preserves retry controls, supports partial cleanup, reports scheduling failure, removes unlocked versions.'
}finally{if($lock){$lock.Dispose()};if(Test-Path $InstallRoot){Remove-Item $InstallRoot -Recurse -Force}}
