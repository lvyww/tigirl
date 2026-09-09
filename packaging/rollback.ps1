param([switch]$Elevated,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\common.ps1"
try {
    Assert-X64System
    $record=Get-Content -LiteralPath $NativeTigerRecord -Raw|ConvertFrom-Json
    if(!$record.previous){throw 'No previous package is available.'}
    $previous=[IO.Path]::GetFullPath($record.previous)
    if(!$previous.StartsWith($NativeTigerInstallRoot+'\versions\',[StringComparison]::OrdinalIgnoreCase)){throw 'Invalid rollback destination.'}
    $manifest=Assert-Package $previous
    foreach($pair in @(@('Registry64','x64'),@('Registry32','x86'))){if((Get-ComPath $pair[0]) -ne (Get-PackageDll $record.directory $pair[1])){throw 'Registration ownership changed.'}}
    if($CheckOnly){@{status='validated';rollback_to=$previous;preserve_user_data=$true}|ConvertTo-Json;exit 0}
    if(!(Test-Administrator)){
        if($Elevated){throw 'Administrator privileges were not granted.'}
        $child=Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -Wait -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$PSCommandPath+'"'),'-Elevated');exit $child.ExitCode
    }
    try {
        Invoke-Registration $previous
        Set-MachineEntries $previous $manifest.version
        $next=@{directory=$previous;version=$manifest.version;generation=(Split-Path $previous -Leaf);previous=$record.directory}
        $temp=$NativeTigerRecord+'.tmp';$next|ConvertTo-Json|Set-Content -LiteralPath $temp -Encoding UTF8
        [IO.File]::Replace($temp,$NativeTigerRecord,[NullString]::Value)
    }catch{Invoke-Registration $record.directory;Set-MachineEntries $record.directory $record.version;throw}
    Write-Output 'Previous program version restored. User data retained. Restart applications.'
}catch{Write-Error $_;exit 1}
