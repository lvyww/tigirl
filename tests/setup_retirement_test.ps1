$ErrorActionPreference='Stop'
function Check($Value,$Message){if(!$Value){throw $Message}}
$t=$null;$e=$null
$ast=[Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot\..\packaging\setup\deploy.ps1",[ref]$t,[ref]$e)
if($e){throw $e}
foreach($fn in $ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst]},$false)){Invoke-Expression $fn.Extent.Text}
. "$PSScriptRoot\..\packaging\data.ps1"
. "$PSScriptRoot\..\packaging\retirement.ps1"
# Stub our own queue function: do not call any operating-system reboot API.
$script:queued=[Collections.Generic.List[string]]::new()
$script:failPath=''
function Queue-Deletion([string]$Path){
 if($Path -eq $script:failPath){throw 'Fixture queue failure'}
 $script:queued.Add($Path);$script:restart=$true
}
$InstallRoot=Join-Path $env:TEMP ('Tigirl-retire-fixture-'+[guid]::NewGuid().ToString('N'))
function Make-Version($Id){
 $dir=Join-Path $InstallRoot ('versions\'+$Id)
 New-Item "$dir\x64","$dir\x86" -ItemType Directory -Force|Out-Null
 foreach($path in @('x64\Tigirl.dll','x86\Tigirl.dll','x64\model.bin')){[IO.File]::WriteAllText((Join-Path $dir $path),'official')}
 $files=@(Get-ChildItem $dir -Recurse -File|ForEach-Object {@{path=$_.FullName.Substring($dir.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
 @{version='2026.9.20.1';files=$files}|ConvertTo-Json -Depth 5|Set-Content "$dir\manifest.json" -Encoding UTF8
 New-Item "$dir\x86\model.bin" -ItemType HardLink -Target "$dir\x64\model.bin"|Out-Null
 return $dir
}
function Simulate-Reboot{
 foreach($path in $script:queued){
  Check ($path.StartsWith($InstallRoot+'\',[StringComparison]::OrdinalIgnoreCase)) 'Fixture path escaped'
  if(Test-Path -LiteralPath $path -PathType Leaf){Remove-Item -LiteralPath $path -Force}
  elseif((Test-Path -LiteralPath $path -PathType Container) -and !(Get-ChildItem -LiteralPath $path -Force|Select-Object -First 1)){Remove-Item -LiteralPath $path}
 }
 $script:queued.Clear()
}
try{
 $current=Make-Version ('a'*32);$old=Make-Version ('1'*16);$older=Make-Version ('2'*32)
 Invoke-RetiredVersionCleanup $current
 Check (Test-Path "$old\x64\model.bin") 'Upgrade deleted files before reboot'
 Check ($script:queued.Contains("$old\x86\model.bin")) 'x86 resource alias was not retired'
 Check (!$script:queued.Contains("$current\x64\model.bin")) 'Current version was retired'
 Check ((Test-Path (Get-RetirementPath $old)) -and (Test-Path (Get-RetirementPath $older))) 'Durable retirement inventories missing'
 Check ($script:queued.IndexOf("$old\x64\model.bin") -lt $script:queued.IndexOf("$old\x64")) 'Directory queued before its files'
 Check ($script:queued.IndexOf("$old\x64") -lt $script:queued.IndexOf($old)) 'Parent queued before subdirectory'
 Simulate-Reboot
 Check (!(Test-Path $old) -and !(Test-Path $older)) 'Reboot simulation did not clear official versions'
 Invoke-RetiredVersionCleanup $current
 Check (!(Test-Path (Get-RetirementPath $old))) 'Completed inventory not pruned'
 Check (Test-Path "$current\x64\model.bin") 'Current resource lost'
 # A same-version reinstall still needs a brand-new directory.
 $fresh=Get-NewVersionDirectory ('b'*32)
 Check ($fresh -ne $current) 'Fresh generation reused current version'
 $blocked=$false;try{Get-NewVersionDirectory ('a'*32)|Out-Null}catch{$blocked=$true}
 Check $blocked 'Existing generation allowed to be overwritten'
 $blocked=$false;try{Get-NewVersionDirectory ('1'*16)|Out-Null}catch{$blocked=$true}
 Check $blocked 'Legacy content-hash generation accepted for a new install'
 # Retired paths cannot be reused even if their runtime directory is already gone.
 Save-Json @{version='2026.9.20.1';files=@()} (Get-RetirementPath $fresh)
 $blocked=$false;try{Get-NewVersionDirectory ('b'*32)|Out-Null}catch{$blocked=$true}
 Check $blocked 'Retired generation was reused'
 # Edits and unlisted user files survive; edits also get an untracked backup.
 $edited=Make-Version ('3'*16)
 [IO.File]::WriteAllText("$edited\x64\model.bin",'personal-model')
 [IO.File]::WriteAllText("$edited\notes.txt",'personal-notes')
 Remove-Version $edited -AtReboot
 Check (!$script:queued.Contains("$edited\x64\model.bin")) 'Modified model queued'
 Check (!$script:queued.Contains("$edited\x86\model.bin")) 'Modified hard-link alias queued'
 Check (!$script:queued.Contains("$edited\notes.txt")) 'Unlisted file queued'
 $preserved=Join-Path $InstallRoot ('preserved\'+('3'*16)+'\x64\model.bin')
 Check ([IO.File]::ReadAllText($preserved) -eq 'personal-model') 'Independent model backup missing'
 Simulate-Reboot
 Check (Test-Path "$edited\notes.txt") 'Unknown user file lost'
 Check ([IO.File]::ReadAllText("$edited\x64\model.bin") -eq 'personal-model') 'Modified resource lost'
 Check (!(Test-Path "$edited\manifest.json")) 'Test did not exercise missing original inventory'
 Remove-Version $edited -AtReboot
 $script:queued.Clear()
 # Partial scheduling remains recoverable after the original manifest disappears.
 $retry=Make-Version ('4'*32);$script:failPath="$retry\x86\Tigirl.dll"
 $blocked=$false;try{Remove-Version $retry -AtReboot}catch{$blocked=$true}
 Check $blocked 'Scheduling error was hidden'
 Check (Test-Path (Get-RetirementPath $retry)) 'Scheduling failure lost retry inventory'
 Simulate-Reboot
 Remove-Item "$retry\manifest.json"
 $script:failPath='';Remove-Version $retry -AtReboot
 Check ($script:queued.Contains("$retry\x86\Tigirl.dll")) 'Partial cleanup was not retried'
 Simulate-Reboot;Check (!(Test-Path $retry)) 'Retry did not finish cleanup'
 # Malformed inventory must fail before it can enqueue an outside path.
 $bad=Make-Version ('5'*32)
 @{version='2026.9.20.1';files=@(@{path='..\outside';sha256=('e'*64)})}|ConvertTo-Json -Depth 5|Set-Content "$bad\manifest.json"
 $script:queued.Clear();$blocked=$false
 try{Remove-Version $bad -AtReboot}catch{$blocked=$true}
 Check ($blocked -and $script:queued.Count -eq 0) 'Escaping inventory reached cleanup queue'
 'PASS: whole-version reboot retirement, both architecture paths, current-version safety, unique generations, preserved edits, unknown files, partial scheduling, retry inventory, path containment.'
}finally{if(Test-Path $InstallRoot){Remove-Item -LiteralPath $InstallRoot -Recurse -Force}}
