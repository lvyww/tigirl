param([Parameter(Mandatory)][string]$Compiler)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$Compiler=(Resolve-Path -LiteralPath $Compiler).Path
$pins=@{'ISCC.exe'='0A8757031B33777E4C9CBFFEE40F11A5062B36D25CBE144C1DB73B6102B80AD7';'ISCmplr.dll'='85A1E3090D3A5B85319F001B7C8F9ECFAD45F37EFF030A67BBE29EF58B7AA2C3'}
foreach($name in $pins.Keys){if((Get-FileHash (Join-Path (Split-Path $Compiler) $name)).Hash -ne $pins[$name]){throw "Compiler pin mismatch: $name"}}
$fixture=Join-Path $env:TEMP ('Tigirl-compiler-fixture-'+[guid]::NewGuid().ToString('N'))
$payload=Join-Path $fixture 'payload';$output=Join-Path $fixture 'output'
New-Item "$payload\setup",$output -ItemType Directory -Force|Out-Null
try{
 # Compile the REAL installer source against small dummy files; never run it.
 foreach($name in @('common.ps1','data.ps1','retirement.ps1')){Copy-Item (Join-Path $repo "packaging\$name") $payload}
 Copy-Item (Join-Path $repo 'packaging\setup\deploy.ps1') "$payload\setup"
 Copy-Item (Join-Path $repo 'assets\Tigirl.ico') $payload
 [IO.File]::WriteAllText("$payload\setup\Tigirl.Maintenance.exe",'compile-only fixture')
 $iss=Join-Path $repo 'packaging\setup\Tigirl.iss'
 & $Compiler '/Qp' "/DPackageDir=$payload" '/DProductVersion=2026.9.20.1' "/DOutputPath=$output" "/DChineseMessages=$repo\packaging\setup\ChineseSimplified.isl" $iss
 if($LASTEXITCODE){throw 'Actual installer script did not compile.'}
 # Separately exercise ONLY the shipping GUID generator. This probe aborts in
 # InitializeSetup, before installation, and contains no files or registry entries.
 $source=Get-Content -LiteralPath $iss -Raw -Encoding UTF8
 $type=[regex]::Match($source,'(?s)type\s+(TInstallationGuid\s*=\s*record.*?end;)').Groups[1].Value
 $helpers=[regex]::Match($source,'(?s)(function CoCreateGuid.*?)(?=function VersionDirectory)').Groups[1].Value
 if(!$type -or !$helpers){throw 'Shipping GUID implementation was not found.'}
 $probe=Join-Path $fixture 'guid-probe.iss'
 $text=@'
[Setup]
AppName=Tigirl GUID fixture
AppVersion=1
DefaultDirName={tmp}\unused-guid-fixture
CreateAppDir=no
Uninstallable=no
PrivilegesRequired=lowest
OutputBaseFilename=guid-probe
Compression=none
[Code]
type
'@
 $text+="`r`n"+$type+"`r`n"+$helpers+@'
function InitializeSetup: Boolean;
begin
  SaveStringToFile(ExpandConstant('{param:Output}'),NewInstallGeneration,False);
  Result:=False;
end;
'@
 $text|Set-Content -LiteralPath $probe -Encoding UTF8
 & $Compiler '/Qp' "/O$output" $probe
 if($LASTEXITCODE){throw 'GUID-only probe did not compile.'}
 $ids=@()
 foreach($n in 1..2){
  $path=Join-Path $fixture "guid-$n.txt"
  $p=Start-Process (Join-Path $output 'guid-probe.exe') -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/SP-',('/Output="'+$path+'"')) -Wait -PassThru
  if($p.ExitCode -ne 1 -or !(Test-Path -LiteralPath $path)){throw 'GUID probe did not abort before installation.'}
  $id=(Get-Content -LiteralPath $path -Raw).Trim()
  if($id -cnotmatch '^[a-f0-9]{32}$'){throw "Invalid runtime GUID: $id"}
  $ids+=$id
 }
 if($ids[0] -eq $ids[1]){throw 'Two setup runs reused an installation generation.'}
 'PASS: pinned Inno 6.7.3, full installer-source compilation, real GUID generator, two distinct runtime generations; no Tigirl installation executed.'
}finally{if(Test-Path $fixture){Remove-Item -LiteralPath $fixture -Recurse -Force}}
