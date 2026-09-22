param([string]$SentenceModelPath = "$PSScriptRoot\data\Models\sentence-fivegram.klm")
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\sentence_package.ps1"
# This remains a release-staging entry point. Use build_native.ps1 for compile-only.
Assert-NativeTigerSentenceModel $SentenceModelPath
& "$PSScriptRoot\build_native.ps1" -Platform x64,Win32 -ToolPlatform x64 -StageRuntimeData
$destination = "$PSScriptRoot\build\x64\Release"
foreach ($tool in @('Tigirl.Import.exe', 'Tigirl.exe', 'Tigirl.Reminder.exe')) {
    Copy-Item -LiteralPath "$PSScriptRoot\build\tests\x64\$tool" -Destination $destination -Force
}
Copy-NativeTigerSentenceModel $SentenceModelPath $destination
