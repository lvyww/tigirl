param([switch]$Elevated,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\common.ps1"
function Initialize-InstalledUser([string]$Directory) {
    $process=Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -Wait -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$Directory+'\initialize.ps1"'))
    if($process.ExitCode) {
        Write-Warning 'User initialization did not complete. The new machine registration remains active; the user-data transaction restores its own changes.'
        return $false
    }
    return $true
}
try {
    Assert-X64System
    $manifest=Assert-Package $PSScriptRoot
    $generation=(Get-FileHash -LiteralPath "$PSScriptRoot\manifest.json").Hash.Substring(0,16).ToLowerInvariant()
    $destination=Join-Path $NativeTigerInstallRoot "versions\$generation"
    $previous=$null
    if(Test-Path -LiteralPath $NativeTigerRecord){$previous=Get-Content -LiteralPath $NativeTigerRecord -Raw|ConvertFrom-Json;Assert-Package $previous.directory|Out-Null}
    foreach($pair in @(@('Registry64','x64'),@('Registry32','x86'))){
        $registered=Get-ComPath $pair[0]
        if($registered -and (!$previous -or $registered -ne (Get-PackageDll $previous.directory $pair[1]))){throw 'An installation outside this package owns the input-method registration. Uninstall it first.'}
    }
    if(Test-Path 'HKLM:\SOFTWARE\Classes\nativetiger'){
        $command=(Get-Item 'HKLM:\SOFTWARE\Classes\nativetiger\shell\open\command').GetValue('')
        if(!$previous -or $command -ne ('"'+(Get-PackageTool $previous.directory)+'" --uri "%1"')){throw 'Another application owns the nativetiger URI protocol.'}
    }
    if($CheckOnly){@{status='validated';version=$manifest.version;destination=$destination}|ConvertTo-Json;exit 0}
    if(!(Test-Administrator)) {
        if($Elevated){throw 'Administrator privileges were not granted.'}
        $child=Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -Wait -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$PSCommandPath+'"'),'-Elevated')
        if($child.ExitCode){throw 'Machine installation did not complete.'}
        # This process retains the original user's identity across UAC. A user-data
        # failure must not roll back a machine registration that already committed.
        [void](Initialize-InstalledUser $destination)
        exit 0
    }
    . "$PSScriptRoot\data.ps1"
    Assert-PlainTree $NativeTigerInstallRoot
    New-Item -ItemType Directory -Force $destination|Out-Null
    foreach($file in @($manifest.files)+@([pscustomobject]@{path='manifest.json';sha256=(Get-FileHash "$PSScriptRoot\manifest.json").Hash})) {
        $target=Join-Path $destination $file.path
        New-Item -ItemType Directory -Force (Split-Path $target -Parent)|Out-Null
        if(Test-Path -LiteralPath $target){if((Get-FileHash -LiteralPath $target).Hash -ne $file.sha256){throw 'Immutable installation collision.'}}
        else{Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file.path) -Destination $target}
    }
    Assert-Package $destination|Out-Null
    $registrationStarted=$false
    try {
        $registrationStarted=$true
        Invoke-Registration $destination
        Set-MachineEntries $destination $manifest.version
        if($previous){Copy-Item -LiteralPath $NativeTigerRecord -Destination (Join-Path $NativeTigerInstallRoot ('previous-'+[guid]::NewGuid().ToString('N')+'.json'))}
        $record=[ordered]@{directory=$destination;version=$manifest.version;generation=$generation;previous=$(if($previous){$previous.directory}else{$null})}
        $temporary=$NativeTigerRecord+'.tmp';$record|ConvertTo-Json|Set-Content -LiteralPath $temporary -Encoding UTF8
        if(Test-Path -LiteralPath $NativeTigerRecord){[IO.File]::Replace($temporary,$NativeTigerRecord,[NullString]::Value)}else{[IO.File]::Move($temporary,$NativeTigerRecord)}
    }catch {
        if($registrationStarted){
            if($previous){Invoke-Registration $previous.directory;Set-MachineEntries $previous.directory $previous.version}
            else{Invoke-Registration $destination -Remove;Remove-MachineEntries}
        }
        throw
    }
    # -Elevated is the machine-only child. An explicitly elevated launch
    # initializes that account; ordinary double-click preserves the user above.
    if(!$Elevated){[void](Initialize-InstalledUser $destination);exit 0}
    Write-Output "Installed machine files: $destination"
}catch{Write-Error $_;exit 1}
