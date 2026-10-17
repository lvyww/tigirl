# Helpers for the repeatable release entry point. No registration or installation.
function Write-TigirlBuildJson([string]$Path,$Value) {
    $folder=Split-Path $Path -Parent
    New-Item -ItemType Directory -Path $folder -Force|Out-Null
    # A dated installer sidecar already has a long filename. Use a short sibling
    # temp name so atomic writes don't exceed Windows PowerShell's path limit.
    $temporary=Join-Path $folder ('.tigirl-'+[guid]::NewGuid().ToString('N')+'.tmp')
    try {
        $Value|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $temporary -Encoding UTF8
        if(Test-Path -LiteralPath $Path){[IO.File]::Replace($temporary,$Path,[NullString]::Value)}
        else {[IO.File]::Move($temporary,$Path)}
    } finally {if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary}}
}

function Get-TigirlBuildVersion([string]$Repo,[string]$RequestedVersion='') {
    $resources=@('SampleIME\SampleIME.rc','tools\SchemaManager.rc','tools\SchemaSelect.rc','tools\LexiconImport.rc','tools\Reminder.rc')
    $version=$RequestedVersion
    foreach($relative in $resources){
        $text=Get-Content -LiteralPath (Join-Path $Repo $relative) -Raw -Encoding UTF8
        $file=[regex]::Match($text,'(?m)^\s*FILEVERSION\s+(\d+),(\d+),(\d+),(\d+)\s*$')
        if(!$file.Success){throw "Missing FILEVERSION: $relative"}
        $actual=($file.Groups[1..4].Value -join '.')
        if(!$version){$version=$actual}
        if($version -notmatch '^\d+\.\d+\.\d+\.\d+$' -or @($version.Split('.')|Where-Object {[long]$_ -gt 65535}).Count){throw 'Version must be four numbers between 0 and 65535.'}
        $product=[regex]::Match($text,'(?m)^\s*PRODUCTVERSION\s+(\d+),(\d+),(\d+),(\d+)\s*$')
        $strings=[regex]::Matches($text,'VALUE\s+"(?:FileVersion|ProductVersion)"\s*,\s*"([^"]+)"')
        if($actual -ne $version -or !$product.Success -or ($product.Groups[1..4].Value -join '.') -ne $version -or
           $strings.Count -ne 2 -or @($strings|Where-Object {$_.Groups[1].Value -ne $version}).Count){
            throw "Version mismatch in $relative; expected $version. Update the resource version before packaging."
        }
    }
    $maintenance=Get-Content -LiteralPath "$Repo\packaging\setup\Maintenance.cs" -Raw -Encoding UTF8
    if([regex]::Match($maintenance,'AssemblyVersion\("([^"]+)"\)').Groups[1].Value -ne $version){
        throw "Version mismatch in packaging\setup\Maintenance.cs; expected $version."
    }
    return $version
}

