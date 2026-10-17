#requires -Version 5.1
<#
Repeatable x64 + ARM64X + x86 installer build. Builds and validates only;
never installs, registers the IME, or changes the real user data directory.
Double-click build_setup_universal.vbs for a hidden background run.
#>
[CmdletBinding()]
param(
    [string]$Version='',
    [string]$Compiler='',
    [string]$OutputDirectory='',
    [switch]$SkipBuild,
    [switch]$CheckOnly
)
$ErrorActionPreference='Stop'
if(!$Compiler){$Compiler=Join-Path $PSScriptRoot 'build\tools\InnoSetup-6.7.3\ISCC.exe'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $PSScriptRoot 'build\packages'}
. "$PSScriptRoot\tools\packaging\build_support.ps1"
$repo=$PSScriptRoot
$run=New-TigirlBuildRun $repo 'pending'
$state=[ordered]@{state='starting';step='starting';pid=$PID;run=$run.id;version=$Version;
    started=$run.started;updated=(Get-Date -Format o);logs=$run.logs;error=$null;installer=$null}
$steps=[Collections.Generic.List[string]]::new()
function Set-BuildState([string]$Status,[string]$Step) {
    $state.state=$Status;$state.step=$Step;$state.updated=Get-Date -Format o
    Write-TigirlBuildJson "$($run.directory)\status.json" $state
    Add-Content -LiteralPath "$($run.directory)\progress.log" -Value ("$($state.updated) [$Status] $Step") -Encoding UTF8
}
function Invoke-BuildStep([string]$Name,[scriptblock]$Action) {
    Set-BuildState 'running' $Name
    Write-Output "[$Name] $($run.logs)\$Name.log"
    & $Action *> "$($run.logs)\$Name.log"
    $steps.Add($Name)
}
# Native outputs are shared by MSBuild across invocations; serialize this entry
# point per checkout. Every payload, log directory and release is otherwise fresh.
$sha=[Security.Cryptography.SHA256]::Create()
try {$key=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($repo.ToLowerInvariant()))).Replace('-','')}
finally {$sha.Dispose()}
$mutex=[Threading.Mutex]::new($false,('Local\Tigirl.UniversalBuild.'+$key))
$owned=$false
try {
    try {$owned=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$owned=$true}
    if(!$owned){throw 'A universal build is already running for this checkout; see build\setup-runs\latest-universal.json.'}
    Write-TigirlBuildJson "$repo\build\setup-runs\latest-universal.json" @{run=$run.id;status_file="$($run.directory)\status.json";progress_log="$($run.directory)\progress.log";logs=$run.logs}
    Invoke-BuildStep '01-prerequisites' {
        # Read the version from sources; never relabel stale binaries or silently
        # rewrite version resources in a dirty working tree.
        $script:releaseVersion=Get-TigirlBuildVersion $repo $Version
        Assert-TigirlBuildPrerequisites $repo $Compiler -SkipBuild:$SkipBuild
    }
    $run.version=$releaseVersion
    $run.payload=Join-Path $run.work "Tigirl-$releaseVersion-universal-setup-payload"
    $state.version=$releaseVersion
    if($CheckOnly){Set-BuildState 'passed' 'prerequisites-only';Write-Output 'Prerequisites passed; no build or installation performed.';return}
    $sourceHashes=@{}
    foreach($relative in @('build_setup_universal.ps1','build_setup.ps1','package_x64.ps1','build_x64.ps1','build_arm64x.ps1','build_native.ps1','package_sources.ps1','sentence_package.ps1','tools\packaging\build_support.ps1','packaging\setup\Tigirl.iss')){
        $sourceHashes[$relative]=(Get-FileHash -LiteralPath (Join-Path $repo $relative)).Hash
    }
    Invoke-BuildStep '02-build-and-stage' {
        & "$repo\build_setup.ps1" -Version $releaseVersion -Architecture universal -Compiler $Compiler -WorkDirectory $run.work -StageOnly -SkipBuild:$SkipBuild
    }
    Invoke-BuildStep '03-payload-validation' {
        $script:payloadReport=Assert-TigirlUniversalPayload $repo $run.payload $releaseVersion
        Write-TigirlBuildJson "$($run.directory)\payload-validation.json" $payloadReport
        $payloadReport|ConvertTo-Json -Depth 6
    }
    Invoke-BuildStep '04-compiler-regression' {
        & "$repo\tests\setup_compiler_smoke_test.ps1" -Compiler $Compiler
    }
    Invoke-BuildStep '05-architecture-regression' {
        & "$repo\tests\setup_architecture_test.ps1"
        & "$repo\tests\setup_architecture_guard_test.ps1" -Compiler $Compiler -Architecture universal
    }
    Invoke-BuildStep '06-first-user-data' {
        & "$repo\tests\setup_first_user_data_test.ps1" -Package $run.payload
    }
    Invoke-BuildStep '07-finished-page' {
        & "$repo\tests\setup_finished_page_test.ps1" -Compiler $Compiler -Output "$($run.directory)\finished-page"
    }
    Invoke-BuildStep '08-compile-installer' {
        # Use Windows TEMP, not the watched/synced deliverables folder. Reuse
        # only THIS run's validated payload, never an older same-version stage.
        & "$repo\build_setup.ps1" -Version $releaseVersion -Architecture universal -Compiler $Compiler -WorkDirectory $run.work -RecompileOnly -OutputDirectory $run.compilerOutput
    }
    foreach($relative in $sourceHashes.Keys){
        if((Get-FileHash -LiteralPath (Join-Path $repo $relative)).Hash -ne $sourceHashes[$relative]){throw "Build input changed during this run: $relative. Run the build again."}
    }
    $report=[ordered]@{status='passed';version=$releaseVersion;run=$run.id;started=$state.started;validated=(Get-Date -Format o);
        compiler='Inno Setup 6.7.3';binary_build_skipped=[bool]$SkipBuild;steps=@($steps.ToArray());payload=$payloadReport;
        source_hashes=$sourceHashes;machine_installation_tested=$false;live_user_data_modified=$false;
        log_directory=$run.logs}
    Write-TigirlBuildJson "$($run.directory)\verification.json" $report
    Invoke-BuildStep '09-publish-artifacts' {
        $script:result=Publish-TigirlUniversalBuild $run ([IO.Path]::GetFullPath($OutputDirectory)) $report
        $result|ConvertTo-Json -Depth 5
    }
    $state.installer=$result.installer
    Set-BuildState 'passed' 'complete'
    Write-Output $result.installer
    Write-Output ("SHA256: "+$result.sha256)
} catch {
    $state.error=($_|Out-String)
    try {Set-BuildState 'failed' $state.step;$state.error|Set-Content -LiteralPath "$($run.directory)\error.log" -Encoding UTF8}catch {}
    Write-Error ($state.error+"`r`nBuild logs: "+$run.directory) -ErrorAction Continue
    exit 1
} finally {
    if($owned){$mutex.ReleaseMutex()}
    $mutex.Dispose()
}
