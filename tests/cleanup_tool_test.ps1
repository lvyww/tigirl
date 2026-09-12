$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot '..\tools\cleanup\cleanup.ps1'
$errors=$null;$tokens=$null
[void][Management.Automation.Language.Parser]::ParseFile($source,[ref]$tokens,[ref]$errors)
if($errors){throw ($errors|Out-String)}
. $source
$fixture=Join-Path $env:TEMP ('tigirl-cleanup-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $fixture|Out-Null
function Assert($ok,$message){if(!$ok){throw $message}}
try {
    # No product paths, machine registry or actual reboot queues are modified.
    Add-Type 'public static class TigirlCleanupNative { public static int Queued; public static bool MoveFileEx(string a,string b,int f){Queued++;return true;} }'
    $tree=Join-Path $fixture 'program'
    New-Item -ItemType Directory "$tree\versions\abc"|Out-Null
    [IO.File]::WriteAllText("$tree\versions\abc\Tigirl.dll",'fixture')
    $CheckOnly=$true;RemoveTree $tree
    Assert (Test-Path "$tree\versions\abc\Tigirl.dll") 'Dry run deleted a file'
    $CheckOnly=$false;RemoveTree $tree
    Assert (!(Test-Path $tree)) 'Normal tree not removed'
    RemoveTree $tree # idempotent
    [IO.File]::WriteAllText("$fixture\locked.dll",'fixture')
    $handle=[IO.File]::Open("$fixture\locked.dll",'Open','Read','Read')
    try {RemoveOrQueue "$fixture\locked.dll";Assert ([TigirlCleanupNative]::Queued -eq 1) 'Locked file not deferred'}finally{$handle.Dispose()}
    $outside=Join-Path $fixture 'outside';New-Item -ItemType Directory $outside|Out-Null
    [IO.File]::WriteAllText("$outside\keep.txt",'keep')
    New-Item -ItemType Directory $tree|Out-Null
    New-Item -ItemType Junction -Path "$tree\junction" -Target $outside|Out-Null
    $rejected=$false;try{RemoveTree $tree}catch{$rejected=$true}
    Assert $rejected 'Junction accepted'
    Assert (Test-Path "$outside\keep.txt") 'Junction target modified'
    [IO.Directory]::Delete("$tree\junction")
    # Registry behavior under an isolated HKCU fixture, never actual TSF state.
    $reg="Software\TigirlCleanupTest-"+[guid]::NewGuid().ToString('N')
    $base=[Microsoft.Win32.Registry]::CurrentUser
    try {
        $k=$base.CreateSubKey($reg+'\keep');$k.Dispose()
        $k=$base.CreateSubKey($reg+'\Software\Microsoft\CTF\SortOrder\0');$k.SetValue('CLSID',$clsid);$k.Dispose()
        $k=$base.CreateSubKey($reg+'\Software\Microsoft\CTF\SortOrder\1');$k.SetValue('CLSID','{00000000-0000-0000-0000-000000000000}');$k.Dispose()
        $CheckOnly=$true;RemoveUserKeys $base ($reg+'\')
        $k=$base.OpenSubKey($reg+'\Software\Microsoft\CTF\SortOrder\0');Assert ($null -ne $k) 'Dry run changed registry';$k.Dispose()
        $CheckOnly=$false;RemoveUserKeys $base ($reg+'\')
        $k=$base.OpenSubKey($reg+'\Software\Microsoft\CTF\SortOrder\0');Assert ($null -eq $k) 'Owned sort entry remains'
        $k=$base.OpenSubKey($reg+'\Software\Microsoft\CTF\SortOrder\1');Assert ($null -ne $k) 'Other IME entry removed';$k.Dispose()
    }finally{$base.DeleteSubKeyTree($reg,$false)}
    # Exercise the elevation branch with a stub: parse the exact generated child
    # command, including a path containing spaces, quotes and shell metacharacters.
    $bat=[IO.File]::ReadAllText((Join-Path $PSScriptRoot '..\Tigirl-Cleanup.bat'),[Text.Encoding]::UTF8)
    $encoded=[regex]::Match($bat,'-EncodedCommand ([A-Za-z0-9+/=]+)').Groups[1].Value
    $bootstrap=[Text.Encoding]::Unicode.GetString([Convert]::FromBase64String($encoded))
    function Start-Process($FilePath,$Verb,$ArgumentList,[switch]$Wait,[switch]$PassThru){
        Assert ($Verb -eq 'RunAs') 'Missing UAC elevation'
        $encoded=($ArgumentList -split ' ')[-1]
        $code=[Text.Encoding]::Unicode.GetString([Convert]::FromBase64String($encoded))
        $errors=$null;$tokens=$null
        [void][Management.Automation.Language.Parser]::ParseInput($code,[ref]$tokens,[ref]$errors)
        Assert (!$errors) 'Elevated command has invalid syntax'
        Assert ($code.Contains($env:TIGIRL_CLEAN_SELF.Replace("'","''"))) 'Elevated path was not safely quoted'
        Assert ($code.Contains("-CheckOnly:")) 'Child loader lost check handling'
        return [pscustomobject]@{ExitCode=0}
    }
    $env:TIGIRL_CLEAN_SELF="C:\a b\quote' & (test)\Tigirl-Cleanup.bat"
    $env:TIGIRL_CLEAN_CHECK=''
    $bootstrap=$bootstrap.Replace('if($admin -or $check)','if($false)').Replace('exit $child.ExitCode','if($child.ExitCode){throw "Mock elevation failed"}').Replace('exit 1','throw "Bootstrap failed"')
    & ([scriptblock]::Create($bootstrap))
    Remove-Item Env:TIGIRL_CLEAN_SELF,Env:TIGIRL_CLEAN_CHECK -ErrorAction SilentlyContinue
    Write-Output 'PASS: syntax, dry run, idempotence, locked file deferral, junction rejection, exact registry ownership, elevation command quoting'
}finally{Remove-Item -LiteralPath $fixture -Recurse -Force}
