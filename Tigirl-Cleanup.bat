@echo off
setlocal DisableDelayedExpansion
set "TIGIRL_CLEAN_SELF=%~f0"
set "TIGIRL_CLEAN_CHECK=%~1"
set "TIGIRL_CLEAN_PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "TIGIRL_CLEAN_PS=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%TIGIRL_CLEAN_PS%" -NoProfile -OutputFormat Text -ExecutionPolicy Bypass -EncodedCommand JABFAHIAcgBvAHIAQQBjAHQAaQBvAG4AUAByAGUAZgBlAHIAZQBuAGMAZQA9ACcAUwB0AG8AcAAnADsAIAAkAHAAPQAkAGUAbgB2ADoAVABJAEcASQBSAEwAXwBDAEwARQBBAE4AXwBTAEUATABGADsAIAAkAGMAaABlAGMAawA9ACQAZQBuAHYAOgBUAEkARwBJAFIATABfAEMATABFAEEATgBfAEMASABFAEMASwAgAC0AZQBxACAAJwAvAGMAaABlAGMAawAnADsAIAAkAGEAZABtAGkAbgA9ACgAWwBTAGUAYwB1AHIAaQB0AHkALgBQAHIAaQBuAGMAaQBwAGEAbAAuAFcAaQBuAGQAbwB3AHMAUAByAGkAbgBjAGkAcABhAGwAXQBbAFMAZQBjAHUAcgBpAHQAeQAuAFAAcgBpAG4AYwBpAHAAYQBsAC4AVwBpAG4AZABvAHcAcwBJAGQAZQBuAHQAaQB0AHkAXQA6ADoARwBlAHQAQwB1AHIAcgBlAG4AdAAoACkAKQAuAEkAcwBJAG4AUgBvAGwAZQAoAFsAUwBlAGMAdQByAGkAdAB5AC4AUAByAGkAbgBjAGkAcABhAGwALgBXAGkAbgBkAG8AdwBzAEIAdQBpAGwAdABJAG4AUgBvAGwAZQBdADoAOgBBAGQAbQBpAG4AaQBzAHQAcgBhAHQAbwByACkAOwAgAGkAZgAoACQAYQBkAG0AaQBuACAALQBvAHIAIAAkAGMAaABlAGMAawApAHsAIAAmACAAKABbAHMAYwByAGkAcAB0AGIAbABvAGMAawBdADoAOgBDAHIAZQBhAHQAZQAoACgAWwBJAE8ALgBGAGkAbABlAF0AOgA6AFIAZQBhAGQAQQBsAGwAVABlAHgAdAAoACQAZQBuAHYAOgBUAEkARwBJAFIATABfAEMATABFAEEATgBfAFMARQBMAEYALABbAFQAZQB4AHQALgBFAG4AYwBvAGQAaQBuAGcAXQA6ADoAVQBUAEYAOAApACAALQBzAHAAbABpAHQAIAAnACgAPwBtACkAXgAjACMAIwBQAE8AVwBFAFIAUwBIAEUATABMACMAIwAjAFwAcgA/ACQAJwAsADIAKQBbADEAXQApACkAIAAtAEMAaABlAGMAawBPAG4AbAB5ADoAKAAkAGUAbgB2ADoAVABJAEcASQBSAEwAXwBDAEwARQBBAE4AXwBDAEgARQBDAEsAIAAtAGUAcQAgACcALwBjAGgAZQBjAGsAJwApACAAfQAgAGUAbABzAGUAIAB7ACAAJABsAGkAdABlAHIAYQBsAD0AJABwAC4AUgBlAHAAbABhAGMAZQAoACIAJwAiACwAIgAnACcAIgApADsAIAAkAGMAbwBkAGUAPQAiAGAAJABlAG4AdgA6AFQASQBHAEkAUgBMAF8AQwBMAEUAQQBOAF8AUwBFAEwARgA9ACcAJABsAGkAdABlAHIAYQBsACcAOwAgAGAAJABlAG4AdgA6AFQASQBHAEkAUgBMAF8AQwBMAEUAQQBOAF8AQwBIAEUAQwBLAD0AJwAnADsAIAAiACAAKwAgACcAJgAgACgAWwBzAGMAcgBpAHAAdABiAGwAbwBjAGsAXQA6ADoAQwByAGUAYQB0AGUAKAAoAFsASQBPAC4ARgBpAGwAZQBdADoAOgBSAGUAYQBkAEEAbABsAFQAZQB4AHQAKAAkAGUAbgB2ADoAVABJAEcASQBSAEwAXwBDAEwARQBBAE4AXwBTAEUATABGACwAWwBUAGUAeAB0AC4ARQBuAGMAbwBkAGkAbgBnAF0AOgA6AFUAVABGADgAKQAgAC0AcwBwAGwAaQB0ACAAJwAnACgAPwBtACkAXgAjACMAIwBQAE8AVwBFAFIAUwBIAEUATABMACMAIwAjAFwAcgA/ACQAJwAnACwAMgApAFsAMQBdACkAKQAgAC0AQwBoAGUAYwBrAE8AbgBsAHkAOgAoACQAZQBuAHYAOgBUAEkARwBJAFIATABfAEMATABFAEEATgBfAEMASABFAEMASwAgAC0AZQBxACAAJwAnAC8AYwBoAGUAYwBrACcAJwApACcAOwAgACQAZQBuAGMAbwBkAGUAZAA9AFsAQwBvAG4AdgBlAHIAdABdADoAOgBUAG8AQgBhAHMAZQA2ADQAUwB0AHIAaQBuAGcAKABbAFQAZQB4AHQALgBFAG4AYwBvAGQAaQBuAGcAXQA6ADoAVQBuAGkAYwBvAGQAZQAuAEcAZQB0AEIAeQB0AGUAcwAoACQAYwBvAGQAZQApACkAOwAgAHQAcgB5AHsAJABjAGgAaQBsAGQAPQBTAHQAYQByAHQALQBQAHIAbwBjAGUAcwBzACAALQBGAGkAbABlAFAAYQB0AGgAIAAoAEoAbwBpAG4ALQBQAGEAdABoACAAJABQAFMASABPAE0ARQAgACcAcABvAHcAZQByAHMAaABlAGwAbAAuAGUAeABlACcAKQAgAC0AVgBlAHIAYgAgAFIAdQBuAEEAcwAgAC0AQQByAGcAdQBtAGUAbgB0AEwAaQBzAHQAIAAoACcALQBOAG8AUAByAG8AZgBpAGwAZQAgAC0ATwB1AHQAcAB1AHQARgBvAHIAbQBhAHQAIABUAGUAeAB0ACAALQBFAHgAZQBjAHUAdABpAG8AbgBQAG8AbABpAGMAeQAgAEIAeQBwAGEAcwBzACAALQBFAG4AYwBvAGQAZQBkAEMAbwBtAG0AYQBuAGQAIAAnACsAJABlAG4AYwBvAGQAZQBkACkAIAAtAFcAYQBpAHQAIAAtAFAAYQBzAHMAVABoAHIAdQA7ACAAZQB4AGkAdAAgACQAYwBoAGkAbABkAC4ARQB4AGkAdABDAG8AZABlAH0AYwBhAHQAYwBoAHsAVwByAGkAdABlAC0ASABvAHMAdAAgACQAXwA7AGUAeABpAHQAIAAxAH0AIAB9AA==
set "TIGIRL_CLEAN_RESULT=%errorlevel%"
echo.
echo Cleanup exit code: %TIGIRL_CLEAN_RESULT% (0=done, 2=cancelled, 3010=restart required, 1=incomplete).
echo Logs: %%TEMP%%\Tigirl-Cleanup-*.log (account used for elevation).
if /i not "%~1"=="/check" pause
exit /b %TIGIRL_CLEAN_RESULT%
###POWERSHELL###
# Embedded in Tigirl-Cleanup.bat by build_cleanup.py. Windows PowerShell 5.1.
param([switch]$CheckOnly)
$ErrorActionPreference='Stop'
$clsid='{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}'
$profile='{43201C7B-F615-469D-9D54-906D9270975E}'
# Legacy Microsoft-sample IDs: used ONLY for ownership-checked retirement.
function Test-TigirlLegacyDll([string]$Path,[string]$DevelopmentRoot) {
    if(!$Path -or !(Test-Path -LiteralPath $Path -PathType Leaf)){return $false}
    $full=[IO.Path]::GetFullPath($Path)
    $root=Join-Path $env:ProgramFiles 'Tigirl\versions'
    if(!$full.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($full) -ne 'Tigirl.dll'){return $false}
    $parent=Split-Path $full -Parent
    while($parent -and $parent.Length -ge $root.Length){
        if((Get-Item -LiteralPath $parent).Attributes -band [IO.FileAttributes]::ReparsePoint){return $false}
        $manifest=Join-Path $parent 'manifest.json'
        if(Test-Path -LiteralPath $manifest -PathType Leaf){
            $relative=$full.Substring($parent.Length+1)
            $entry=@((Get-Content -LiteralPath $manifest -Raw|ConvertFrom-Json).files|Where-Object {$_.path -eq $relative})
            if($entry.Count -eq 1 -and $entry[0].sha256 -eq (Get-FileHash -LiteralPath $full).Hash){return $true}
        }
        $parent=Split-Path $parent -Parent
    }
    if($DevelopmentRoot){
        foreach($name in @('native-install.json','native-install-x86.json')){
            $record=Join-Path $DevelopmentRoot ('build\'+$name)
            if(Test-Path -LiteralPath $record){
                $r=Get-Content -LiteralPath $record -Raw|ConvertFrom-Json
                if($r.dll -eq $full -and $r.dll_hash -eq (Get-FileHash -LiteralPath $full).Hash){return $true}
            }
        }
    }
    return $false
}
function Get-TigirlLegacyPaths {
    $result=@()
    foreach($view in @('Registry64','Registry32')){
        # A user override may belong to an entirely different sample-derived IME.
        foreach($hive in @('CurrentUser','LocalMachine')){
            $base=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::$hive,[Microsoft.Win32.RegistryView]::$view)
            try{
                $key=$base.OpenSubKey('SOFTWARE\Classes\CLSID\{D2291A80-84D8-4641-9AB2-BDD1472C846B}\InprocServer32')
                if($key){try{$result+= [pscustomobject]@{view=$view;hive=$hive;path=[string]$key.GetValue('')}}finally{$key.Dispose()}}
            }finally{$base.Dispose()}
        }
    }
    return $result
}
function Remove-TigirlLegacyIdentity([string]$DevelopmentRoot) {
    $paths=@(Get-TigirlLegacyPaths)
    if(!$paths.Count){return}
    foreach($p in $paths){
        if($p.hive -ne 'LocalMachine' -or !(Test-TigirlLegacyDll $p.path $DevelopmentRoot)){
            Write-Warning 'Legacy sample identity ownership is uncertain; retained all legacy registrations.';return
        }
    }
    if(!('TigirlLegacyTip' -as [type])){
        Add-Type @'
using System.Runtime.InteropServices;
public static class TigirlLegacyTip {
 [DllImport("input.dll", CharSet=CharSet.Unicode)]
 [return:MarshalAs(UnmanagedType.Bool)]
 public static extern bool InstallLayoutOrTip(string tip, uint flags);
}
'@
    }
    [void][TigirlLegacyTip]::InstallLayoutOrTip('0804:{D2291A80-84D8-4641-9AB2-BDD1472C846B}{83955C0E-2C09-47A5-BCF3-F2B98E11EE8B}',1)
    # Validate every view before invoking any old DLL, since profiles are shared.
    foreach($p in $paths){
        $current=@(Get-TigirlLegacyPaths)
        foreach($c in $current){
            if($c.hive -ne 'LocalMachine' -or !(Test-TigirlLegacyDll $c.path $DevelopmentRoot)){throw 'Legacy registration ownership changed.'}
        }
        $system=if($p.view -eq 'Registry32'){'SysWOW64'}else{'System32'}
        $process=Start-Process "$env:windir\$system\regsvr32.exe" -ArgumentList @('/s','/u',('"'+$p.path+'"')) -Wait -PassThru
        if($process.ExitCode){throw "Legacy unregistration failed: $($p.view)"}
    }
    foreach($view in @('Registry64','Registry32')){
        $base=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,[Microsoft.Win32.RegistryView]::$view)
        try{
            $keyPath='SOFTWARE\Microsoft\Active Setup\Installed Components\{D2291A80-84D8-4641-9AB2-BDD1472C846B}'
            $key=$base.OpenSubKey($keyPath)
            if($key){
                try{$stub=[string]$key.GetValue('StubPath')}finally{$key.Dispose()}
                foreach($p in $paths){
                    $directory=Split-Path (Split-Path $p.path -Parent) -Parent
                    if($stub.Contains('"'+$directory+'\initialize.ps1"')){$base.DeleteSubKeyTree($keyPath,$false);break}
                }
            }
        }finally{$base.Dispose()}
    }
    Write-Host 'Retired verified Tigirl legacy sample registration.'
}

