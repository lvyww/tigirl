$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$installer = Join-Path $root 'install_arm64.ps1'
# Import only the validation function, never the installer's registration body.
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($installer, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Installer parse failed.' }
$function = $ast.Find({ param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
    $node.Name -eq 'Test-NativeTigerDeployment'
}, $false)
if (!$function) { throw 'Deployment validator missing.' }
. ([scriptblock]::Create($function.Extent.Text))
$directory = Join-Path $root 'build\ARM64X\ARM64EC\Release'
$hashes = [ordered]@{}
foreach ($file in @('Tigirl.dll', 'verify_load_arm64.exe', 'verify_load_x64.exe')) {
    $hashes[$file] = (Get-FileHash -LiteralPath (Join-Path $directory $file)).Hash
}
$valid = Test-NativeTigerDeployment $directory $hashes -Arm64X
if (!$valid.arm64.class_instance -or !$valid.x64.class_instance) { throw 'Valid package rejected.' }
$hashes['Tigirl.dll'] = '0' * 64
$rejected = $false
try { Test-NativeTigerDeployment $directory $hashes -Arm64X | Out-Null }
catch {
    if ($_.Exception.Message -notlike '*Deployment hash mismatch: Tigirl.dll*') { throw }
    $rejected = $true
}
if (!$rejected) { throw 'Mismatched deployment bytes accepted.' }
@{ status='passed'; valid_dual_load=$true; hash_mismatch_rejected=$true;
   registration_executed=$false; installed_directory_tested=$false;
   installer_sha256=(Get-FileHash -LiteralPath $installer).Hash } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\deployment-hash-validation.json') -Encoding UTF8
Write-Output 'Deployment validator: dual load and hash mismatch checks passed without registration.'
