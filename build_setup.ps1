param([string]$Version='2026.9.10.1',[switch]$SkipBuild,[switch]$RecompileOnly,[string]$Compiler="$PSScriptRoot\build\tools\InnoSetup-6.7.3\ISCC.exe")
$ErrorActionPreference='Stop'
if(!(Test-Path $Compiler)){throw 'Install Inno Setup 6.7.3, then pass -Compiler <ISCC.exe>.'}
if((Get-FileHash (Join-Path (Split-Path $Compiler) 'ISCC.exe')).Hash -ne '0A8757031B33777E4C9CBFFEE40F11A5062B36D25CBE144C1DB73B6102B80AD7'){throw 'Compiler checksum mismatch; use Inno Setup 6.7.3.'}
if((Get-FileHash (Join-Path (Split-Path $Compiler) 'ISCmplr.dll')).Hash -ne '85A1E3090D3A5B85319F001B7C8F9ECFAD45F37EFF030A67BBE29EF58B7AA2C3'){throw 'Compiler checksum mismatch; use Inno Setup 6.7.3.'}
$base="$PSScriptRoot\build\packages\Tigirl-$Version-x64"
if(!$RecompileOnly -and (!$SkipBuild -or !(Test-Path $base))){
 & "$PSScriptRoot\package_x64.ps1" -Version $Version -SkipBuild:$SkipBuild
 if($LASTEXITCODE){throw 'Package build failed.'}
}
# Separate stage: existing ZIP installer keeps its existing lifecycle and uninstall entry.
$stage="$PSScriptRoot\build\packages\Tigirl-$Version-x64-setup-payload"
if(!$RecompileOnly){
if(Test-Path $stage){throw 'Setup staging directory exists; choose a new version or explicitly remove the staging directory.'}
Copy-Item -LiteralPath $base -Destination $stage -Recurse
Copy-Item "$PSScriptRoot\packaging\setup\gui.txt" "$stage\setup\gui.txt"
Copy-Item "$PSScriptRoot\packaging\setup\README.txt" "$stage\README.txt" -Force
foreach($legacy in @('install.cmd','install.ps1','uninstall.ps1','rollback.ps1')){Remove-Item -LiteralPath (Join-Path $stage $legacy)}
$files=@(Get-ChildItem $stage -Recurse -File|Where-Object Name -ne 'manifest.json'|Sort-Object FullName|ForEach-Object{
 [ordered]@{path=$_.FullName.Substring($stage.Length+1);bytes=$_.Length;sha256=(Get-FileHash $_.FullName).Hash}
})
[ordered]@{version=$Version;target='Windows x64, x64+x86 TSF';files=$files}|ConvertTo-Json -Depth 5|Set-Content "$stage\manifest.json" -Encoding UTF8
}
. "$stage\common.ps1"
Assert-Package $stage|Out-Null
$generation=(Get-FileHash "$stage\manifest.json").Hash.Substring(0,16).ToLowerInvariant()
& $Compiler "/DPackageDir=$stage" "/DGeneration=$generation" "/DProductVersion=$Version" "/DOutputPath=$PSScriptRoot\build\packages" "/DChineseMessages=$PSScriptRoot\packaging\setup\ChineseSimplified.isl" "$PSScriptRoot\packaging\setup\Tigirl.iss"
if($LASTEXITCODE){throw 'Installer compilation failed.'}
$exe="$PSScriptRoot\build\packages\Tigirl-$Version-x64-Setup.exe"
(Get-FileHash $exe).Hash+'  '+[IO.Path]::GetFileName($exe)|Set-Content "$exe.sha256" -Encoding ASCII
@{version=$Version;generation=$generation;compiler='Inno Setup 6.7.3';sha256=(Get-FileHash $exe).Hash;bytes=(Get-Item $exe).Length}|ConvertTo-Json|Set-Content "$exe.build.json" -Encoding UTF8
Write-Output $exe