if(!(Get-Command Remove-TigirlLegacyIdentity -ErrorAction SilentlyContinue)){. (Join-Path $PSScriptRoot '..\..\packaging\legacy_identity.ps1')}
$script:failures=0;$script:pending=0
function Note($s){Write-Host $s}
function Failed($s){$script:failures++;Note "失败/需人工处理：$s"}
function PlainPath([string]$Path) {
    $p=[IO.Path]::GetFullPath($Path)
    while($p){
        if(Test-Path -LiteralPath $p){
            if((Get-Item -LiteralPath $p -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw "拒绝处理链接或联接点：$p"}
        }
        $parent=Split-Path $p -Parent
        if($parent -eq $p){break};$p=$parent
    }
}
function RemoveTree([string]$Path) {
    if(!(Test-Path -LiteralPath $Path)){return}
    PlainPath $Path
    # Materialize and validate the full tree before removing anything. Never follow junctions.
    $files=New-Object 'System.Collections.Generic.List[string]'
    $dirs=New-Object 'System.Collections.Generic.List[string]'
    $stack=New-Object 'System.Collections.Generic.Stack[string]';$stack.Push($Path)
    while($stack.Count){
        $dir=$stack.Pop();PlainPath $dir;$dirs.Add($dir)
        foreach($item in Get-ChildItem -LiteralPath $dir -Force){
            if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "目录包含链接，已保留：$($item.FullName)"}
            if($item.PSIsContainer){$stack.Push($item.FullName)}else{$files.Add($item.FullName)}
        }
    }
    Note "程序目录：$Path（$($files.Count) 个文件）"
    if($CheckOnly){return}
    foreach($file in $files){RemoveOrQueue $file}
    for($i=$dirs.Count-1;$i -ge 0;$i--){RemoveOrQueue $dirs[$i]}
}
function RemoveOrQueue([string]$Path) {
    PlainPath $Path
    try {
        $item=Get-Item -LiteralPath $Path -Force
        if($item.PSIsContainer){[IO.Directory]::Delete($Path,$false)}
        else {if($item.Attributes -band [IO.FileAttributes]::ReadOnly){$item.Attributes=$item.Attributes -band (-bnot [IO.FileAttributes]::ReadOnly)};[IO.File]::Delete($Path)}
        Note "已删除：$Path"
    } catch {
        if(![TigirlCleanupNative]::MoveFileEx($Path,[NullString]::Value,4)){throw "删除及重启排队均失败：$Path (Win32 $([Runtime.InteropServices.Marshal]::GetLastWin32Error()))"}
        $script:pending++;Note "重启时删除：$Path"
    }
}
function RemoveKey($Base,[string]$Path){
    $key=$Base.OpenSubKey($Path)
    if(!$key){return};$key.Dispose()
    Note "注册项：$($Base.Name)\$Path"
    if(!$CheckOnly){$Base.DeleteSubKeyTree($Path,$false)}
}
function RemoveUserKeys($Base,[string]$Prefix) {
    foreach($key in @("Software\Classes\CLSID\$clsid","Software\Microsoft\CTF\TIP\$clsid","Software\Microsoft\Active Setup\Installed Components\$clsid")){
        RemoveKey $Base ($Prefix+$key)
    }
    # SortOrder entries have numbered names; match the exact CLSID value only.
    $path=$Prefix+'Software\Microsoft\CTF\SortOrder'
    $root=$Base.OpenSubKey($path)
    if(!$root){return};$root.Dispose()
    $queue=New-Object 'System.Collections.Generic.Stack[string]';$queue.Push($path)
    while($queue.Count){
        $current=$queue.Pop();$key=$Base.OpenSubKey($current)
        if(!$key){continue}
        try {
            $owned=([string]$key.GetValue('CLSID') -ieq $clsid)
            $children=$key.GetSubKeyNames()
        }finally{$key.Dispose()}
        if($owned){RemoveKey $Base $current}else{foreach($child in $children){$queue.Push($current+'\'+$child)}}
    }
}
function Main {
    $admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if(!$CheckOnly -and !$admin){throw '请通过 BAT 自动提权运行。'}
    Note '虎娘 / NativeTiger 安装清理工具'
    Note '保留所有用户的码表、拼音反查表和设置。不会关闭正在运行的应用。'
    if(!$CheckOnly){
        Note '将移除本机所有 Tigirl / NativeTiger 安装版本。完成后请重启，再安装新版。'
        if((Read-Host '输入 CLEAN 确认，直接回车取消') -cne 'CLEAN'){return 2}
    }
    if(!$CheckOnly){try{Remove-TigirlLegacyIdentity}catch{Failed $_}}
    $roots=@()
    foreach($pf in @($env:ProgramW6432,$env:ProgramFiles,${env:ProgramFiles(x86)})|Where-Object {$_}|Select-Object -Unique){
        foreach($name in @('Tigirl','NativeTiger')){$roots+=Join-Path $pf $name}
    }
    $roots=@($roots|Select-Object -Unique)
    Add-Type @'
using System.Runtime.InteropServices;
public static class TigirlCleanupNative {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool MoveFileEx(string a,string b,int f);
 [DllImport("input.dll",CharSet=CharSet.Unicode,SetLastError=true)] [return:MarshalAs(UnmanagedType.Bool)] public static extern bool InstallLayoutOrTip(string profile,uint flags);
}
'@
    if(!$CheckOnly){
        # User preference only; machine registration is removed independently below.
        if(![TigirlCleanupNative]::InstallLayoutOrTip("0804:$clsid$profile",1)){Note '当前账户输入法列表移除未成功；继续清理注册项，重启后刷新。'}
    }
    foreach($view in @([Microsoft.Win32.RegistryView]::Registry64,[Microsoft.Win32.RegistryView]::Registry32)){
        $machine=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,$view)
        try {
            foreach($key in @("SOFTWARE\Classes\CLSID\$clsid","SOFTWARE\Microsoft\CTF\TIP\$clsid","SOFTWARE\Microsoft\Active Setup\Installed Components\$clsid",
                'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Tigirl','SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Tigirl_is1',
                'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\NativeTiger','SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\NativeTiger_is1')){
                try{RemoveKey $machine $key}catch{Failed $_}
            }
            foreach($protocol in @('nativetiger','tigirl')){
                $path="SOFTWARE\Classes\$protocol";$k=$machine.OpenSubKey($path+'\shell\open\command')
                if($k){try{$command=[string]$k.GetValue('')}finally{$k.Dispose()}
                    $owned=$false;foreach($root in $roots){if($command.StartsWith('"'+$root+'\',[StringComparison]::OrdinalIgnoreCase)){$owned=$true}}
                    if($owned){try{RemoveKey $machine $path}catch{Failed $_}}else{Failed "协议 $protocol 不指向已知安装目录，已保留：$command"}
                }
            }
        } finally {$machine.Dispose()}
        $users=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::Users,$view)
        try {foreach($sid in $users.GetSubKeyNames()|Where-Object {$_ -match '^S-1-5-21-(\d+-){3}\d+$|^S-1-12-1-(\d+-){3}\d+$'}){
            try{RemoveUserKeys $users ($sid+'\')}catch{Failed $_}
        }}finally{$users.Dispose()}
    }
    # Delete only links whose resolved target belongs to a known installation root.
    $programs=@([Environment]::GetFolderPath('CommonPrograms'),[Environment]::GetFolderPath('Programs'))
    $shell=New-Object -ComObject WScript.Shell
    try {foreach($program in $programs){foreach($folder in @('虎娘','Tigirl','NativeTiger','原生虎码')){
        $dir=Join-Path $program $folder
        if(!(Test-Path -LiteralPath $dir)){continue}
        try {
            PlainPath $dir
            foreach($file in Get-ChildItem -LiteralPath $dir -Filter '*.lnk' -File){
                PlainPath $file.FullName;$link=$shell.CreateShortcut($file.FullName)
                try{$target=$link.TargetPath}finally{[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($link)}
                foreach($root in $roots){if($target.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){
                    Note "快捷方式：$($file.FullName)";if(!$CheckOnly){RemoveOrQueue $file.FullName};break
                }}
            }
            if(!$CheckOnly -and !(Get-ChildItem -LiteralPath $dir -Force|Select-Object -First 1)){[IO.Directory]::Delete($dir)}
        }catch{Failed $_}
    }}}finally{[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)}
    $legacyPaths=@(Get-TigirlLegacyPaths)
    foreach($root in $roots){
        if(@($legacyPaths|Where-Object {$_.path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)}).Count){Failed "旧注册仍引用此目录，已保留：$root";continue}
        try{RemoveTree $root}catch{Failed $_}
    }
    Note '未登录账户的用户级缓存未加载；机器输入法注册已处理，用户数据始终保留。'
    if($CheckOnly){Note '仅检查完成，未执行清理。';return $(if($script:failures){1}else{0})}
    if($script:failures){Note "有 $script:failures 项未完成，请保留日志联系维护者。";return 1}
    if($script:pending){Note "清理已执行，$script:pending 项将在重启时删除。请重启后再安装。";return 3010}
    Note '清理完成。请重启后再安装新版。';return 0
}
# Tests dot-source only definitions, without executing cleanup.
if($MyInvocation.InvocationName -ne '.'){
    $log=Join-Path ([IO.Path]::GetTempPath()) ('Tigirl-Cleanup-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')+'.log')
    $started=$false
    try{Start-Transcript -LiteralPath $log|Out-Null;$started=$true;$code=Main}
    catch{Note $_; $code=1}
    finally{if($started){Stop-Transcript|Out-Null};Note "日志：$log"}
    exit $code
}
