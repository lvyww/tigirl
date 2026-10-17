$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\packaging\common.ps1"
$fixture=Join-Path $env:TEMP ('Tigirl-architecture-'+[guid]::NewGuid().ToString('N'))
New-Item "$fixture\x64","$fixture\x86","$fixture\arm64" -ItemType Directory -Force|Out-Null
function Check($Value,$Message){if(!$Value){throw $Message}}
function Get-SystemArchitecture { return $script:testArchitecture }
function Get-ComPath($View){return $script:registry[$View]}
# Inspect the actual registration command plan, without launching regsvr32.
function Start-Process($FilePath,$ArgumentList,[switch]$Wait,[switch]$PassThru){
 $script:commands+=@{exe=$FilePath;arguments=$ArgumentList}
 $view=if($FilePath -like '*\SysWOW64\*'){'Registry32'}else{'Registry64'}
 $script:registry[$view]=$ArgumentList[-1].Trim('"')
 return [pscustomobject]@{ExitCode=0}
}
try {
 @{version='2026.10.15.1';files=@(@{path='x64\Tigirl.dll'},@{path='x86\Tigirl.dll'},@{path='arm64\Tigirl.dll'})}|ConvertTo-Json -Depth 4|Set-Content "$fixture\manifest.json" -Encoding UTF8
 foreach($arch in @('X64','Arm64')) {
  $script:testArchitecture=$arch
  $expected=if($arch -eq 'Arm64'){'arm64'}else{'x64'}
  Check ((Get-PackageDll $fixture 'native') -eq "$fixture\$expected\Tigirl.dll") "Wrong native identity on $arch"
  Check ((Get-PackageDll $fixture 'x64') -eq "$fixture\x64\Tigirl.dll") 'Explicit x64 loader path was redirected'
  Assert-InstallArchitecture $fixture
  $checks=@(Get-PackageLoadChecks $fixture | ForEach-Object {$_.probe+'->'+$_.dll})
  Check ($checks -contains 'x64->x64' -and $checks -contains 'x86->x86') 'Missing Intel architecture load checks'
  if($arch -eq 'Arm64') {Check ($checks.Count -eq 4 -and $checks -contains 'arm64->arm64' -and $checks -contains 'x64->arm64') 'ARM64X dual-view verification missing'}
  else {Check ($checks.Count -eq 2) 'Intel host attempts to load ARM64 binary'}
  $script:registry=@{};$script:commands=@()
  Invoke-Registration $fixture
  Check ($script:commands.Count -eq 2) 'Unexpected registration count'
  Check ($script:commands[0].exe -eq "$env:windir\System32\regsvr32.exe") 'Native registration uses wrong system directory'
  Check ($script:registry.Registry64 -eq "$fixture\$expected\Tigirl.dll") 'Native COM registration selected wrong binary'
  Check ($script:registry.Registry32 -eq "$fixture\x86\Tigirl.dll") 'x86 COM registration selected wrong binary'
 }
 # Missing files retain the manifest identity (for interrupted install repair).
 Check (!(Test-Path "$fixture\arm64\Tigirl.dll") -and (Get-PackageDll $fixture 'native') -eq "$fixture\arm64\Tigirl.dll") 'Missing ARM64 DLL silently fell back to x64'
 @{version='2026.10.15.1';files=@(@{path='x64\Tigirl.dll'},@{path='x86\Tigirl.dll'})}|ConvertTo-Json -Depth 4|Set-Content "$fixture\manifest.json" -Encoding UTF8
 $blocked=$false;try{Assert-InstallArchitecture $fixture}catch{$blocked=$true}
 Check $blocked 'ARM64 host accepted a payload without ARM64X'
 $script:testArchitecture='X64';Assert-InstallArchitecture $fixture
 'PASS: x64/ARM64 native selection, both ARM64X loader views, missing-file identity, architecture-specific registration commands, old x64 payload compatibility.'
} finally {Remove-Item -LiteralPath $fixture -Recurse -Force}
