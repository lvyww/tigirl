param([switch]$PrivateBuild, [switch]$ActivationOnly, [switch]$Mixed, [switch]$AddWord, [switch]$Timer, [switch]$Schema, [switch]$Arm64X, [switch]$X64Host)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'build'
if ($Arm64X -and !$PrivateBuild) { throw '-Arm64X requires -PrivateBuild' }
if ($X64Host -and !$Arm64X) { throw '-X64Host requires -Arm64X' }
$hostArchitecture = if ($X64Host) { 'x64' } else { 'ARM64' }
$exe = Join-Path $build "tests\$hostArchitecture\tsf_host.exe"
$registrationKey = 'Registry::HKEY_CLASSES_ROOT\CLSID\{D2291A80-84D8-4641-9AB2-BDD1472C846B}\InprocServer32'
$registeredBefore = (Get-Item $registrationKey).GetValue('')
$dll = $registeredBefore
$manifest = $null
if ($ActivationOnly -and !$PrivateBuild) { throw '-ActivationOnly requires -PrivateBuild' }
if ($PrivateBuild) {
    $dll = Join-Path $build $(if ($Arm64X) { 'ARM64X\ARM64EC\Release\Tigirl.dll' } else { 'ARM64\Release\Tigirl.dll' })
    $manifest = Join-Path (Split-Path $dll -Parent) "NativeTiger.Host.$hostArchitecture.manifest"
    $manifestText = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'NativeTiger.Test.manifest') -Raw
    if ($X64Host) { $manifestText = $manifestText.Replace('processorArchitecture="arm64"', 'processorArchitecture="amd64"') }
    [IO.File]::WriteAllText($manifest, $manifestText)
}
if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) { throw 'Registered native DLL is missing' }
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'Build tests/TsfHost.vcxproj first' }
$dllHash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
$exeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
if ($Mixed -and (!$PrivateBuild -or $ActivationOnly)) { throw '-Mixed requires full private tests' }
if ($AddWord -and (!$PrivateBuild -or $ActivationOnly -or $Mixed)) { throw '-AddWord requires full private tests without -Mixed' }
if ($Timer -and (!$PrivateBuild -or $ActivationOnly -or $Mixed -or $AddWord)) { throw '-Timer requires separate full private tests' }
if ($Schema -and (!$PrivateBuild -or $Mixed -or $AddWord -or $Timer)) { throw '-Schema requires separate full private tests' }
$results = @()
$variants = if ($ActivationOnly) { @('activation') } else { @('ui-less', 'native-ui') }
foreach ($variant in $variants) {
    # Serial execution is required: each host needs actual foreground focus.
    $arguments = @($dll)
    if ($variant -eq 'native-ui') { $arguments += (Join-Path $build $(if ($Arm64X) { "native-candidate-arm64x-$hostArchitecture.bmp" } else { 'native-candidate.bmp' })) }
    elseif ($PrivateBuild) { $arguments += '-' }
    if ($PrivateBuild) { $arguments += $manifest }
    if ($ActivationOnly -and !$Schema) { $arguments += '--activation-only' }
    if ($Mixed) { $arguments += '--mixed' }
    if ($AddWord) { $arguments += '--add-word' }
    if ($Timer) { $arguments += '--timer' }
    if ($Schema) { $arguments += $(if ($ActivationOnly) { '--schema-activation-only' } else { '--schema' }) }
    $prefix = if ($PrivateBuild) { 'tsf-private' } else { 'tsf-host' }
    if ($Arm64X) { $prefix += "-arm64x-$hostArchitecture" }
    if ($Mixed) { $prefix += '-mixed' }
    if ($AddWord) { $prefix += '-add-word' }
    if ($Timer) { $prefix += '-timer' }
    if ($Schema) { $prefix += '-schema'; if ($ActivationOnly) { $prefix += '-activation' } }
    if ($Schema -and $ActivationOnly -and (Test-Path -LiteralPath (Join-Path $env:NATIVE_TIGER_USER_ROOT '.generation-tsf-test'))) { $prefix += '-generation' }
    $stdout = Join-Path $build "$prefix-$variant.json"
    $stderr = Join-Path $build "$prefix-$variant.stderr.txt"
    # Windows PowerShell wraps native stderr as ErrorRecord; capture it without
    # terminating before we can persist the actual executable exit code.
    try {
        $ErrorActionPreference = 'Continue'
        & $exe @arguments > $stdout 2> $stderr
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = 'Stop' }
    $record = [ordered]@{ variant = $variant; exit_code = $code; stdout = $stdout; stderr = $stderr }
    if ($code -eq 0) { $record.result = Get-Content -LiteralPath $stdout -Raw | ConvertFrom-Json }
    $results += $record
    [ordered]@{
        host_architecture = $hostArchitecture; arm64x = [bool]$Arm64X
        dll = $dll; dll_hash = $dllHash; test_hash = $exeHash; private_activation = [bool]$PrivateBuild
        user_root = $env:NATIVE_TIGER_USER_ROOT
        registration_before = $registeredBefore; registration_after = (Get-Item $registrationKey).GetValue('')
        timestamp_utc = [DateTime]::UtcNow.ToString('o'); results = $results
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build "$prefix-validation.json") -Encoding UTF8
    if ((Get-Item $registrationKey).GetValue('') -ne $registeredBefore) { throw 'Registration changed during test' }
    if ($code -ne 0) { throw "TSF host $variant failed (exit $code); see $stderr" }
}
if ($ActivationOnly) { Write-Host 'Private TSF activation and explicit mode callback checks passed; physical keyboard/focus and rendering were not tested.' }
else { Write-Host 'Real TSF host checks passed for UI-less and native UI, including missing-layout recovery.' }
