param([string]$SentenceModelPath = "$PSScriptRoot\..\bime_codex_src_20260513\release_arm64\Models\sentence-ngram-v2.bin")
$ErrorActionPreference='Stop'
. "$PSScriptRoot\sentence_package.ps1"
Assert-NativeTigerSentenceModel $SentenceModelPath
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (!$vs) { throw 'Visual Studio C++ build tools are required.' }
foreach ($platform in @('x64','Win32')) {
    & "$vs\MSBuild\Current\Bin\MSBuild.exe" "$PSScriptRoot\SampleIME\SampleIME.vcxproj" /m:4 /nr:false /p:Configuration=Release "/p:Platform=$platform" /v:minimal
    if ($LASTEXITCODE) { throw "DLL build failed: $platform" }
    & "$vs\MSBuild\Current\Bin\MSBuild.exe" "$PSScriptRoot\tests\ArchitectureLoadProbe.vcxproj" /nr:false /p:Configuration=Release "/p:Platform=$platform" /v:minimal
    if ($LASTEXITCODE) { throw "Verifier build failed: $platform" }
}
foreach ($tool in @('LexiconImport','SchemaManager','Reminder')) {
    & "$vs\MSBuild\Current\Bin\MSBuild.exe" "$PSScriptRoot\tools\$tool.vcxproj" /nr:false /p:Configuration=Release /p:Platform=x64 /v:minimal
    if ($LASTEXITCODE) { throw "Tool build failed: $tool" }
}
$destination="$PSScriptRoot\build\x64\Release"
foreach ($tool in @('Tigirl.Import.exe','Tigirl.exe','Tigirl.Reminder.exe')) {
    Copy-Item -LiteralPath "$PSScriptRoot\build\tests\x64\$tool" -Destination $destination -Force
}
Copy-NativeTigerSentenceModel $SentenceModelPath $destination
