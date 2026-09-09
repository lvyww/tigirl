param([switch]$Elevated,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\common.ps1"
try {
    Assert-X64System
    $record=Get-Content -LiteralPath $NativeTigerRecord -Raw|ConvertFrom-Json
    Assert-Package $record.directory|Out-Null
    foreach($pair in @(@('Registry64','x64'),@('Registry32','x86'))){if((Get-ComPath $pair[0]) -ne (Get-PackageDll $record.directory $pair[1])){throw 'Registration ownership changed; refusing to unregister another installation.'}}
    if($CheckOnly){@{status='validated';preserve_user_data=$true;directory=$record.directory}|ConvertTo-Json;exit 0}
    if(!(Test-Administrator)){
        if($Elevated){throw 'Administrator privileges were not granted.'}
        $child=Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -Wait -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$PSCommandPath+'"'),'-Elevated');exit $child.ExitCode
    }
    Invoke-Registration $record.directory -Remove
    if((Get-ComPath 'Registry64') -or (Get-ComPath 'Registry32')){throw 'COM registration remains.'}
    Remove-MachineEntries
    Move-Item -LiteralPath $NativeTigerRecord -Destination (Join-Path $NativeTigerInstallRoot ('uninstalled-'+[guid]::NewGuid().ToString('N')+'.json'))
    Add-Type -AssemblyName System.Windows.Forms
    [Windows.Forms.MessageBox]::Show('已卸载输入法注册。用户数据和程序版本文件已保留；请重开正在使用的程序。','虎娘')|Out-Null
}catch{Write-Error $_;exit 1}
