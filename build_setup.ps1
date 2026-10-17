param([string]$Version='2026.10.15.1',[switch]$SkipBuild,[switch]$RecompileOnly,[string]$Compiler="$PSScriptRoot\build\tools\InnoSetup-6.7.3\ISCC.exe",[ValidateSet('universal','x64')][string]$Architecture='universal',[string]$OutputDirectory="$PSScriptRoot\build\packages",[string]$WorkDirectory="$PSScriptRoot\build\packages",[switch]$StageOnly)
$ErrorActionPreference='Stop'
if($StageOnly -and $RecompileOnly){throw 'StageOnly and RecompileOnly cannot be combined.'}
if(!(Test-Path $Compiler)){throw 'Install Inno Setup 6.7.3, then pass -Compiler <ISCC.exe>.'}
if((Get-FileHash -LiteralPath $Compiler).Hash -ne '0A8757031B33777E4C9CBFFEE40F11A5062B36D25CBE144C1DB73B6102B80AD7'){throw 'Compiler checksum mismatch; use Inno Setup 6.7.3.'}
if((Get-FileHash (Join-Path (Split-Path $Compiler) 'ISCmplr.dll')).Hash -ne '85A1E3090D3A5B85319F001B7C8F9ECFAD45F37EFF030A67BBE29EF58B7AA2C3'){throw 'Compiler checksum mismatch; use Inno Setup 6.7.3.'}
$Architecture=$Architecture.ToLowerInvariant()
$includeArm64=$Architecture -eq 'universal'
$label=if($includeArm64){'x64-ARM64-x86'}else{'x64-x86'}
$target=if($includeArm64){'Windows x64/ARM64, x64+ARM64X+x86 TSF'}else{'Windows x64, x64+x86 TSF'}
$WorkDirectory=[IO.Path]::GetFullPath($WorkDirectory)
$base=Join-Path $WorkDirectory "Tigirl-$Version-$Architecture"
if(!$RecompileOnly -and (!$SkipBuild -or !(Test-Path $base))){
 & "$PSScriptRoot\package_x64.ps1" -Version $Version -IncludeArm64:$includeArm64 -SkipBuild:$SkipBuild -OutputDirectory $WorkDirectory -NoZip
 if($LASTEXITCODE){throw 'Package build failed.'}
}
# Separate stage: existing ZIP installer keeps its existing lifecycle and uninstall entry.
$stage=Join-Path $WorkDirectory "Tigirl-$Version-$Architecture-setup-payload"
if(!$RecompileOnly){
if(Test-Path $stage){throw 'Setup staging directory exists; choose a new version or explicitly remove the staging directory.'}
Copy-Item -LiteralPath $base -Destination $stage -Recurse
Copy-Item "$PSScriptRoot\packaging\setup\gui.txt" "$stage\setup\gui.txt"
Copy-Item "$PSScriptRoot\packaging\setup\README.txt" "$stage\README.txt" -Force
if(!$includeArm64){
 $readme=Get-Content "$stage\README.txt" -Raw -Encoding UTF8
 $readme=$readme.Replace('x64-ARM64-x86','x64-x86').Replace('，以及 ARM64 Windows 11','')
 $readme=$readme.Replace('安装时自动选择本机输入法组件：x64 系统使用 x64 DLL，ARM64 系统使用 ARM64X DLL，兼容 ARM64 和 x64 应用。桌面工具使用共享的 x64 版本。不支持 32 位 Windows。','此包使用 x64 和 x86 输入法组件，桌面工具使用共享的 x64 版本。适用于 Intel/AMD x64 系统；ARM64 设备请使用 universal 安装包。不支持 32 位 Windows。')
 $readme|Set-Content "$stage\README.txt" -Encoding UTF8
}
foreach($legacy in @('install.cmd','install.ps1','uninstall.ps1','rollback.ps1')){Remove-Item -LiteralPath (Join-Path $stage $legacy)}
$files=@(Get-ChildItem $stage -Recurse -File|Where-Object Name -ne 'manifest.json'|Sort-Object FullName|ForEach-Object{
 [ordered]@{path=$_.FullName.Substring($stage.Length+1);bytes=$_.Length;sha256=(Get-FileHash $_.FullName).Hash}
})
[ordered]@{version=$Version;target=$target;files=$files}|ConvertTo-Json -Depth 5|Set-Content "$stage\manifest.json" -Encoding UTF8
}
. "$stage\common.ps1"
Assert-Package $stage|Out-Null
$manifest=Get-Content "$stage\manifest.json" -Raw|ConvertFrom-Json
$hasArm64=@($manifest.files|Where-Object path -eq 'arm64\Tigirl.dll').Count -gt 0
if($hasArm64 -ne $includeArm64){throw "Payload architecture does not match selected setup architecture: $Architecture"}
if(!$includeArm64 -and ((Test-Path "$stage\arm64") -or (Test-Path "$stage\verify_arm64.exe"))){throw 'x64-only setup payload contains ARM64 files.'}
if($StageOnly){Write-Output $stage;return}
$packageId=(Get-FileHash "$stage\manifest.json").Hash.Substring(0,16).ToLowerInvariant()
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
New-Item $OutputDirectory -ItemType Directory -Force|Out-Null
& $Compiler "/DPackageDir=$stage" "/DProductVersion=$Version" "/DPackageArchitecture=$Architecture" "/DOutputPath=$OutputDirectory" "/DChineseMessages=$PSScriptRoot\packaging\setup\ChineseSimplified.isl" "$PSScriptRoot\packaging\setup\Tigirl.iss"
if($LASTEXITCODE){throw 'Installer compilation failed.'}
$exe=Join-Path $OutputDirectory "虎娘-$Version-$label-安装程序.exe"
(Get-FileHash $exe).Hash+'  '+[IO.Path]::GetFileName($exe)|Set-Content "$exe.sha256" -Encoding UTF8
@{version=$Version;architecture=$Architecture;packageId=$packageId;generationPolicy='per-install-guid';compiler='Inno Setup 6.7.3';sha256=(Get-FileHash $exe).Hash;bytes=(Get-Item $exe).Length;setupScriptSha256=(Get-FileHash "$PSScriptRoot\packaging\setup\Tigirl.iss").Hash;payloadManifestSha256=(Get-FileHash "$stage\manifest.json").Hash}|ConvertTo-Json|Set-Content "$exe.build.json" -Encoding UTF8
Write-Output $exe
