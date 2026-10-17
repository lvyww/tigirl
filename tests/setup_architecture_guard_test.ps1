param([Parameter(Mandatory)][string]$Compiler,[ValidateSet('universal','x64')][string]$Architecture='universal')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
. "$repo\packaging\common.ps1"
$before=@((Get-ComPath 'Registry64'),(Get-ComPath 'Registry32'),(Test-Path "$env:LOCALAPPDATA\Tigirl"),(Test-Path "$env:ProgramFiles\Tigirl")) -join '|'
$source=Get-Content "$repo\packaging\setup\Tigirl.iss" -Raw -Encoding UTF8
$preamble=$source.Substring(0,$source.IndexOf('[Setup]'))
$directives=@('ArchitecturesAllowed','ArchitecturesInstallIn64BitMode','MinVersion')|ForEach-Object {
 $value=[regex]::Match($source,'(?m)^'+$_+'=(.+)$').Value
 if(!$value){throw "Missing architecture policy: $_"};$value
}
$type=[regex]::Match($source,'(?s)type\s+(TInstallationGuid\s*=\s*record.*?end;)').Groups[1].Value
$helpers=[regex]::Match($source,'(?s)(function CoCreateGuid.*?)(?=function VersionDirectory)').Groups[1].Value
$entry=[regex]::Match($source,'(?s)function InitializeSetup: Boolean;.*?\nend;').Value.Replace('function InitializeSetup:', 'function ShippingInitializeSetup:')
if(!$type -or !$helpers -or !$entry){throw 'Shipping setup initialization was not found.'}
$fixture=Join-Path $env:TEMP ('Tigirl-architecture-entry-'+[guid]::NewGuid().ToString('N'))
New-Item $fixture -ItemType Directory|Out-Null
try {
 # Compile the shipping architecture directives and InitializeSetup, then abort
 # before any installer pages/files/registration; lowest privileges, no UAC.
 $text=@'
[Setup]
AppName=Tigirl architecture entry probe
AppVersion=1
DefaultDirName={tmp}\unused-architecture-probe
CreateAppDir=no
Uninstallable=no
PrivilegesRequired=lowest
OutputBaseFilename=architecture-probe
Compression=none
'@
 $text=$preamble+$text+"`r`n"+($directives -join "`r`n")+"`r`n[Code]`r`ntype`r`n"+$type+"`r`nvar InstallGeneration: String;`r`n"+$helpers+"`r`n"+$entry+@'

function InitializeSetup: Boolean;
begin
  if ShippingInitializeSetup then
    SaveStringToFile(ExpandConstant('{param:Output}'),InstallGeneration,False);
  Result:=False;
end;
'@
 $script=Join-Path $fixture 'architecture.iss';$output=Join-Path $fixture 'accepted.txt'
 $text|Set-Content $script -Encoding UTF8
 & $Compiler '/Qp' "/O$fixture" "/DPackageDir=$fixture" "/DPackageArchitecture=$Architecture" $script
 if($LASTEXITCODE){throw 'Architecture entry probe did not compile.'}
 $p=Start-Process (Join-Path $fixture 'architecture-probe.exe') -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/SP-',('/Output="'+$output+'"')) -Wait -PassThru
 $expectedAccepted=$Architecture -eq 'universal' -or (Get-SystemArchitecture) -eq 'X64'
 if($p.ExitCode -ne 1 -or (Test-Path $output) -ne $expectedAccepted){throw 'Shipping setup entry did not enforce the selected package architecture.'}
 if($expectedAccepted -and (Get-Content $output -Raw) -cnotmatch '^[a-f0-9]{32}$'){throw 'Setup did not initialize a valid generation.'}
 $after=@((Get-ComPath 'Registry64'),(Get-ComPath 'Registry32'),(Test-Path "$env:LOCALAPPDATA\Tigirl"),(Test-Path "$env:ProgramFiles\Tigirl")) -join '|'
 if($before -ne $after){throw 'Architecture entry probe changed live installation state.'}
 @{status='passed';architecture=(Get-SystemArchitecture);package_architecture=$Architecture;shipping_architecture_policy_accepted=$expectedAccepted;shipping_initialize_setup_accepted=$expectedAccepted;live_installation_unchanged=$true}|ConvertTo-Json
} finally {Remove-Item -LiteralPath $fixture -Recurse -Force}
