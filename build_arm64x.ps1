param([string]$SentenceModelPath = "$PSScriptRoot\data\Models\sentence-fivegram.klm")
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'sentence_package.ps1')
Assert-NativeTigerSentenceModel $SentenceModelPath
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (!$vs) { throw 'Visual Studio with C++ ARM64/ARM64EC build tools is required.' }
& "$vs\MSBuild\Current\Bin\MSBuild.exe" "$PSScriptRoot\SampleIME\SampleIME.vcxproj" /m:4 /nr:false /p:Configuration=Release /p:Platform=ARM64EC /p:NativeArm64XBuild=true /v:minimal
if ($LASTEXITCODE -ne 0) { throw "ARM64X build failed: $LASTEXITCODE" }
$destination = Join-Path $PSScriptRoot 'build\ARM64X\ARM64EC\Release'
foreach ($tool in @('SchemaSelect', 'LexiconImport', 'SchemaManager', 'Reminder')) {
    & "$vs\MSBuild\Current\Bin\MSBuild.exe" "$PSScriptRoot\tools\$tool.vcxproj" /nr:false /p:Configuration=Release /p:Platform=ARM64 /v:minimal
    if ($LASTEXITCODE -ne 0) { throw "ARM64X companion build failed: $tool" }
}
foreach ($tool in @('Tigirl.SchemaSelect.exe', 'Tigirl.Import.exe', 'Tigirl.exe', 'Tigirl.Reminder.exe')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "build\tests\ARM64\$tool") -Destination (Join-Path $destination $tool) -Force
}
foreach ($platform in @('ARM64', 'x64')) {
    & "$vs\MSBuild\Current\Bin\MSBuild.exe" "$PSScriptRoot\tests\ArchitectureLoadProbe.vcxproj" /nr:false /p:Configuration=Release "/p:Platform=$platform" /v:minimal
    if ($LASTEXITCODE -ne 0) { throw "ARM64X loader verifier build failed: $platform" }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "build\tests\$platform\architecture_load_probe.exe") -Destination (Join-Path $destination "verify_load_$($platform.ToLowerInvariant()).exe") -Force
}
Copy-NativeTigerSentenceModel $SentenceModelPath $destination
Write-Output "Experimental ARM64X DLL: $PSScriptRoot\build\ARM64X\ARM64EC\Release\Tigirl.dll"
