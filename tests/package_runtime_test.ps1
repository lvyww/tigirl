param([string]$Package)
$ErrorActionPreference='Stop'
$Package=[IO.Path]::GetFullPath($Package)
. "$Package\common.ps1"
$manifest=Assert-Package $Package
$fixture=Join-Path $env:TEMP ('NativeTiger-release-'+[guid]::NewGuid().ToString('N'))
$env:NATIVE_TIGER_USER_ROOT=Join-Path $fixture 'Tigirl'
$root=$env:NATIVE_TIGER_USER_ROOT
function Check($Condition,$Message){if(!$Condition){throw $Message}}
function Tool($Action,$Value=''){
    $p=Start-Process -FilePath "$Package\shared\Tigirl.exe" -ArgumentList @('--menu-action',$Action,('"'+$Value+'"')) -Wait -PassThru
    Check ($p.ExitCode -eq 0) "Action failed: $Action"
}
function Generation($Name){return [IO.File]::ReadAllText("$root\schemas\$Name\current.txt")}
try {
    $archFiles=@(Get-ChildItem "$Package\x64","$Package\x86" -File -Recurse)
    Check ($archFiles.Count -eq 2 -and @($archFiles|Where-Object Name -ne 'Tigirl.dll').Count -eq 0) 'Architecture directories contain duplicated shared runtime files'
    foreach($relative in @('Tigirl.exe','Tigirl.Import.exe','Tigirl.Reminder.exe','tiger-v2.tcd','Models\sentence-fivegram.klm','字体\LXGWWenKaiGBScreen.ttf')){
        Check (Test-Path -LiteralPath (Join-Path "$Package\shared" $relative) -PathType Leaf) "Missing shared runtime file: $relative"
    }
    & "$Package\initialize.ps1" -Quiet -NoEnable
    Check (Test-Path "$root\码表\虎码字词") 'Missing default sources'
    Check (Test-Path "$root\拼音反查码表") 'Missing pinyin source'
    Check (Test-Path "$root\schemas\虎整句\current.txt") 'Sentence scheme not compiled'
    Check ([IO.File]::ReadAllText("$root\config.txt").Contains("Ctrl+空格切换中英文`t否")) 'First install must leave Ctrl+Space to Windows'
    $first=Generation '虎码字词'
    Tool 'use' '虎码字词'
    Check ((Generation '虎码字词') -eq $first) 'Unmodified sources were rebuilt'
    $pinyinFile=Join-Path $root '拼音反查码表\release-test.txt'
    [IO.File]::WriteAllText($pinyinFile,"ceshi 测试`r`n",[Text.UTF8Encoding]::new($true))
    Tool 'reload'
    Check ((Generation '虎码字词') -ne $first) 'Pinyin change did not invalidate cache'
    $second=Generation '虎码字词'
    $savedConfiguration=[IO.File]::ReadAllText("$root\config.txt")
    $metadata="$root\schemas\虎码字词\source-cache.txt"
    [IO.File]::SetAttributes($metadata,[IO.FileAttributes]::ReadOnly)
    $badChange="$root\码表\虎码字词\temporary-test.txt"
    [IO.File]::WriteAllText($badChange,"ab 新内容`r`n",[Text.UTF8Encoding]::new($true))
    try {
        $failed=Start-Process -FilePath "$Package\shared\Tigirl.exe" -ArgumentList '--initialize' -Wait -PassThru
        Check ($failed.ExitCode -ne 0) 'Failed publication was not reported'
        Check ((Generation '虎码字词') -eq $second) 'Failure changed selected generation'
        Check ([IO.File]::ReadAllText("$root\config.txt") -eq $savedConfiguration) 'Failure changed selected scheme'
    }finally{[IO.File]::SetAttributes($metadata,[IO.FileAttributes]::Normal);Remove-Item -LiteralPath $badChange}
    Tool 'use' '虎整句'
    Check ([IO.File]::ReadAllText("$root\config.txt").Contains("当前码表`t虎整句")) 'Selection was not saved'
    Tool 'recent'
    Check ([IO.File]::ReadAllText("$root\config.txt").Contains("当前码表`t虎码字词")) 'Recent switch failed'
    Check ((Generation '虎码字词') -eq $second) 'Recent selection rebuilt unchanged data'
    # Added folder becomes selectable without a manager/import dialog.
    New-Item -ItemType Directory -Force "$root\码表\新增方案"|Out-Null
    [IO.File]::WriteAllText("$root\码表\新增方案\词条.txt","ab 新增`r`n",[Text.UTF8Encoding]::new($true))
    Tool 'use' '新增方案'
    Check (Test-Path "$root\schemas\新增方案\current.txt") 'New folder was not compiled'
    Move-Item -LiteralPath "$root\码表\新增方案" -Destination "$fixture\removed-source"
    Tool 'reload'
    Check (![IO.File]::ReadAllText("$root\config.txt").Contains("当前码表`t新增方案")) 'Deleted scheme did not fall back'
    # Legacy configurable roots must not redirect runtime reads.
    Add-Content -LiteralPath "$root\config.txt" -Value "码表存储位置`tC:\does-not-exist`r`n拼音反查目录`tC:\does-not-exist" -Encoding UTF8
    Tool 'reload'
    Check (Test-Path "$root\installed-data-version.txt") 'Initialization not recorded'
    [ordered]@{status='passed';version=$manifest.version;package=$Package;checks=@('manifest and both architecture loaders','single-copy shared runtime layout','first-user initialization','default source copy','sentence sidecars','cache reuse','pinyin invalidation','scheme selection','recent selection','new source folder','deleted source fallback','fixed runtime paths','default system Ctrl+Space','failed publication preserves selection');machine_registration_tested=$false;uwp_uri_activation_tested=$false}|ConvertTo-Json -Depth 4
}finally{Remove-Item Env:NATIVE_TIGER_USER_ROOT -ErrorAction SilentlyContinue;if(Test-Path -LiteralPath $fixture){Remove-Item -LiteralPath $fixture -Recurse -Force}}
