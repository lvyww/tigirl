param([switch]$Quiet,[switch]$NoEnable,[switch]$SkipConflicts,[switch]$RequireStandardUser,[switch]$NoDialogs,
 [string]$Transaction,[ValidateSet('Initialize','Complete','Rollback')][string]$TransactionAction='Initialize')
$ErrorActionPreference='Stop'
. "$PSScriptRoot\data.ps1"
. "$PSScriptRoot\appcontainer_data.ps1"
. "$PSScriptRoot\user_transaction.ps1"
if($RequireStandardUser -and ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){exit 20}
$root=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'NativeTiger'
if($env:NATIVE_TIGER_USER_ROOT){if(!$NoEnable){throw 'A test user-root override cannot enable a production profile.'};$root=[IO.Path]::GetFullPath($env:NATIVE_TIGER_USER_ROOT)}
Assert-PlainTree $root
New-Item -ItemType Directory -Force $root|Out-Null
$lock=$null;$changes=@();$snapshots=@();$compilationStarted=$false;$config=Join-Path $root 'config.txt'
try {
    $lock=[IO.File]::Open((Join-Path $root '.initialize.lock'),'OpenOrCreate','ReadWrite','None')
    $userJournal=Join-Path $root '.setup-user-transaction.json'
    if($TransactionAction -eq 'Rollback'){Restore-UserJournal $root;exit 0}
    if($TransactionAction -eq 'Complete'){if(Test-Path $userJournal){Remove-Item $userJournal};exit 0}
    if(Test-Path $userJournal){
        $pending=Get-Content $userJournal -Raw|ConvertFrom-Json
        $machineRecord=Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'install.json'
        $committed=$false
        if(Test-Path $machineRecord){$m=Get-Content $machineRecord -Raw|ConvertFrom-Json;$committed=($pending.id -and $m.transaction -eq $pending.id)}
        if($committed){Remove-Item $userJournal}else{Restore-UserJournal $root}
    }
    $version=(Get-Content -LiteralPath "$PSScriptRoot\manifest.json" -Raw|ConvertFrom-Json).version
    $marker=Join-Path $root 'installed-data-version.txt'
    if($Quiet -and !$Transaction -and (Test-Path -LiteralPath $marker) -and (Get-Content -LiteralPath $marker -Raw).Trim() -eq $version){exit 0}
    $plan=@(Get-DataMerge "$PSScriptRoot\DefaultData" $root)
    # Legacy source settings are read only for this one-time migration.
    if((Test-Path -LiteralPath $config) -and !(Test-Path -LiteralPath (Join-Path $root 'fixed-path-migrated.txt'))) {
        $legacy=@{};foreach($line in Get-Content -LiteralPath $config -Encoding UTF8){if($line -match '^\s*(码表存储位置|拼音反查目录)[\t ,]+(.+?)\s*$'){$legacy[$matches[1]]=$matches[2]}}
        $oldSource=$legacy['码表存储位置'];$oldPinyin=$legacy['拼音反查目录']
        if($oldSource -and ![IO.Path]::IsPathRooted($oldSource)){$oldSource=Join-Path $root $oldSource}
        if(!$oldPinyin -and $oldSource){$oldPinyin=Join-Path (Split-Path $oldSource -Parent) '拼音反查码表'}
        # A migration is explicit in the same conflict dialog. Existing legacy
        # files take precedence over bundled defaults for the initial copy.
        foreach($pair in @(@($oldSource,(Join-Path $root '码表')),@($oldPinyin,(Join-Path $root '拼音反查码表')))) {
            if($pair[0] -and (Test-Path -LiteralPath $pair[0]) -and [IO.Path]::GetFullPath($pair[0]).TrimEnd('\') -ne [IO.Path]::GetFullPath($pair[1]).TrimEnd('\')) {
                $migration=@(Get-DataMerge $pair[0] $pair[1]);$targets=@{};foreach($item in $migration){$targets[$item.Target]=$true}
                $plan=@($plan|Where-Object{!$targets.ContainsKey($_.Target)})+$migration
            }
        }
    }
    if(!$SkipConflicts){Select-DataConflicts $plan}
    $backup=Join-Path $root ('backups\install-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
    # Snapshot descriptors before the compiler publishes any generations.
    $paths=@($config,(Join-Path $root 'installed-data-version.txt'),(Join-Path $root 'fixed-path-migrated.txt'))+@(Get-ChildItem -LiteralPath (Join-Path $root 'schemas') -Filter current.txt -Recurse -ErrorAction SilentlyContinue|ForEach-Object FullName)
    foreach($path in $paths){$snapshots+= [pscustomobject]@{Path=$path;Bytes=$(if(Test-Path -LiteralPath $path){[IO.File]::ReadAllBytes($path)}else{$null})}}
    Save-UserJournal @{id=$Transaction;backup=$backup;snapshots=$snapshots} $userJournal
    $compilationStarted=$true
    $changes=@(Invoke-DataMerge $plan $backup)
    if(!(Test-Path -LiteralPath $config)){Copy-Item -LiteralPath "$PSScriptRoot\default-config.txt" -Destination $config}
    Set-NativeTigerAppContainerAccess $root
    $compilationStarted=$true
    $process=Start-Process -FilePath "$PSScriptRoot\x64\Tigirl.exe" -ArgumentList '--initialize' -Wait -PassThru
    if($process.ExitCode){throw '码表编译失败，已保留原方案。请检查码表格式后重试。'}
    if(!$NoEnable) {
        Add-Type -TypeDefinition 'using System.Runtime.InteropServices; public static class NativeTigerTip { [DllImport("input.dll", CharSet=CharSet.Unicode)] public static extern bool InstallLayoutOrTip(string profile, uint flags); }'
        if(![NativeTigerTip]::InstallLayoutOrTip('0804:{D2291A80-84D8-4641-9AB2-BDD1472C846B}{83955C0E-2C09-47A5-BCF3-F2B98E11EE8B}',0)){throw 'Cannot enable input profile.'}
    }
    $version|Set-Content -LiteralPath $marker -Encoding UTF8
    'Source paths are now fixed; legacy originals were retained.'|Set-Content -LiteralPath (Join-Path $root 'fixed-path-migrated.txt') -Encoding UTF8
    $copied=@($plan|Where-Object Choice -eq 'Copy').Count;$skipped=$plan.Count-$copied
    if(!$Transaction -and (Test-Path $userJournal)){Remove-Item $userJournal}
    if(!$Quiet -and !$NoDialogs){Add-Type -AssemblyName System.Windows.Forms;[Windows.Forms.MessageBox]::Show("安装完成。复制 $copied 个文件，跳过 $skipped 个文件。`n请重开正在使用的程序。",'虎娘')|Out-Null}
}catch {
    if($compilationStarted -or $changes.Count){Restore-UserJournal $root}
    ($_|Out-String)|Add-Content -LiteralPath (Join-Path $root 'setup-initialize.log') -Encoding UTF8
    if($NoDialogs){Write-Error $_ -ErrorAction Continue;exit 1}
    Add-Type -AssemblyName System.Windows.Forms
    [Windows.Forms.MessageBox]::Show($_.Exception.Message,'虎娘：初始化未完成')|Out-Null
    exit 1
}finally{if($lock){$lock.Dispose()}}
