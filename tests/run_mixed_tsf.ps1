param()
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$isolated = Join-Path $root ('build\mixed-tsf-user-' + [Guid]::NewGuid().ToString('N'))
$previous = $env:NATIVE_TIGER_USER_ROOT
try {
    New-Item -ItemType Directory -Path $isolated | Out-Null
    @"
中英文不限长混合输入 是
最大码长 2
"@ | Set-Content -LiteralPath (Join-Path $isolated 'config.txt') -Encoding UTF8
    $env:NATIVE_TIGER_USER_ROOT = $isolated
    & (Join-Path $PSScriptRoot 'run_tsf_host.ps1') -PrivateBuild -Mixed
} finally {
    $env:NATIVE_TIGER_USER_ROOT = $previous
    if (Test-Path -LiteralPath $isolated) { Remove-Item -LiteralPath $isolated -Recurse -Force }
}
