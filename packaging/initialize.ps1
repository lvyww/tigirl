param([switch]$Quiet,[switch]$NoEnable,[switch]$SkipConflicts,[switch]$RequireStandardUser,[switch]$NoDialogs,
 [string]$Transaction,[ValidateSet('Initialize','Complete','Rollback')][string]$TransactionAction='Initialize')
$ErrorActionPreference='Stop'
. "$PSScriptRoot\data.ps1"
. "$PSScriptRoot\appcontainer_data.ps1"
. "$PSScriptRoot\user_transaction.ps1"
if($RequireStandardUser -and ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){exit 20}
$root=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Tigirl'
if($env:NATIVE_TIGER_USER_ROOT){if(!$NoEnable){throw 'A test user-root override cannot enable a production profile.'};$root=[IO.Path]::GetFullPath($env:NATIVE_TIGER_USER_ROOT)}
Assert-PlainTree $root
New-Item -ItemType Directory -Force $root|Out-Null
# Wait for the compiler itself, not its entire descendant process tree. The
# compiler synchronously waits for its import workers before returning.
function Invoke-InitializerCompiler([string]$Executable) {
    $process=[Diagnostics.Process]::new()
    try {
        $process.StartInfo.FileName=$Executable
        $process.StartInfo.Arguments='--initialize'
        $process.StartInfo.UseShellExecute=$false
        $process.StartInfo.CreateNoWindow=$true
        if(!$process.Start()){throw 'Cannot start dictionary compiler.'}
        Write-InitializeLog "Compiler started: PID=$($process.Id)"
        $process.WaitForExit()
        $code=$process.ExitCode
        Write-InitializeLog "Compiler exited: code=$code"
        return $code
    }finally{$process.Dispose()}
}
function Write-InitializeLog([string]$Message) {
    # Diagnostic failures must not change installation/rollback behavior.
    try { ('{0:o} PID={1} tx={2} {3}' -f [DateTime]::Now,$PID,$Transaction,$Message) |
        Add-Content -LiteralPath (Join-Path $root 'setup-initialize.log') -Encoding UTF8
    }catch{}
}
$lock=$null;$changes=@();$snapshots=@();$compilationStarted=$false;$config=Join-Path $root 'config.txt'
try {
    Write-InitializeLog "Begin $TransactionAction"
    $lock=[IO.File]::Open((Join-Path $root '.initialize.lock'),'OpenOrCreate','ReadWrite','None')
    $userJournal=Join-Path $root '.setup-user-transaction.json'
    if($TransactionAction -eq 'Rollback'){Restore-UserJournal $root;exit 0}
    if($TransactionAction -eq 'Complete'){if(Test-Path $userJournal){Remove-Item $userJournal};exit 0}
    if(Test-Path $userJournal){
        $pending=Get-Content $userJournal -Raw|ConvertFrom-Json
        $machineRecord=Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'install.json'
        $committed=$false
        if(Test-Path $machineRecord){$m=Get-Content $machineRecord -Raw|ConvertFrom-Json;$committed=($pending.id -and $m.transaction -eq $pending.id)}
        # Machine registration now commits before user initialization. Only a journal
        # explicitly marked initialized can be discarded after a committed machine tx;
        # a prepared journal means the user step was interrupted and must be restored.
        if($committed -and $pending.state -eq 'initialized'){Remove-Item $userJournal}else{Restore-UserJournal $root}
    }
    $version=(Get-Content -LiteralPath "$PSScriptRoot\manifest.json" -Raw|ConvertFrom-Json).version
    $marker=Join-Path $root 'installed-data-version.txt'
    if($Quiet -and !$Transaction -and (Test-Path -LiteralPath $marker) -and (Get-Content -LiteralPath $marker -Raw).Trim() -eq $version){exit 0}
    Write-InitializeLog 'Scanning dictionary conflicts'
    $plan=@(Get-DataMerge "$PSScriptRoot\DefaultData" $root)
    $conflictCount=@($plan|Where-Object Conflict).Count
    Write-InitializeLog "Scan complete: files=$($plan.Count), conflicts=$conflictCount"
    # -NoDialogs is a hard non-interactive contract. Conflict entries already default
    # to Skip, so installers and other hidden callers safely preserve the user's files.
    if(!$SkipConflicts -and !$NoDialogs){
        Write-InitializeLog 'Selecting conflict actions (dialog if conflicts exist)'
        Select-DataConflicts $plan
        Write-InitializeLog 'Conflict selection complete'
    }elseif($conflictCount){
        Write-InitializeLog "Conflict dialogs suppressed; retaining existing files: conflicts=$conflictCount"
    }
    $backup=Join-Path $root ('backups\install-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
    # Snapshot descriptors before the compiler publishes any generations.
    $paths=@($config,(Join-Path $root 'installed-data-version.txt'))+@(Get-ChildItem -LiteralPath (Join-Path $root 'schemas') -Filter current.txt -Recurse -ErrorAction SilentlyContinue|ForEach-Object FullName)
    foreach($path in $paths){$snapshots+= [pscustomobject]@{Path=$path;Bytes=$(if(Test-Path -LiteralPath $path){[IO.File]::ReadAllBytes($path)}else{$null})}}
    Save-UserJournal @{id=$Transaction;state='prepared';backup=$backup;snapshots=$snapshots} $userJournal
    $compilationStarted=$true
    Write-InitializeLog 'Copying user data'
    $changes=@(Invoke-DataMerge $plan $backup)
    if(!(Test-Path -LiteralPath $config)){Copy-Item -LiteralPath "$PSScriptRoot\default-config.txt" -Destination $config}
    Write-InitializeLog 'Data copy complete; applying AppContainer permissions'
    Set-NativeTigerAppContainerAccess $root
    Write-InitializeLog 'Permissions complete; compiling dictionaries'
    $compilationStarted=$true
    $compilerExit=Invoke-InitializerCompiler "$PSScriptRoot\x64\Tigirl.exe"
    if($compilerExit -ne 0){throw '码表编译失败，已保留原方案。请检查码表格式后重试。'}
    if(!$NoEnable) {
        Write-InitializeLog 'Enabling input profile'
        Add-Type -TypeDefinition 'using System.Runtime.InteropServices; public static class NativeTigerTip { [DllImport("input.dll", CharSet=CharSet.Unicode)] public static extern bool InstallLayoutOrTip(string profile, uint flags); }'
        if(![NativeTigerTip]::InstallLayoutOrTip('0804:{D2291A80-84D8-4641-9AB2-BDD1472C846B}{83955C0E-2C09-47A5-BCF3-F2B98E11EE8B}',0)){throw 'Cannot enable input profile.'}
    }
    Write-InitializeLog 'Writing completion marker'
    $version|Set-Content -LiteralPath $marker -Encoding UTF8
    Save-UserJournal @{id=$Transaction;state='initialized';backup=$backup;snapshots=$snapshots} $userJournal
    $copied=@($plan|Where-Object Choice -eq 'Copy').Count;$skipped=$plan.Count-$copied
    if(!$Transaction -and (Test-Path $userJournal)){Remove-Item $userJournal}
    if(!$Quiet -and !$NoDialogs){Add-Type -AssemblyName System.Windows.Forms;[Windows.Forms.MessageBox]::Show("安装完成。复制 $copied 个文件，跳过 $skipped 个文件。`n请重开正在使用的程序。",'虎娘')|Out-Null}
}catch {
    Write-InitializeLog ('Failed: '+($_|Out-String))
    if($compilationStarted -or $changes.Count){Restore-UserJournal $root}
    ($_|Out-String)|Add-Content -LiteralPath (Join-Path $root 'setup-initialize.log') -Encoding UTF8
    if($NoDialogs){Write-Error $_ -ErrorAction Continue;exit 1}
    Add-Type -AssemblyName System.Windows.Forms
    [Windows.Forms.MessageBox]::Show($_.Exception.Message,'虎娘：初始化未完成')|Out-Null
    exit 1
}finally{Write-InitializeLog "End $TransactionAction";if($lock){$lock.Dispose()}}
