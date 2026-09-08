$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (!$vs) { throw 'Visual Studio with C++ ARM64 build tools is required.' }
& "$vs\MSBuild\Current\Bin\MSBuild.exe" "$PSScriptRoot\SampleIME.sln" /m:4 /nr:false /p:Configuration=Release /p:Platform=ARM64 /v:minimal
if ($LASTEXITCODE -ne 0) { throw "ARM64 build failed: $LASTEXITCODE" }
foreach ($tool in @('SchemaSelect', 'LexiconImport', 'SchemaManager', 'Reminder')) {
    & "$vs\MSBuild\Current\Bin\MSBuild.exe" "$PSScriptRoot\tools\$tool.vcxproj" /nr:false /p:Configuration=Release /p:Platform=ARM64 /v:minimal
    if ($LASTEXITCODE -ne 0) { throw "ARM64 tool build failed: $tool" }
}
foreach ($tool in @('schema_select.exe', 'lexicon_import.exe', 'schema_manager.exe', 'timer_reminder.exe')) {
    $toolSource = Join-Path $PSScriptRoot "build\tests\ARM64\$tool"
    $toolDestination = Join-Path $PSScriptRoot "build\ARM64\Release\$tool"
    if (!(Test-Path -LiteralPath $toolDestination) -or (Get-FileHash -LiteralPath $toolSource).Hash -ne (Get-FileHash -LiteralPath $toolDestination).Hash) {
        Copy-Item -LiteralPath $toolSource -Destination $toolDestination -Force
    }
}