function Assert-TigirlBuildPrerequisites([string]$Repo,[string]$Compiler,[switch]$SkipBuild) {
    if(![Environment]::Is64BitProcess){throw 'Use 64-bit Windows PowerShell 5.1.'}
    $pins=@{'ISCC.exe'='0A8757031B33777E4C9CBFFEE40F11A5062B36D25CBE144C1DB73B6102B80AD7';'ISCmplr.dll'='85A1E3090D3A5B85319F001B7C8F9ECFAD45F37EFF030A67BBE29EF58B7AA2C3'}
    foreach($name in $pins.Keys){
        $file=if($name -eq 'ISCC.exe'){$Compiler}else{Join-Path (Split-Path $Compiler) $name}
        if(!(Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file).Hash -ne $pins[$name]){throw "Install pinned Inno Setup 6.7.3 or pass -Compiler <ISCC.exe>: $file"}
    }
    if(!(Test-Path -LiteralPath "$env:windir\Microsoft.NET\Framework64\v4.0.30319\csc.exe")){throw '.NET Framework 64-bit C# compiler is required.'}
    if(!$SkipBuild){
        $vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if(!(Test-Path -LiteralPath $vswhere)){throw 'Visual Studio C++ build tools are required.'}
        $vs=& $vswhere -latest -products * -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if(!$vs -or !(Test-Path -LiteralPath "$vs\MSBuild\Current\Bin\MSBuild.exe")){throw 'Visual Studio MSBuild and x64/x86 C++ tools are required.'}
        $toolset=Get-ChildItem -LiteralPath "$vs\VC\Tools\MSVC" -Directory|Sort-Object Name -Descending|Select-Object -First 1
        foreach($arch in @('x64','x86','arm64')){
            if(!$toolset -or !(Test-Path -LiteralPath (Join-Path $toolset.FullName "lib\$arch\libcmt.lib"))){throw "Install Visual Studio C++ $arch build tools (project toolset v145)."}
        }
        # ARM64EC uses the ARM64 compiler and hybrid CRT libraries; its own
        # directory contains support objects, not a separate libcmt.lib.
        if(!(Test-Path -LiteralPath (Join-Path $toolset.FullName 'lib\arm64ec\chkstk_arm64ec.obj'))){throw 'Install Visual Studio C++ ARM64EC build tools (project toolset v145).'}
    }
    . "$Repo\sentence_package.ps1"
    Assert-NativeTigerSentenceModel "$Repo\data\Models\sentence-fivegram-mobile.bin"
    Assert-NativeTigerSentenceLexicalPrior "$Repo\resources\sentence-lexical-v1.bin"
    . "$Repo\package_sources.ps1"
    $sources=Get-NativeTigerPackageSources "$Repo\resources\DefaultData" "$Repo\resources\Skins"
    if(!$sources.Skins.Count){throw 'No SSF skins found in resources\Skins.'}
}

function New-TigirlBuildRun([string]$Repo,[string]$Version) {
    # Capture the local start time once. A long compression step may cross
    # midnight; the run directory, published name and metadata must still agree.
    $started=[DateTimeOffset]::Now
    $stamp=$started.ToString('yyyyMMdd-HHmmss',[Globalization.CultureInfo]::InvariantCulture)
    $id=$stamp+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)
    $directory=Join-Path $Repo "build\setup-runs\$id"
    New-Item -ItemType Directory -Path "$directory\logs","$directory\work"|Out-Null
    [pscustomobject]@{id=$id;version=$Version;started=$started.ToString('o',[Globalization.CultureInfo]::InvariantCulture);
        directory=$directory;logs="$directory\logs";work="$directory\work";
        payload="$directory\work\Tigirl-$Version-universal-setup-payload";
        compilerOutput=(Join-Path ([IO.Path]::GetTempPath()) "Tigirl-setup-$id")}
}

