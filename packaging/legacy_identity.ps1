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
