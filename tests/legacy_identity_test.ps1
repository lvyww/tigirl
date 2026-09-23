$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\packaging\legacy_identity.ps1"
# Test actual manifest/hash ownership with an isolated ProgramFiles directory.
$realProgramFiles=$env:ProgramFiles
$temp=Join-Path $env:TEMP ('tigirl-identity-'+[guid]::NewGuid().ToString('N'))
try{
 $env:ProgramFiles=$temp
 $version=Join-Path $temp 'Tigirl\versions\test'
 New-Item "$version\x64" -ItemType Directory -Force|Out-Null
 $dll="$version\x64\Tigirl.dll"
 [IO.File]::WriteAllText($dll,'test DLL bytes')
 @{files=@(@{path='x64\Tigirl.dll';sha256=(Get-FileHash $dll).Hash})}|ConvertTo-Json -Depth 4|Set-Content "$version\manifest.json"
 if(!(Test-TigirlLegacyDll $dll '')){throw 'Owned manifest was rejected'}
 [IO.File]::AppendAllText($dll,'changed')
 if(Test-TigirlLegacyDll $dll ''){throw 'Modified DLL was accepted'}
 if(Test-TigirlLegacyDll "$temp\missing.dll" ''){throw 'Missing DLL was accepted'}
}finally{
 $env:ProgramFiles=$realProgramFiles
 if(Test-Path $temp){Remove-Item -LiteralPath $temp -Recurse -Force}
}
# Exercise all ownership gates without touching registration or launching regsvr32.
$script:calls=0
function Start-Process { $script:calls++; return @{ExitCode=0} }
function Get-TigirlLegacyPaths {return $script:paths}
function Test-TigirlLegacyDll($Path,$DevelopmentRoot){return $Path -eq 'owned'}
foreach($case in @(
 @(),
 @([pscustomobject]@{view='Registry64';hive='LocalMachine';path='foreign'}),
 @([pscustomobject]@{view='Registry64';hive='LocalMachine';path='owned'},[pscustomobject]@{view='Registry32';hive='LocalMachine';path='foreign'}),
 @([pscustomobject]@{view='Registry64';hive='CurrentUser';path='owned'})
)){
 $script:paths=$case
 Remove-TigirlLegacyIdentity
 if($script:calls){throw 'Unsafe legacy unregistration attempted'}
}
# Parse every tracked PowerShell script, including generated standalone payload.
$root=Split-Path $PSScriptRoot -Parent
foreach($file in @(Get-ChildItem $root -Filter '*.ps1' -File)+@(Get-ChildItem "$root\packaging","$root\tests","$root\tools\cleanup" -Filter '*.ps1' -Recurse -File)){
 $tokens=$null;$errors=$null
 [void][Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$errors)
 if($errors){throw "$file : $errors"}
}
'PASS: legacy ownership refusal and PowerShell parsing'
