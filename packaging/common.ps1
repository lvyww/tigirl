. (Join-Path $PSScriptRoot 'legacy_identity.ps1')
$ErrorActionPreference='Stop'
$NativeTigerClsid='{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}'
$NativeTigerProfile='{43201C7B-F615-469D-9D54-906D9270975E}'
$NativeTigerInstallRoot=Join-Path $env:ProgramFiles 'Tigirl'
function Get-SystemArchitecture { return [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString() }
function Get-PackageNativeArchitecture([string]$Directory) {
    # Select by manifest, not file presence: a missing DLL must not silently
    # switch the identity used for repair/uninstall to another architecture.
    if((Get-SystemArchitecture) -eq 'Arm64') {
        $path=Join-Path $Directory 'manifest.json'
        if(Test-Path -LiteralPath $path -PathType Leaf) {
            try {
                $manifest=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json
                if(@($manifest.files|Where-Object {$_.path -eq 'arm64\Tigirl.dll'}).Count){return 'arm64'}
            } catch {}
        }
    }
    return 'x64'
}
function Get-PackageDll([string]$Directory,[string]$Architecture) {
    if($Architecture -eq 'native'){$Architecture=Get-PackageNativeArchitecture $Directory}
    return Join-Path $Directory ($Architecture+'\Tigirl.dll')
}
function Get-PackageTool([string]$Directory) {
    $shared=Join-Path $Directory 'shared\Tigirl.exe'
    $manifest=Join-Path $Directory 'manifest.json'
    if(Test-Path -LiteralPath $manifest -PathType Leaf){
        try{
            $files=@((Get-Content -LiteralPath $manifest -Raw|ConvertFrom-Json).files)
            if(@($files|Where-Object {$_.path -eq 'shared\Tigirl.exe'}).Count){return $shared}
        }catch{}
    }
    return Join-Path $Directory 'x64\Tigirl.exe'
}
$NativeTigerRecord=Join-Path $NativeTigerInstallRoot 'install.json'
function Assert-SupportedSystem {
    $architecture=Get-SystemArchitecture
    if($architecture -notin @('X64','Arm64') -or ![Environment]::Is64BitProcess){throw 'This package requires 64-bit Windows and 64-bit PowerShell.'}
    if([Environment]::OSVersion.Version.Build -lt 17763){throw 'Windows 10 version 1809 or later is required.'}
    if($architecture -eq 'Arm64' -and [Environment]::OSVersion.Version.Build -lt 22000){throw 'Windows 11 or later is required on ARM64 for the shared x64 desktop tools.'}
}
function Assert-InstallArchitecture([string]$Directory) {
    Assert-SupportedSystem
    if((Get-SystemArchitecture) -eq 'Arm64' -and (Get-PackageNativeArchitecture $Directory) -ne 'arm64'){
        throw 'The package is missing its ARM64X input-method component.'
    }
}
function Get-PackageLoadChecks([string]$Directory) {
    @('x64','x86')|ForEach-Object {[pscustomobject]@{probe=$_;dll=$_}}
    if((Get-SystemArchitecture) -eq 'Arm64' -and (Get-PackageNativeArchitecture $Directory) -eq 'arm64'){
        # Both native ARM64 and emulated x64 applications must load ARM64X.
        @('arm64','x64')|ForEach-Object {[pscustomobject]@{probe=$_;dll='arm64'}}
    }
}
function Test-Administrator { return ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) }
function Get-ComPath([string]$View) {
    $base=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,[Microsoft.Win32.RegistryView]::$View)
    try{$key=$base.OpenSubKey("SOFTWARE\Classes\CLSID\$NativeTigerClsid\InprocServer32");if($key){try{return $key.GetValue('')}finally{$key.Dispose()}}}finally{$base.Dispose()}
}
function Invoke-Registration([string]$Directory,[switch]$Remove) {
    # Remove shared profile only once, after removing x86 COM registration.
    if($Remove){
        $base=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,[Microsoft.Win32.RegistryView]::Registry32)
        try{$base.DeleteSubKeyTree("SOFTWARE\Classes\CLSID\$NativeTigerClsid",$false)}finally{$base.Dispose()}
        $p=Start-Process "$env:windir\System32\regsvr32.exe" -ArgumentList @('/s','/u',('"'+(Get-PackageDll $Directory 'native')+'"')) -Wait -PassThru
        if($p.ExitCode){throw 'TSF unregistration failed.'};return
    }
    Assert-InstallArchitecture $Directory
    foreach($pair in @(@('System32','native'),@('SysWOW64','x86'))){
        $p=Start-Process "$env:windir\$($pair[0])\regsvr32.exe" -ArgumentList @('/s',('"'+(Get-PackageDll $Directory $pair[1])+'"')) -Wait -PassThru
        if($p.ExitCode){throw "TSF registration failed: $($pair[1])"}
    }
    if((Get-ComPath 'Registry64') -ne (Get-PackageDll $Directory 'native') -or (Get-ComPath 'Registry32') -ne (Get-PackageDll $Directory 'x86')){throw 'COM registration verification failed.'}
}
function Assert-Package([string]$Directory) {
    . (Join-Path $Directory 'data.ps1')
    Assert-PlainTree $Directory
    $manifest=Get-Content -LiteralPath "$Directory\manifest.json" -Raw|ConvertFrom-Json
    if($manifest.version -notmatch '^\d+\.\d+\.\d+\.\d+$'){throw 'Invalid package version.'}
    foreach($file in $manifest.files){
        if($file.path -match '(^[\\/]|:|(^|[\\/])\.\.([\\/]|$))'){throw 'Invalid package path.'}
        if((Get-FileHash -LiteralPath (Join-Path $Directory $file.path) -Algorithm SHA256).Hash -ne $file.sha256){throw "Package hash mismatch: $($file.path)"}
    }
    foreach($check in (Get-PackageLoadChecks $Directory)){
        $arch=$check.probe+'->'+$check.dll
        $dll=Get-PackageDll $Directory $check.dll;$probe=Join-Path $Directory ('verify_'+$check.probe+'.exe')
        $process=[Diagnostics.Process]::new()
        try{
            $process.StartInfo.FileName=$probe
            $process.StartInfo.Arguments='"'+$dll+'"'
            $process.StartInfo.UseShellExecute=$false
            $process.StartInfo.CreateNoWindow=$true
            $process.StartInfo.RedirectStandardOutput=$true
            if(!$process.Start()){throw "Cannot start load verification: $arch"}
            $output=$process.StandardOutput.ReadToEnd()
            $process.WaitForExit()
            if($process.ExitCode -ne 0){throw "Load verification process failed: $arch ($($process.ExitCode))"}
            $loaded=$output|ConvertFrom-Json
            if(!$loaded.loaded -or !$loaded.class_instance){throw "Load verification failed: $arch"}
        }finally{$process.Dispose()}
    }
    return $manifest
}
function Set-MachineEntries([string]$Directory,[string]$Version) {
    $protocol='HKLM:\SOFTWARE\Classes\nativetiger'
    New-Item -Path "$protocol\shell\open\command" -Force|Out-Null
    Set-Item -LiteralPath $protocol -Value 'URL:Tigirl actions'
    New-ItemProperty -LiteralPath $protocol -Name 'URL Protocol' -Value '' -Force|Out-Null
    Set-Item -LiteralPath "$protocol\shell\open\command" -Value ('"'+(Get-PackageTool $Directory)+'" --uri "%1"')
    $active="HKLM:\SOFTWARE\Microsoft\Active Setup\Installed Components\$NativeTigerClsid"
    New-Item -Path $active -Force|Out-Null
    Set-ItemProperty $active -Name Version -Value ($Version.Replace('.',','))
    Set-ItemProperty $active -Name StubPath -Value ('"'+$env:windir+'\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "'+$Directory+'\initialize.ps1" -Quiet -SkipConflicts -RequireStandardUser')
    if(Test-Path -LiteralPath (Join-Path $Directory 'setup\gui.txt')){return}
    $uninstall='HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Tigirl'
    New-Item -Path $uninstall -Force|Out-Null
    Set-ItemProperty $uninstall -Name DisplayName -Value '虎娘'
    Set-ItemProperty $uninstall -Name DisplayVersion -Value $Version
    Set-ItemProperty $uninstall -Name DisplayIcon -Value ((Get-PackageTool $Directory)+',0')
    Set-ItemProperty $uninstall -Name UninstallString -Value ('"'+$env:windir+'\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "'+$Directory+'\uninstall.ps1"')
}
function Remove-MachineEntries {
    foreach($key in @('HKLM:\SOFTWARE\Classes\nativetiger',"HKLM:\SOFTWARE\Microsoft\Active Setup\Installed Components\$NativeTigerClsid",'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Tigirl')){if(Test-Path $key){Remove-Item -LiteralPath $key -Recurse}}
}
