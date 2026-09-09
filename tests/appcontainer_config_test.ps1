$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $repo 'appcontainer_data.ps1')
$probe = Join-Path $repo 'build\tests\ARM64\appcontainer_config_probe.exe'
if (!(Test-Path -LiteralPath $probe)) { throw 'Build AppContainerConfigProbe.vcxproj for ARM64 first.' }
$processes = @(Get-Process QQMusic,SearchHost -ErrorAction SilentlyContinue)
if (!$processes.Count) { throw 'Open QQ Music UWP or Windows Search before running the token tests.' }
$fixture = Join-Path $repo ('build\appcontainer-config-' + [Guid]::NewGuid().ToString('N') + '\NativeTiger')
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
New-Item -ItemType File -Path (Join-Path $fixture '.appcontainer-config-test') | Out-Null
Set-NativeTigerAppContainerAccess $fixture
$checks = @()
foreach ($process in $processes) {
    $output = & $probe $process.Id $fixture
    if ($LASTEXITCODE -ne 0) { throw "Restricted configuration/menu test failed: $($process.ProcessName)" }
    $checks += @{ process = $process.ProcessName; pid = $process.Id; result = ($output | ConvertFrom-Json) }
}
@{ status = 'passed'; fixture = $fixture; checks = $checks;
   probe_hash = (Get-FileHash -LiteralPath $probe).Hash; foreground_tested = $false } |
    ConvertTo-Json -Depth 6 | Tee-Object -FilePath (Join-Path $repo 'build\uwp-config-validation.json')
