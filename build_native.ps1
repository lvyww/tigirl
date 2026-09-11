# Compile only: no model/word-list lookup, packaging, registration or installation.
[CmdletBinding()]
param(
    [ValidateSet('x64', 'Win32', 'ARM64')]
    [string[]]$Platform = @('x64', 'Win32'),
    [ValidateSet('x64', 'Win32', 'ARM64')]
    [string[]]$ToolPlatform,
    [ValidatePattern('^v[0-9]+$')]
    [string]$PlatformToolset,
    [switch]$StageRuntimeData
)
$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio C++ build tools are required.' }
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ build tools are required.' }
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
$common = @('/m:4', '/nr:false', '/p:Configuration=Release', '/v:minimal',
    "/p:NativeCompileOnly=$(-not $StageRuntimeData)")
if ($PlatformToolset) { $common += "/p:PlatformToolset=$PlatformToolset" }
function Build-Project([string]$Project, [string]$TargetPlatform) {
    & $msbuild (Join-Path $PSScriptRoot $Project) @common "/p:Platform=$TargetPlatform"
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $Project ($TargetPlatform), exit $LASTEXITCODE" }
}
foreach ($target in $Platform) {
    Build-Project 'SampleIME\SampleIME.vcxproj' $target
    Build-Project 'tests\ArchitectureLoadProbe.vcxproj' $target
}
if (!$ToolPlatform) { $ToolPlatform = $Platform }
foreach ($target in $ToolPlatform) {
    foreach ($tool in @('LexiconImport', 'SchemaManager', 'SchemaSelect', 'Reminder')) {
        Build-Project "tools\$tool.vcxproj" $target
    }
}
