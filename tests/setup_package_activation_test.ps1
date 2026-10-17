param([Parameter(Mandatory)][string]$Package)
$ErrorActionPreference='Stop'
. "$Package\common.ps1"
$fixture=Join-Path $env:TEMP ('Tigirl-package-activation-'+[guid]::NewGuid().ToString('N'))
New-Item $fixture -ItemType Directory|Out-Null
$saved=$env:NATIVE_TIGER_USER_ROOT
$before=@((Get-ComPath 'Registry64'),(Get-ComPath 'Registry32'),(Test-Path "$env:LOCALAPPDATA\Tigirl"),(Test-Path "$env:ProgramFiles\Tigirl"),(Get-WinUserLanguageList|ConvertTo-Json -Depth 5 -Compress)) -join '|'
$checks=@()
try {
 foreach($check in (Get-PackageLoadChecks $Package)) {
  $platform=switch($check.probe){'x86'{'Win32'} 'arm64'{'ARM64'} default{'x64'}}
  $label=$check.probe+'-'+$check.dll
  $env:NATIVE_TIGER_USER_ROOT=Join-Path $fixture ($label+'\Tigirl')
  New-Item $env:NATIVE_TIGER_USER_ROOT -ItemType Directory -Force|Out-Null
  "最大码长`t4"|Set-Content "$env:NATIVE_TIGER_USER_ROOT\config.txt" -Encoding UTF8
  $exe=Join-Path $PSScriptRoot "..\build\tests\$platform\package_activation_probe.exe"
  $dll=Get-PackageDll $Package $check.dll
  $process=[Diagnostics.Process]::new()
  try {
   $process.StartInfo.FileName=$exe
   $process.StartInfo.Arguments='"'+$dll+'"'
   $process.StartInfo.UseShellExecute=$false
   $process.StartInfo.CreateNoWindow=$true
   $process.StartInfo.RedirectStandardOutput=$true
   $process.StartInfo.RedirectStandardError=$true
   if(!$process.Start()){throw "Cannot start resource probe: $label"}
   if(!$process.WaitForExit(30000)){$process.Kill();$process.WaitForExit();throw "Resource startup timeout: $label"}
   $output=$process.StandardOutput.ReadToEnd();$errors=$process.StandardError.ReadToEnd()
   if($process.ExitCode -ne 0){throw "Resource startup failed: $label / code=$($process.ExitCode) / $errors / $output"}
   $result=$output|ConvertFrom-Json
  } finally {$process.Dispose()}
  if(!$result.resources_opened -or !$result.deactivated){throw "Activation result missing: $label"}
  $checks+=@{host=$check.probe;dll=$check.dll;result=$result}
 }
 $after=@((Get-ComPath 'Registry64'),(Get-ComPath 'Registry32'),(Test-Path "$env:LOCALAPPDATA\Tigirl"),(Test-Path "$env:ProgramFiles\Tigirl"),(Get-WinUserLanguageList|ConvertTo-Json -Depth 5 -Compress)) -join '|'
 if($before -ne $after){throw 'Package activation changed live installation state.'}
 @{status='passed';checks=$checks;live_installation_unchanged=$true;registered_input_tested=$false}|ConvertTo-Json -Depth 6
} finally {$env:NATIVE_TIGER_USER_ROOT=$saved;Remove-Item -LiteralPath $fixture -Recurse -Force}
