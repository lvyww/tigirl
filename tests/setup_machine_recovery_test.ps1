$ErrorActionPreference='Stop'
function Check($Value,$Message){if(!$Value){throw $Message}}
# Load function definitions only; never execute the elevated entry point or real registry calls.
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot\..\packaging\setup\deploy.ps1",[ref]$tokens,[ref]$errors)
if($errors){throw $errors}
foreach($fn in $ast.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)){
 Invoke-Expression $fn.Extent.Text
}
. "$PSScriptRoot\..\packaging\data.ps1"
function Get-PackageDll($Dir,$Arch){Join-Path $Dir "$Arch\Tigirl.dll"}
function Get-PackageTool($Dir){Join-Path $Dir 'x64\Tigirl.exe'}
function Get-ComPath($View){$script:registry[$View]}
function Invoke-Registration($Dir,[switch]$Remove){foreach($pair in @(@('Registry64','x64'),@('Registry32','x86'))){$script:registry[$pair[0]]=if($Remove){$null}else{Get-PackageDll $Dir $pair[1]}}}
function Set-MachineEntries($Dir,$Version){}
function Remove-MachineEntries(){}
$InstallRoot=Join-Path $env:TEMP ('Tigirl-machine-fixture-'+[guid]::NewGuid().ToString('N'))
$NativeTigerRecord=Join-Path $InstallRoot 'install.json';$journal=Join-Path $InstallRoot 'setup-transaction.json'
$old=Join-Path $InstallRoot 'versions\1111111111111111';$new=Join-Path $InstallRoot 'versions\2222222222222222'
New-Item $old,$new -ItemType Directory -Force|Out-Null
$previous=[pscustomobject]@{directory=$old;version='2026.9.9.5'}
try{
 $script:registry=@{Registry64=(Get-PackageDll $new 'x64');Registry32=(Get-PackageDll $old 'x86')}
 Save-Json @{id='mixed';directory=$new;previous=$previous;state='registering'} $journal
 Restore-Transaction
 Check ($script:registry.Registry64 -eq (Get-PackageDll $old 'x64')) 'Native registration not restored'
 Check ($script:registry.Registry32 -eq (Get-PackageDll $old 'x86')) 'x86 registration not restored'
 Check ((Get-Content $NativeTigerRecord -Raw|ConvertFrom-Json).directory -eq $old) 'Install record not restored'
 Check (!(Test-Path $journal)) 'Recovery was not committed'
 Restore-Transaction
 # A crash after atomic commit must not reactivate a now-retired previous version.
 $script:registry=@{Registry64=(Get-PackageDll $new 'x64');Registry32=(Get-PackageDll $new 'x86')}
 Save-Json @{directory=$new;version='2026.9.20.1';transaction='published';previous=$null} $NativeTigerRecord
 Save-Json @{id='published';directory=$new;previous=$previous;state='registered'} $journal
 Restore-Transaction
 Check ($script:registry.Registry64 -eq (Get-PackageDll $new 'x64')) 'Published commit rolled back'
 Check ((Get-Content $NativeTigerRecord -Raw|ConvertFrom-Json).directory -eq $new) 'Published install record replaced'
 Check (Test-Path (Join-Path $InstallRoot 'committed-published.json')) 'Published journal not finalized'
 Save-Json @{id='foreign';directory=$new;previous=$previous;state='registering'} $journal
 $script:registry.Registry32='C:\another-input-method\other.dll'
 $blocked=$false;try{Restore-Transaction}catch{$blocked=$true}
 Check $blocked 'Foreign registration was overwritten'
 Check ($script:registry.Registry32 -eq 'C:\another-input-method\other.dll') 'Foreign registration changed'
 Check (Test-Path $journal) 'Failed recovery discarded journal'
 $bad=$false;try{Assert-VersionPath (Join-Path $InstallRoot '..\elsewhere')}catch{$bad=$true}
 Check $bad 'Version path escape accepted'
 'PASS: half-registered upgrade rollback, durable record, idempotence, foreign ownership protection, version containment.'
}finally{Remove-Item $InstallRoot -Recurse -Force}
