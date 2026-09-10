$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\packaging\common.ps1"
$fixture=Join-Path $env:TEMP ('Tigirl-paths-'+[guid]::NewGuid().ToString('N'))
New-Item "$fixture\x64" -ItemType Directory -Force|Out-Null
function Check($Value,$Message){if(!$Value){throw $Message}}
try{
 Check ((Get-PackageDll $fixture 'x64') -eq "$fixture\x64\Tigirl.dll") 'Missing DLL changed registration identity'
 Check ((Get-PackageTool $fixture) -eq "$fixture\x64\Tigirl.exe") 'Missing helper changed URI identity'
 [IO.File]::WriteAllText("$fixture\x64\SampleIME.dll",'fixture')
 [IO.File]::WriteAllText("$fixture\x64\schema_manager.exe",'fixture')
 Check ((Get-PackageDll $fixture 'x64') -eq "$fixture\x64\Tigirl.dll") 'Legacy DLL must not be adopted'
 Check ((Get-PackageTool $fixture) -eq "$fixture\x64\Tigirl.exe") 'Legacy helper must not be adopted'
 [IO.File]::WriteAllText("$fixture\x64\Tigirl.dll",'fixture')
 [IO.File]::WriteAllText("$fixture\x64\Tigirl.exe",'fixture')
 Check ((Get-PackageDll $fixture 'x64') -eq "$fixture\x64\Tigirl.dll") 'Current DLL lost precedence'
 Check ((Get-PackageTool $fixture) -eq "$fixture\x64\Tigirl.exe") 'Current helper lost precedence'
 'PASS: missing-file repair identity, legacy rejection, current filename precedence.'
}finally{Remove-Item $fixture -Recurse -Force}
