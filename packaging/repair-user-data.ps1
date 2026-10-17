param([string]$Package)
$ErrorActionPreference='Stop'
# Run as the affected Windows user. This tool does not register/unregister the IME.
$root=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Tigirl'
if($env:NATIVE_TIGER_USER_ROOT){$root=[IO.Path]::GetFullPath($env:NATIVE_TIGER_USER_ROOT)}
New-Item -ItemType Directory -Force $root|Out-Null
$log=Join-Path $root 'setup-repair.log'
function Log([string]$Text){$line=('{0:o} {1}' -f [DateTime]::Now,$Text);Write-Host $line;Add-Content -LiteralPath $log -Value $line -Encoding UTF8}
function Invoke-RepairStep([string]$Action){
    $process=[Diagnostics.Process]::new()
    try{
        $process.StartInfo.FileName=Join-Path $env:windir 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $process.StartInfo.Arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$Package+'\initialize.ps1" -Quiet -NoDialogs -NoEnable -SkipConflicts -Transaction "'+$transaction+'" -TransactionAction '+$Action
        $process.StartInfo.UseShellExecute=$false;$process.StartInfo.CreateNoWindow=$true
        if(!$process.Start()){throw '无法启动用户数据初始化。'}
        $process.WaitForExit();Log ("$Action exit="+$process.ExitCode)
        if($process.ExitCode){throw '用户数据初始化失败，请查看 setup-initialize.log。'}
    }finally{$process.Dispose()}
}
try{
    if(!$Package){
        $base=[Environment]::GetFolderPath('ProgramFiles');if(${env:ProgramW6432}){$base=${env:ProgramW6432}}
        $record=Join-Path $base 'Tigirl\install.json'
        if(!(Test-Path -LiteralPath $record)){throw '没有找到虎娘安装记录，请先安装虎娘。'}
        $Package=(Get-Content -LiteralPath $record -Raw|ConvertFrom-Json).directory
    }
    $Package=[IO.Path]::GetFullPath($Package)
    $manifest=Get-Content -LiteralPath (Join-Path $Package 'manifest.json') -Raw|ConvertFrom-Json
    Log "Version=$($manifest.version); Package=$Package; UserRoot=$root"
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    $elevated=([Security.Principal.WindowsPrincipal]$identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    Log "CurrentSid=$($identity.User.Value); Elevated=$elevated; ExistingInitializeLog=$(Test-Path (Join-Path $root 'setup-initialize.log')); ExistingMarker=$(Test-Path (Join-Path $root 'installed-data-version.txt'))"
    $data=@($manifest.files|Where-Object {$_.path -like 'DefaultData\*'})
    if(!$data.Count){throw '安装目录内没有默认资源清单，请重新下载安装程序。'}
    # Validate scripts as well as data before executing the installed initializer.
    foreach($entry in $manifest.files){
        if(!$entry.path -or $entry.path -match '(^[\\/]|:|(^|[\\/])\.\.([\\/]|$))'){throw '安装清单包含无效路径。'}
        $file=Join-Path $Package $entry.path
        if(!(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-FileHash -LiteralPath $file).Hash -ne $entry.sha256){throw "安装文件缺失或损坏：$($entry.path)。请重新下载安装程序。"}
    }
    $transaction='repair-'+[guid]::NewGuid().ToString('N')
    # A transaction bypasses a stale same-version marker. Existing user files and
    # settings retain the normal noninteractive conflict/rollback protections.
    Invoke-RepairStep 'Initialize'
    foreach($entry in $data){
        $relative=$entry.path.Substring('DefaultData\'.Length)
        if(!(Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf)){throw "资源没有复制成功：$relative"}
    }
    foreach($scheme in Get-ChildItem -LiteralPath (Join-Path $Package 'DefaultData\码表') -Directory){
        if(!(Test-Path -LiteralPath (Join-Path $root ('schemas\'+$scheme.Name+'\current.txt')))){throw "码表没有编译成功：$($scheme.Name)"}
    }
    Invoke-RepairStep 'Complete'
    Log '修复完成：皮肤、码表及缓存已补齐，已有设置和个人词条已保留。请重新打开输入设置和输入法菜单。'
    exit 0
}catch{
    Log ('修复未完成：'+($_|Out-String))
    # Never roll back another initializer that acquired the lock concurrently.
    if($transaction){try{
        $journal=Join-Path $root '.setup-user-transaction.json'
        if((Test-Path -LiteralPath $journal) -and (Get-Content -LiteralPath $journal -Raw|ConvertFrom-Json).id -eq $transaction){Invoke-RepairStep 'Rollback'}
    }catch{Log ('回滚检查：'+($_|Out-String))}}
    exit 1
}
