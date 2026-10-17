$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
. "$repo\tools\packaging\build_support.ps1"
$fixture=Join-Path $env:TEMP ('Tigirl build workflow '+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture|Out-Null
function Check($value,[string]$message){if(!$value){throw $message}}
function Reject([scriptblock]$action,[string]$pattern){
    $message='';try{& $action|Out-Null}catch{$message=$_.Exception.Message}
    Check ($message -match $pattern) "Expected failure '$pattern', got '$message'"
}
function Create-OutputFixture($run) {
    $run.compilerOutput=Join-Path $fixture ('compiler-'+$run.id)
    New-Item -ItemType Directory -Path $run.compilerOutput,$run.payload -Force|Out-Null
    Write-TigirlBuildJson "$($run.payload)\manifest.json" @{version=$run.version;files=@()}
    $name="虎娘-$($run.version)-x64-ARM64-x86-安装程序.exe"
    $file=Join-Path $run.compilerOutput $name
    [IO.File]::WriteAllText($file,('test fixture '+$run.id))
    $hash=(Get-FileHash -LiteralPath $file).Hash
    Write-TigirlBuildJson ($file+'.build.json') @{architecture='universal';version=$run.version;sha256=$hash;bytes=(Get-Item -LiteralPath $file).Length;
        payloadManifestSha256=(Get-FileHash -LiteralPath "$($run.payload)\manifest.json").Hash;setupScriptSha256='fixture-script-hash'}
    ($hash+'  '+$name)|Set-Content -LiteralPath ($file+'.sha256') -Encoding UTF8
    return $file
}
try {
    # Validate real version parsing, then prove a single stale resource blocks it.
    foreach($relative in @('SampleIME\SampleIME.rc','tools\SchemaManager.rc','tools\SchemaSelect.rc','tools\LexiconImport.rc','tools\Reminder.rc','packaging\setup\Maintenance.cs')){
        $target=Join-Path $fixture $relative
        New-Item -ItemType Directory -Path (Split-Path $target) -Force|Out-Null
        Copy-Item -LiteralPath (Join-Path $repo $relative) -Destination $target
    }
    $version=Get-TigirlBuildVersion $fixture
    Check ($version -eq (Get-TigirlBuildVersion $repo)) 'Automatic version discovery differs from source'
    $rc=Join-Path $fixture 'tools\Reminder.rc'
    $original=Get-Content -LiteralPath $rc -Raw -Encoding UTF8
    $original.Replace('"FileVersion", "'+$version+'"','"FileVersion", "1.2.3.4"')|Set-Content -LiteralPath $rc -Encoding UTF8
    Reject {Get-TigirlBuildVersion $fixture} 'Version mismatch'
    $original|Set-Content -LiteralPath $rc -Encoding UTF8
    Reject {Get-TigirlBuildVersion $fixture '65536.1.1.1'} 'four numbers'
    Reject {Get-TigirlBuildVersion $fixture '1.2.3.4'} 'Version mismatch'

    # Same-version runs get separate directories, without deleting previous work.
    $first=New-TigirlBuildRun $fixture $version
    $start=[DateTimeOffset]::Parse($first.started,[Globalization.CultureInfo]::InvariantCulture)
    Check ($first.id.StartsWith($start.ToString('yyyyMMdd-HHmmss',[Globalization.CultureInfo]::InvariantCulture)+'-')) 'Run ID does not match captured start time'
    Set-Content -LiteralPath "$($first.work)\keep.txt" -Value 'previous build'
    $second=New-TigirlBuildRun $fixture $version
    Check ($first.directory -ne $second.directory) 'Same-version run reused staging'
    Check ((Get-Content -LiteralPath "$($first.work)\keep.txt") -eq 'previous build') 'Previous staging was changed'
    $output=Join-Path $fixture 'artifacts with spaces'
    $report=@{status='passed';source_hashes=@{'packaging\setup\Tigirl.iss'='fixture-script-hash'}}
    $firstFile=Create-OutputFixture $first
    # Publishing can happen on a later day. Use the captured start timestamp
    # (including its local timezone), not the current date or product version.
    $first.started='2026-12-31T23:59:59.0000000+08:00'
    $published=Publish-TigirlUniversalBuild $first $output $report
    $pointer=Join-Path $output 'latest-universal.json'
    $pointerHash=(Get-FileHash -LiteralPath $pointer).Hash
    Check ((Get-FileHash -LiteralPath $published.installer).Hash -eq (Get-FileHash -LiteralPath $firstFile).Hash) 'Published installer differs'
    $expectedName="虎娘-20261231-235959-v$version-x64-ARM64-x86-安装程序.exe"
    Check ([IO.Path]::GetFileName($published.installer) -eq $expectedName) 'Published name did not use the captured local build date/time'
    Check ($published.build_started -eq $first.started) 'Latest record lost the build timestamp'
    Check ((Get-Content -LiteralPath ($published.installer+'.sha256') -Raw -Encoding UTF8).Trim() -eq ($published.sha256+'  '+$expectedName)) 'Checksum sidecar still names the old file'
    $publishedMetadata=Get-Content -LiteralPath ($published.installer+'.build.json') -Raw -Encoding UTF8|ConvertFrom-Json
    Check ($publishedMetadata.artifact_file -eq $expectedName -and $publishedMetadata.build_started -eq $first.started -and $publishedMetadata.version -eq $version) 'Published metadata does not match filename/version/time'
    Check ((Get-FileHash -LiteralPath ($published.installer+'.manifest.json')).Hash -eq (Get-FileHash -LiteralPath "$($first.payload)\manifest.json").Hash) 'Published manifest changed'
    Check (Test-Path -LiteralPath $firstFile) 'Publication removed the original compiler output'
    Reject {Publish-TigirlUniversalBuild $first $output $report} 'collision'
    Check ((Get-FileHash -LiteralPath $pointer).Hash -eq $pointerHash) 'Collision changed latest successful build'
    $secondFile=Create-OutputFixture $second
    $secondStart=$second.started;$second.started=''
    Reject {Publish-TigirlUniversalBuild $second $output $report} 'build start timestamp'
    $second.started=$secondStart
    Reject {Publish-TigirlUniversalBuild $second $output @{status='failed'}} 'Only a verified build'
    Add-Content -LiteralPath $secondFile -Value 'corruption'
    Reject {Publish-TigirlUniversalBuild $second $output $report} 'verification failed'
    Check ((Get-FileHash -LiteralPath $pointer).Hash -eq $pointerHash) 'Corrupted output replaced latest successful build'
    $secondFile=Create-OutputFixture $second
    Add-Content -LiteralPath "$($second.payload)\manifest.json" -Value 'tamper'
    Reject {Publish-TigirlUniversalBuild $second $output $report} 'verification failed'
    Check ((Get-FileHash -LiteralPath $pointer).Hash -eq $pointerHash) 'Changed payload replaced latest successful build'
    $secondFile=Create-OutputFixture $second
    $next=Publish-TigirlUniversalBuild $second $output $report
    Check ($published.installer -ne $next.installer -and (Test-Path -LiteralPath $published.installer)) 'Repeated publication overwrote previous installer'
    Check ((Get-Content -LiteralPath $pointer -Raw -Encoding UTF8|ConvertFrom-Json).installer -eq $next.installer) 'Latest pointer did not advance after successful verification'
    @{status='passed';automatic_version=$true;stale_version_rejected=$true;repeat_runs_isolated=$true;
        failed_build_keeps_previous=$true;corrupt_exe_rejected=$true;changed_payload_rejected=$true;
        verified_publication=$true;build_date_in_filename=$true;captured_local_timestamp=$true;sidecar_names_match=$true;
        paths_with_spaces_and_unicode=$true;machine_installation_tested=$false}|ConvertTo-Json
} finally {Remove-Item -LiteralPath $fixture -Recurse -Force}
