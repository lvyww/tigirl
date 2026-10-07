param(
    [ValidateSet('ARM64','x64','Win32')][string]$Platform='ARM64',
    [string]$SourceOverride='',
    [string]$OutputDirectory='',
    [switch]$ExpectFailure
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $root "build\mode-unset-compartments-$Platform"}
$out=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $out -Force | Out-Null
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
if(!$vs){throw 'Visual Studio unavailable'}
$msbuild=Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
$source=Join-Path $root 'native\tsf\ModeCompartments.cpp'
$params=@('/m:2','/nr:false','/p:Configuration=Release',"/p:Platform=$Platform","/p:ModeUnsetOutputRoot=$out",'/v:minimal','/nologo')
if($SourceOverride){$source=[IO.Path]::GetFullPath($SourceOverride);$params+=("/p:ModeCompartmentsSource=$source")}
& $msbuild (Join-Path $PSScriptRoot 'ModeUnsetCompartmentProbe.vcxproj') @params *> (Join-Path $out 'build.log')
if($LASTEXITCODE -ne 0){Get-Content -LiteralPath (Join-Path $out 'build.log') -Tail 35;throw 'Mode unset probe build failed'}
$exe=Join-Path $out 'bin\mode_unset_compartment_probe.exe'
$stdout=Join-Path $out 'stdout.jsonl';$stderr=Join-Path $out 'stderr.txt'
$process=Start-Process -FilePath $exe -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdout -RedirectStandardError $stderr
$records=@(Get-Content -LiteralPath $stdout -Encoding UTF8 | Where-Object {$_} | ForEach-Object {$_ | ConvertFrom-Json})
$passed=@($records | Where-Object {$_.passed}).Count
$complete=$records.Count -eq 12
$expected=if($ExpectFailure){$complete -and $process.ExitCode -eq 1 -and $passed -lt 12}else{$complete -and $process.ExitCode -eq 0 -and $passed -eq 12}
$report=[ordered]@{
 status=$(if($expected){'passed'}else{'failed'});platform=$Platform;exit_code=$process.ExitCode;expected_negative_control=[bool]$ExpectFailure
 case_count=$records.Count;passed_count=$passed;source=$source;source_sha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
 probe_source_sha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'mode_unset_compartment_probe.cpp') -Algorithm SHA256).Hash.ToLowerInvariant()
 executable_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
 real_tsf_backing_store=$true;unset_notification_injected=$true;callback_write_failure_injected=$true;real_foreground_activation_tested=$false
 checks=$records
}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out 'result.json') -Encoding UTF8
Write-Output ($report | ConvertTo-Json -Depth 6 -Compress)
if(!$expected){Get-Content -LiteralPath $stderr;throw 'Mode unset probe assertions failed'}
