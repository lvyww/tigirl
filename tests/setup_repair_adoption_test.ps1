$ErrorActionPreference='Stop'
function Check($Value,$Message){if(!$Value){throw $Message}}
function SamePath([string]$A,[string]$B){return [IO.Path]::GetFullPath($A).TrimEnd('\') -eq [IO.Path]::GetFullPath($B).TrimEnd('\')}
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot\..\packaging\setup\deploy.ps1",[ref]$tokens,[ref]$errors)
if($errors){throw $errors}
foreach($fn in $ast.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)){Invoke-Expression $fn.Extent.Text}
. "$PSScriptRoot\..\packaging\data.ps1"
$InstallRoot=Join-Path $env:TEMP ('Tigirl-repair-adoption-'+[guid]::NewGuid().ToString('N'))
$NativeTigerClsid='{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}'
$NativeTigerRecord=Join-Path $InstallRoot 'install.json'
$log=Join-Path $InstallRoot 'repair.log'
$dir=Join-Path $InstallRoot 'versions\1111111111111111'
$other=Join-Path $InstallRoot 'versions\2222222222222222'
New-Item "$dir\x64","$dir\x86","$other\x64","$other\x86" -ItemType Directory -Force|Out-Null
foreach($root in @($dir,$other)){
 [IO.File]::WriteAllText((Join-Path $root 'common.ps1'),$NativeTigerClsid)
 [IO.File]::WriteAllText((Join-Path $root 'x64\Tigirl.dll'),'fixture')
 [IO.File]::WriteAllText((Join-Path $root 'x86\Tigirl.dll'),'fixture')
 @{version='2026.9.12.1';files=@()}|ConvertTo-Json|Set-Content (Join-Path $root 'manifest.json') -Encoding UTF8
}
$script:registry=@{Registry64=(Join-Path $dir 'x64\Tigirl.dll');Registry32=(Join-Path $dir 'x86\Tigirl.dll')}
function Get-ComPath($View){$script:registry[$View]}
# The fixture must not inspect the workstation's real protocol registration.
function Get-MachineUriCommand { return $null }
try{
 Check (SamePath (Get-ManagedRegistrationDirectory $script:registry.Registry64 'x64') $dir) 'Managed x64 registration path was not recognized'
 Check (SamePath (Get-ManagedRegistrationDirectory $script:registry.Registry32 'x86') $dir) 'Managed x86 registration path was not recognized'
 $record=Resolve-OwnedRecord
 Check ($null -ne $record) 'Missing install.json produced no adopted record'
 Check (SamePath $record.directory $dir) ("Missing install.json adopted the wrong directory: expected=$dir actual=$($record.directory)")
 Check ($record.version -eq '2026.9.12.1') 'Adopted version did not come from the managed manifest'
 [IO.File]::WriteAllText($NativeTigerRecord,'{broken json')
 $record=Resolve-OwnedRecord
 Check (SamePath $record.directory $dir) 'Corrupt install.json blocked repair adoption'
 Check ((Get-Content $log -Raw) -match 'Ignoring invalid install.json') 'Corrupt record repair was not logged'
 $script:registry.Registry32=Join-Path $other 'x86\Tigirl.dll'
 $record=Resolve-OwnedRecord
 Check (SamePath $record.directory $dir) 'Split registration did not prefer the x64 managed generation'
 Check ((Get-Content $log -Raw) -match 'Repairing split x64/x86 registration') 'Split registration repair was not logged'
 $script:registry.Registry32='C:\ForeignIme\Tigirl.dll'
 $blocked=$false;try{Resolve-OwnedRecord}catch{$blocked=$true}
 Check $blocked 'Registration outside Program Files\Tigirl managed versions was adopted'
 'PASS: missing/corrupt record adoption, split-registration repair, foreign-registration rejection.'
}finally{if(Test-Path $InstallRoot){Remove-Item $InstallRoot -Recurse -Force}}
