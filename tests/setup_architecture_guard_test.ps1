param([Parameter(Mandatory)][string]$Installer)
$ErrorActionPreference='Stop'
if([Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString() -ne 'Arm64'){throw 'This negative test requires ARM64 Windows.'}
. "$PSScriptRoot\..\packaging\common.ps1"
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$before=@((Get-ComPath 'Registry64'),(Get-ComPath 'Registry32'))
$config=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'NativeTiger\config.txt'
$hash=(Get-FileHash $config).Hash
$existed=Test-Path (Join-Path $env:ProgramFiles 'Tigirl')
$launch=Get-Date;$p=Start-Process $Installer -PassThru
$window=$null;$button=$null;$dialog=''
try{
 $name=[IO.Path]::GetFileNameWithoutExtension($Installer)+'*'
 for($i=0;$i -lt 20;$i++){
  Start-Sleep -Milliseconds 250
  $ui=Get-Process|Where-Object {$_.ProcessName -like $name -and $_.StartTime -ge $launch.AddSeconds(-1) -and $_.MainWindowHandle -ne 0}|Select-Object -First 1
  if($ui){$window=[Windows.Automation.AutomationElement]::FromHandle($ui.MainWindowHandle);break}
 }
 if(!$window){throw 'No architecture error window.'}
 $items=$window.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)
 $names=@()
 foreach($item in $items){$names+=$item.Current.Name;if($item.Current.ControlType -eq [Windows.Automation.ControlType]::Button -and $item.Current.Name -match '确定|OK'){$button=$item}}
 $dialog=$names -join ' '
 # Inno rejects this OS before SetupLogging starts; verify the actual error window.
 if($dialog -notmatch 'Intel/AMD x64' -or $dialog -notmatch 'ARM64'){throw "Unexpected rejection: $dialog"}
}finally{if($ui){[void]$ui.CloseMainWindow()}}
if(!$p.WaitForExit(5000)){throw 'Architecture rejection did not terminate.'}
if($p.ExitCode -eq 0){throw 'Installer accepted ARM64.'}
$after=@((Get-ComPath 'Registry64'),(Get-ComPath 'Registry32'))
if(($before -join '|') -ne ($after -join '|') -or (Get-FileHash $config).Hash -ne $hash){throw 'Rejected installer changed live registration or configuration.'}
if((Test-Path (Join-Path $env:ProgramFiles 'Tigirl')) -ne $existed){throw 'Rejected installer created a program directory.'}
$result=@{status='passed';exit_code=$p.ExitCode;dialog=$dialog;live_registration_preserved=$true;configuration_preserved=$true;program_directory_preserved=$true}
$result|ConvertTo-Json|Set-Content "$PSScriptRoot\..\build\setup-arm64-rejection.json" -Encoding UTF8
$result|ConvertTo-Json
