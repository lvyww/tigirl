$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\packaging\common.ps1"
$fixture=Join-Path $env:TEMP ('Tigirl-paths-'+[guid]::NewGuid().ToString('N'))
New-Item "$fixture\x64","$fixture\shared" -ItemType Directory -Force|Out-Null
function Check($Value,$Message){if(!$Value){throw $Message}}
try{
 @{version='2026.9.20.99';files=@(@{path='shared\Tigirl.exe'})}|ConvertTo-Json -Depth 4|Set-Content "$fixture\manifest.json" -Encoding UTF8
 Check ((Get-PackageDll $fixture 'x64') -eq "$fixture\x64\Tigirl.dll") 'Missing DLL changed registration identity'
 Check ((Get-PackageTool $fixture) -eq "$fixture\shared\Tigirl.exe") 'Shared helper path not selected'
 [IO.File]::WriteAllText("$fixture\x64\SampleIME.dll",'fixture')
 [IO.File]::WriteAllText("$fixture\x64\schema_manager.exe",'fixture')
 Check ((Get-PackageDll $fixture 'x64') -eq "$fixture\x64\Tigirl.dll") 'Legacy DLL must not be adopted'
 Check ((Get-PackageTool $fixture) -eq "$fixture\shared\Tigirl.exe") 'Legacy helper name overrode shared layout'
 [IO.File]::WriteAllText("$fixture\x64\Tigirl.dll",'fixture')
 [IO.File]::WriteAllText("$fixture\shared\Tigirl.exe",'fixture')
 Check ((Get-PackageDll $fixture 'x64') -eq "$fixture\x64\Tigirl.dll") 'Current DLL lost precedence'
 Check ((Get-PackageTool $fixture) -eq "$fixture\shared\Tigirl.exe") 'Current shared helper lost precedence'
 Remove-Item "$fixture\shared" -Recurse -Force
 Remove-Item "$fixture\manifest.json" -Force
 [IO.File]::WriteAllText("$fixture\x64\Tigirl.exe",'legacy')
 Check ((Get-PackageTool $fixture) -eq "$fixture\x64\Tigirl.exe") 'Legacy package fallback was lost'
 'PASS: architecture DLL identity, shared tool layout, legacy x64 fallback.'
}finally{Remove-Item $fixture -Recurse -Force}
