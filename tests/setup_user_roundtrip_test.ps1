param([Parameter(Mandatory)][string]$Package)
$ErrorActionPreference='Stop'
$fixture=Join-Path $env:TEMP ('Tigirl-user-roundtrip-'+[guid]::NewGuid().ToString('N'))
$saved=$env:NATIVE_TIGER_USER_ROOT
$env:NATIVE_TIGER_USER_ROOT=Join-Path $fixture 'NativeTiger'
$root=$env:NATIVE_TIGER_USER_ROOT
New-Item $root -ItemType Directory -Force|Out-Null
$config="当前码表`t虎码字词`r`nCtrl+空格切换中英文`t否`r`n"
[IO.File]::WriteAllText("$root\config.txt",$config,[Text.UTF8Encoding]::new($true))
$hash=(Get-FileHash "$root\config.txt").Hash
function Check($Value,$Message){if(!$Value){throw $Message}}
function Run($Action){
 $p=Start-Process "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$Package+'\initialize.ps1"'),'-NoEnable','-Quiet','-NoDialogs','-SkipConflicts','-Transaction','test-transaction','-TransactionAction',$Action) -RedirectStandardOutput "$fixture\$Action.out" -RedirectStandardError "$fixture\$Action.err" -PassThru -Wait
 Check ($p.ExitCode -eq 0) "User transaction action failed: $Action / $(Get-Content "$fixture\$Action.err" -Raw)"
}
try{
 Run 'Initialize'
 Check (Test-Path "$root\.setup-user-transaction.json") 'Pending journal missing'
 Check (Test-Path "$root\schemas\虎整句\current.txt") 'Compiled sentence descriptor missing'
 Run 'Rollback'
 Check ((Get-FileHash "$root\config.txt").Hash -eq $hash) 'Rollback changed preexisting configuration'
 Check (!(Test-Path "$root\installed-data-version.txt")) 'Rollback retained success marker'
 Check (!(Test-Path "$root\schemas\虎整句\current.txt")) 'Rollback retained published descriptor'
 Check (@(Get-ChildItem "$root\码表" -File -Recurse).Count -eq 0) 'Rollback retained newly copied source files'
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