function Assert-TigirlUniversalPayload([string]$Repo,[string]$Package,[string]$Version) {
    . "$Package\common.ps1"
    $manifest=Assert-Package $Package
    if($manifest.version -ne $Version -or $manifest.target -ne 'Windows x64/ARM64, x64+ARM64X+x86 TSF'){throw 'Incorrect universal payload version/target.'}
    $listed=@{};foreach($entry in $manifest.files){$listed[$entry.path]=$entry}
    $machines=@{'x64\Tigirl.dll'=0x8664;'x86\Tigirl.dll'=0x14c;'arm64\Tigirl.dll'=0xaa64;
        'verify_x64.exe'=0x8664;'verify_x86.exe'=0x14c;'verify_arm64.exe'=0xaa64}
    $versions=@()
    foreach($relative in @('x64\Tigirl.dll','x86\Tigirl.dll','arm64\Tigirl.dll','shared\Tigirl.exe','shared\Tigirl.Import.exe','shared\Tigirl.Reminder.exe','setup\Tigirl.Maintenance.exe')){
        if(!$listed.ContainsKey($relative)){throw "Missing manifest entry: $relative"}
        $actual=[Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $Package $relative)).FileVersion
        if($actual -ne $Version){throw "Stale binary version: $relative ($actual), expected $Version"}
        $versions+=@{file=$relative;version=$actual}
    }
    foreach($relative in $machines.Keys){
        if(!$listed.ContainsKey($relative)){throw "Missing architecture artifact: $relative"}
        $bytes=[IO.File]::ReadAllBytes((Join-Path $Package $relative))
        if($bytes.Length -lt 64 -or [BitConverter]::ToUInt16($bytes,0) -ne 0x5a4d){throw "Invalid PE: $relative"}
        $pe=[BitConverter]::ToInt32($bytes,0x3c)
        if($pe -lt 0 -or $pe -gt $bytes.Length-6 -or [BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or
           [BitConverter]::ToUInt16($bytes,$pe+4) -ne $machines[$relative]){throw "Wrong PE architecture: $relative"}
    }
    . "$Repo\sentence_package.ps1"
    Assert-NativeTigerSentenceModel "$Package\shared\Models\sentence-fivegram-mobile.bin"
    Assert-NativeTigerSentenceLexicalPrior "$Package\shared\Models\sentence-lexical-v1.bin"
    foreach($relative in @('shared\tiger-v2.tcd','shared\tiger-v2.tcd.sentence.tcd','shared\tiger-v2.tcd.supplement.tcd','shared\字体\LXGWWenKaiGBScreen.ttf','shared\字体\OFL.txt','setup\gui.txt','default-config.txt')){
        if(!$listed.ContainsKey($relative) -or (Get-Item -LiteralPath (Join-Path $Package $relative)).Length -eq 0){throw "Missing runtime resource: $relative"}
    }
    . "$Repo\package_sources.ps1"
    $sources=Get-NativeTigerPackageSources "$Repo\resources\DefaultData" "$Repo\resources\Skins"
    $expected=@{}
    foreach($folder in @($sources.Schemes)+@($sources.Reverse)){
        foreach($file in Get-ChildItem -LiteralPath $folder.FullName -Recurse -File){
            $relative=$file.FullName.Substring((Join-Path $Repo 'resources\DefaultData').Length+1)
            $expected[('DefaultData\'+$relative)]=$file.FullName
        }
    }
    foreach($skin in $sources.Skins){$expected[('DefaultData\皮肤\'+$skin.Name)]=$skin.FullName}
    if(@($manifest.files|Where-Object {$_.path -like 'DefaultData\*'}).Count -ne $expected.Count){throw 'Bundled user resource inventory differs from current sources.'}
    foreach($relative in $expected.Keys){
        if(!$listed.ContainsKey($relative) -or $listed[$relative].sha256 -ne (Get-FileHash -LiteralPath $expected[$relative]).Hash){throw "Bundled resource does not match workspace: $relative"}
    }
    foreach($file in Get-ChildItem -LiteralPath "$Repo\packaging" -File){
        if($file.Name -in @('README.txt','install.cmd','install.ps1','uninstall.ps1','rollback.ps1')){continue}
        if(!$listed.ContainsKey($file.Name) -or $listed[$file.Name].sha256 -ne (Get-FileHash -LiteralPath $file.FullName).Hash){throw "Stale package script: $($file.Name)"}
    }
    foreach($relative in @('setup\deploy.ps1','appcontainer_data.ps1')){
        $source=if($relative -like 'setup\*'){Join-Path "$Repo\packaging" $relative}else{Join-Path $Repo $relative}
        if(!$listed.ContainsKey($relative) -or $listed[$relative].sha256 -ne (Get-FileHash -LiteralPath $source).Hash){throw "Stale runtime script: $relative"}
    }
    if((Get-FileHash -LiteralPath "$Package\README.txt").Hash -ne (Get-FileHash -LiteralPath "$Repo\packaging\setup\README.txt").Hash){throw 'Incorrect universal installer README.'}
    foreach($legacy in @('install.cmd','install.ps1','uninstall.ps1','rollback.ps1')){
        if(Test-Path -LiteralPath (Join-Path $Package $legacy)){throw "Legacy lifecycle entry in GUI package: $legacy"}
    }
    [ordered]@{status='passed';version=$Version;files=$manifest.files.Count;resource_files=$expected.Count;
        skins=@($sources.Skins.Name);schemes=@($sources.Schemes.Name);versions=$versions;
        load_checks=@(Get-PackageLoadChecks $Package|ForEach-Object {$_.probe+'->'+$_.dll});
        arm64_dual_view_executed=((Get-SystemArchitecture) -eq 'Arm64');machine_installation_tested=$false}
}

function Publish-TigirlUniversalBuild($Run,[string]$OutputRoot,$Report) {
    if($Report.status -ne 'passed'){throw 'Only a verified build can be published.'}
    # Product version is not the build date. Keep it in the name with an explicit
    # v prefix, and use the recorded run start for the human-facing date/time.
    $started=[DateTimeOffset]::MinValue
    if(![DateTimeOffset]::TryParseExact([string]$Run.started,'o',[Globalization.CultureInfo]::InvariantCulture,
        [Globalization.DateTimeStyles]::None,[ref]$started)){throw 'Missing or invalid build start timestamp.'}
    $stamp=$started.ToString('yyyyMMdd-HHmmss',[Globalization.CultureInfo]::InvariantCulture)
    $name="虎娘-$stamp-v$($Run.version)-x64-ARM64-x86-安装程序.exe"
    $sourceName="虎娘-$($Run.version)-x64-ARM64-x86-安装程序.exe"
    $source=Join-Path $Run.compilerOutput $sourceName
    $metadata=Get-Content -LiteralPath ($source+'.build.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $hash=(Get-FileHash -LiteralPath $source).Hash
    if($metadata.architecture -ne 'universal' -or $metadata.version -ne $Run.version -or $metadata.sha256 -ne $hash -or
       $metadata.bytes -ne (Get-Item -LiteralPath $source).Length -or
       $metadata.payloadManifestSha256 -ne (Get-FileHash -LiteralPath "$($Run.payload)\manifest.json").Hash -or
       $metadata.setupScriptSha256 -ne $Report.source_hashes['packaging\setup\Tigirl.iss'] -or
       (Get-Content -LiteralPath ($source+'.sha256') -Raw -Encoding UTF8).Trim() -ne ($hash+'  '+$sourceName)){
        throw 'Installer output metadata/hash verification failed.'
    }
    $destination=Join-Path $OutputRoot ("releases\"+$Run.version+'-universal-'+$Run.id)
    $pending=$destination+'.partial'
    if((Test-Path -LiteralPath $destination) -or (Test-Path -LiteralPath $pending)){throw 'Release output collision; existing results are preserved.'}
    New-Item -ItemType Directory -Path $pending -Force|Out-Null
    for($attempt=0;;$attempt++){
        try {Copy-Item -LiteralPath $source -Destination (Join-Path $pending $name) -ErrorAction Stop;break}
        catch {if($attempt -ge 4){throw};Start-Sleep -Seconds 1}
    }
    if((Get-FileHash -LiteralPath (Join-Path $pending $name)).Hash -ne $hash){throw 'Copied installer hash mismatch.'}
    # Sidecar contents must reference the published filename, not the compiler's
    # internal version-only name. The EXE and its embedded version stay intact.
    ($hash+'  '+$name)|Set-Content -LiteralPath (Join-Path $pending ($name+'.sha256')) -Encoding UTF8
    $publishedMetadata=[ordered]@{}
    foreach($property in $metadata.PSObject.Properties){$publishedMetadata[$property.Name]=$property.Value}
    $publishedMetadata['build_started']=$Run.started
    $publishedMetadata['artifact_file']=$name
    $publishedMetadata['source_artifact']=$sourceName
    Write-TigirlBuildJson (Join-Path $pending ($name+'.build.json')) $publishedMetadata
    Copy-Item -LiteralPath "$($Run.payload)\manifest.json" -Destination (Join-Path $pending ($name+'.manifest.json'))
    Write-TigirlBuildJson (Join-Path $pending 'verification.json') $Report
    [IO.Directory]::Move($pending,$destination)
    $result=[ordered]@{status='passed';version=$Run.version;architecture='universal';run=$Run.id;build_started=$Run.started;
        installer=(Join-Path $destination $name);sha256=$hash;bytes=$metadata.bytes;
        verification=(Join-Path $destination 'verification.json');logs=$Run.logs;payload=$Run.payload;
        finished=(Get-Date -Format o)}
    # This pointer changes only after the complete artifact set has been verified.
    Write-TigirlBuildJson (Join-Path $OutputRoot 'latest-universal.json') $result
    return $result
}
