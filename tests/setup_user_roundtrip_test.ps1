param([Parameter(Mandatory)][string]$Package)
$ErrorActionPreference='Stop'
$fixture=Join-Path $env:TEMP ('Tigirl-user-roundtrip-'+[guid]::NewGuid().ToString('N'))
$saved=$env:NATIVE_TIGER_USER_ROOT
$env:NATIVE_TIGER_USER_ROOT=Join-Path $fixture 'Tigirl'
$root=$env:NATIVE_TIGER_USER_ROOT
New-Item $root -ItemType Directory -Force|Out-Null
$legacy=Join-Path $fixture 'NativeTiger'
New-Item "$legacy\码表","$legacy\拼音反查码表" -ItemType Directory -Force|Out-Null
[IO.File]::WriteAllText("$legacy\码表\legacy-only.txt","ab`t旧数据")
[IO.File]::WriteAllText("$legacy\拼音反查码表\legacy-only.txt","ceshi 测试")
$legacyHash=(Get-FileHash "$legacy\码表\legacy-only.txt").Hash
$config="码表存储位置`t$legacy\码表`r`n拼音反查目录`t$legacy\拼音反查码表`r`n当前码表`t虎码字词`r`nCtrl+空格切换中英文`t否`r`n"
[IO.File]::WriteAllText("$root\config.txt",$config,[Text.UTF8Encoding]::new($true))
$hash=(Get-FileHash "$root\config.txt").Hash
function Check($Value,$Message){if(!$Value){throw $Message}}
function Run($Action){
 $p=Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$Package+'\initialize.ps1"'),'-NoEnable','-Quiet','-NoDialogs','-SkipConflicts','-Transaction','test-transaction','-TransactionAction',$Action) -RedirectStandardOutput "$fixture\$Action.out" -RedirectStandardError "$fixture\$Action.err" -PassThru -Wait
 Check ($p.ExitCode -eq 0) "User transaction action failed: $Action / $(Get-Content "$fixture\$Action.err" -Raw)"
}
try{
 Run 'Initialize'
 Check (!(Test-Path "$root\码表\legacy-only.txt") -and !(Test-Path "$root\拼音反查码表\legacy-only.txt")) 'Legacy external data was migrated'
 Check (!(Test-Path "$root\fixed-path-migrated.txt")) 'Legacy migration marker was created'
 Check ((Get-FileHash "$legacy\码表\legacy-only.txt").Hash -eq $legacyHash) 'Legacy source changed'
 Check (Test-Path "$root\.setup-user-transaction.json") 'Pending journal missing'
 Check (Test-Path "$root\schemas\虎整句\current.txt") 'Compiled sentence descriptor missing'
 $pending=Get-Content -LiteralPath "$root\.setup-user-transaction.json" -Raw|ConvertFrom-Json
 $copied=Get-Content -LiteralPath (Join-Path $pending.backup 'files.json') -Raw|ConvertFrom-Json
 $newFiles=@($copied|Where-Object {!$_.Backup})
 Check ($newFiles.Count -gt 0) 'Transaction recorded no newly copied files'
 $copiedPaths=@{};foreach($item in $copied){$copiedPaths[$item.Target]=$true}
 # The compiler opens each schema's UserStore, creating an empty journal and
 # stable lock. These are not package sources and must not be swept by rollback.
 $generatedPaths=@{}
 foreach($descriptor in Get-ChildItem "$root\schemas" -File -Filter current.txt -Recurse){
  $journal=Join-Path (Join-Path "$root\码表" $descriptor.Directory.Name) '用户调整.txt'
  $generatedPaths[$journal]=$true;$generatedPaths[$journal+'.lock']=$true
 }
 Run 'Rollback'
 Check ((Get-FileHash "$root\config.txt").Hash -eq $hash) 'Rollback changed preexisting configuration'
 Check (!(Test-Path "$root\installed-data-version.txt")) 'Rollback retained success marker'
 Check (!(Test-Path "$root\schemas\虎整句\current.txt")) 'Rollback retained published descriptor'
 foreach($item in $newFiles){
  Check (!(Test-Path -LiteralPath $item.Target)) ("Rollback retained newly copied file: "+$item.Target)
 }
 $remaining=@(Get-ChildItem "$root\码表" -File -Recurse)
 foreach($file in $remaining){
  Check ($generatedPaths.ContainsKey($file.FullName) -and $file.Length -eq 0 -and !$copiedPaths.ContainsKey($file.FullName)) ("Rollback retained unexpected source file: "+$file.FullName)
 }
 Write-Output ("ROLLBACK_VALIDATED copied_new_files={0} allowed_empty_journal_files={1}" -f $newFiles.Count,$remaining.Count)
 Run 'Initialize'
 Run 'Complete'
 Check (!(Test-Path "$root\.setup-user-transaction.json")) 'Commit retained journal'
 Check (Test-Path "$root\schemas\虎整句\current.txt") 'Commit lost compiled data'
 Run 'Rollback'
 Check (Test-Path "$root\schemas\虎整句\current.txt") 'Rollback without pending transaction changed committed data'
 'PASS: real compiler initialization, pending rollback, original config, copied-file/descriptor cleanup, commit, idempotence.'
}finally{
 if(Test-Path "$root\.setup-user-transaction.json"){
  Copy-Item "$root\.setup-user-transaction.json" "$PSScriptRoot\..\build\failed-user-transaction.json" -Force
  $j=Get-Content "$root\.setup-user-transaction.json" -Raw|ConvertFrom-Json
  if(Test-Path (Join-Path $j.backup 'files.json')){Copy-Item (Join-Path $j.backup 'files.json') "$PSScriptRoot\..\build\failed-user-files.json" -Force}
 }
 $env:NATIVE_TIGER_USER_ROOT=$saved;Remove-Item $fixture -Recurse -Force
}
