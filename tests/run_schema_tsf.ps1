param([switch]$ActivationOnly, [switch]$Generation)
if ($Generation -and !$ActivationOnly) { throw "-Generation currently requires -ActivationOnly" }
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$isolated = Join-Path $root ('build\schema-tsf-user-' + [Guid]::NewGuid().ToString('N'))
$previous = $env:NATIVE_TIGER_USER_ROOT
try {
    $schema = Join-Path $isolated 'schemas\SchemaTest'
    New-Item -ItemType Directory -Path $schema -Force | Out-Null
    New-Item -ItemType File -Path (Join-Path $isolated '.schema-tsf-test') | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $isolated 'user') | Out-Null
    $dictionary = Join-Path $root 'build\ARM64\Release\tiger-v2.tcd'
    Copy-Item -LiteralPath $dictionary -Destination (Join-Path $schema 'tiger-v2.tcd')
    $probe = Join-Path $root 'build\tests\ARM64\user_store_probe.exe'
    & $probe $dictionary (Join-Path $isolated '码表\虎码字词\用户调整.txt') write qqqqqq '内置' 1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot seed built-in schema journal' }
    & $probe (Join-Path $schema 'tiger-v2.tcd') (Join-Path $isolated '码表\SchemaTest\用户调整.txt') write qqqqqq '方案' 1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot seed alternate schema journal' }
    if ($Generation) {
        $generationId = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
        $generationDirectory = Join-Path $schema ('generations\' + $generationId)
        New-Item -ItemType Directory -Path $generationDirectory -Force | Out-Null
        Copy-Item -LiteralPath $dictionary -Destination (Join-Path $generationDirectory 'tiger-v2.tcd')
        "generation`t$generationId" | Set-Content -LiteralPath (Join-Path $schema 'current.txt') -Encoding UTF8
        New-Item -ItemType File -Path (Join-Path $isolated '.generation-tsf-test') | Out-Null
    }
    if ($ActivationOnly) { '当前码表 SchemaTest' | Set-Content -LiteralPath (Join-Path $isolated 'config.txt') -Encoding UTF8 }
    $env:NATIVE_TIGER_USER_ROOT = $isolated
    & (Join-Path $PSScriptRoot 'run_tsf_host.ps1') -PrivateBuild -Schema -ActivationOnly:$ActivationOnly
} finally {
    $env:NATIVE_TIGER_USER_ROOT = $previous
    if (Test-Path -LiteralPath $isolated) { Remove-Item -LiteralPath $isolated -Recurse -Force }
    $displaced = "$isolated.displaced"
    if (Test-Path -LiteralPath (Join-Path $displaced '.schema-tsf-test')) {
        Remove-Item -LiteralPath $displaced -Recurse -Force
    }
}
