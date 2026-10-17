param([Parameter(Mandatory)][string]$Compiler,[string]$Output)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
if(!$Output){$Output=Join-Path $repo ('build\setup-finished-'+[guid]::NewGuid().ToString('N'))}
New-Item $Output -ItemType Directory -Force|Out-Null
$Output=(Resolve-Path $Output).Path
if((Test-Path "$Output\negative") -or (Test-Path "$Output\fixed")){
 throw 'Choose a fresh output directory; previous fixture markers must not be reused.'
}
Add-Type -Path "$PSScriptRoot\setup_wizard_probe.cs"
$source=Get-Content "$repo\packaging\setup\Tigirl.iss" -Raw -Encoding UTF8
$restore=[regex]::Match($source,'(?s)function IsWindowEnabled.*?(?=function Deploy)').Value
$finished=[regex]::Match($source,'(?s)procedure CurPageChanged.*?(?=function NeedRestart)').Value.Replace('procedure CurPageChanged(', 'procedure ShippingCurPageChanged(')
$canOpen=[regex]::Match($source,'(?s)function CanOpenSettings: Boolean;.*?end;').Value
if(!$restore -or !$finished -or !$canOpen){throw 'Shipping finished-page implementation not found'}
$template=@'
[Setup]
AppName=Tigirl finished-page regression fixture
AppVersion=1
DefaultDirName={tmp}\unused-finished-page-fixture
CreateAppDir=no
Uninstallable=no
PrivilegesRequired=lowest
DisableWelcomePage=yes
DisableDirPage=yes
DisableProgramGroupPage=yes
AllowCancelDuringInstall=no
Compression=none
WizardStyle=modern
OutputBaseFilename=finished-probe
[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{param:Output}\launch.ps1"""; Description: "Open settings fixture"; Flags: postinstall nowait skipifsilent runasoriginaluser runhidden; Check: CanOpenSettings
[Code]
var
  Committed, Failed, Deferred, UserInitFailed, RestartCleanup: Boolean;
  LogButton: TNewButton;
  NativeBefore, VclBefore: Integer;
__SHIPPING__
procedure LogClick(Sender: TObject);
begin
  SaveStringToFile(ExpandConstant('{param:Output}\log-clicked.txt'),'clicked',False);
end;
procedure InitializeWizard;
begin
  LogButton:=TNewButton.Create(WizardForm); LogButton.Parent:=WizardForm;
  LogButton.Caption:='Log fixture'; LogButton.Left:=12;
  LogButton.Top:=WizardForm.CancelButton.Top;
  LogButton.OnClick:=@LogClick;
end;
procedure CurStepChanged(CurStep: TSetupStep);
var Code: Integer; Scenario: String;
begin
  if CurStep=ssPostInstall then begin
    if not ExecAsOriginalUser(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
      '-NoProfile -Command "exit 0"','',SW_HIDE,ewWaitUntilTerminated,Code) then
      RaiseException('Could not run hidden child fixture');
    if Code<>0 then RaiseException('Hidden child fixture failed');
    Scenario:=ExpandConstant('{param:Scenario}');
    Committed:=True;
    Failed:=Scenario='failed'; Deferred:=Scenario='deferred';
    UserInitFailed:=Scenario='user-failed'; RestartCleanup:=Scenario='restart-cleanup';
    { Reproduce the observed native disabled/VCL enabled mismatch. }
    if Scenario<>'healthy' then EnableWindow(WizardForm.Handle,False);
    if Scenario='vcl-disabled' then WizardForm.Enabled:=False;
    NativeBefore:=Ord(IsWindowEnabled(WizardForm.Handle));
    VclBefore:=Ord(WizardForm.Enabled);
    { Never undo a temporary disable on an in-progress page. }
    ShippingCurPageChanged(wpInstalling);
    if Ord(IsWindowEnabled(WizardForm.Handle))<>NativeBefore then
      RaiseException('Enabled wizard during installation');
  end;
end;
procedure CurPageChanged(CurPageID: Integer);
var Text: String;
begin
  ShippingCurPageChanged(CurPageID);
  if CurPageID=wpFinished then begin
    Text:='wizard='+IntToStr(WizardForm.Handle)+#13#10+
      'finish='+IntToStr(WizardForm.NextButton.Handle)+#13#10+
      'log='+IntToStr(LogButton.Handle)+#13#10+
      'native_before='+IntToStr(NativeBefore)+#13#10+
      'vcl_before='+IntToStr(VclBefore)+#13#10+
      'run_count='+IntToStr(WizardForm.RunList.Items.Count)+#13#10+
      'run_list='+IntToStr(WizardForm.RunList.Handle)+#13#10+
      'run_height='+IntToStr(SendMessage(WizardForm.RunList.Handle,$01A1,0,0))+#13#10;
    SaveStringToFile(ExpandConstant('{param:Output}\ready.tmp'),Text,False);
    RenameFile(ExpandConstant('{param:Output}\ready.tmp'),ExpandConstant('{param:Output}\ready.txt'));
  end else if CurPageID=wpReady then
    PostMessage(WizardForm.NextButton.Handle,$00F5,0,0);
end;
function NextButtonClick(CurPageID: Integer): Boolean;
var Text: String;
begin
  Result:=True;
  if CurPageID=wpFinished then begin
    Text:='none';
    if WizardForm.RunList.Items.Count>0 then Text:=IntToStr(Ord(WizardForm.RunList.Checked[0]));
    SaveStringToFile(ExpandConstant('{param:Output}\finish-clicked.txt'),Text,False);
  end;
end;
'@
$results=@()
foreach($variant in @('negative','fixed')){
 $variantDir=Join-Path $Output $variant
 New-Item $variantDir -ItemType Directory -Force|Out-Null
 $handler=$finished
 if($variant -eq 'negative'){$handler=$handler.Replace('    RestoreFinishedWizard;','')}
 $script=$template.Replace('__SHIPPING__',($restore+"`r`n"+$canOpen+"`r`n"+$handler))
 $iss=Join-Path $variantDir 'probe.iss'
 $script|Set-Content $iss -Encoding UTF8
 & $Compiler '/Qp' "/O$variantDir" $iss
 if($LASTEXITCODE){throw 'Finished-page fixture did not compile'}
 $cases=@('disabled','uncheck','vcl-disabled','healthy','failed','user-failed','deferred','restart-cleanup')
 if($variant -eq 'negative'){$cases=@('disabled')}
 foreach($case in $cases){
  $run=Join-Path $variantDir $case
  New-Item $run -ItemType Directory -Force|Out-Null
  'Set-Content -LiteralPath "$PSScriptRoot\settings-launched.txt" -Value launched'|Set-Content "$run\launch.ps1" -Encoding UTF8
  $canLaunch=$case -notin @('failed','user-failed','deferred')
  [SetupWizardProbe]::Run("$variantDir\finished-probe.exe",$run,$case,($variant -eq 'fixed'),$canLaunch,($case -eq 'uncheck'))
  $results+=@{variant=$variant;scenario=$case;passed=$true}
 }
}
$report=@{status='passed';cases=$results;isolated_desktop=$true;input_desktop_switched=$false;live_installation_executed=$false;shipping_handler_sha256=(Get-FileHash "$repo\packaging\setup\Tigirl.iss").Hash}|ConvertTo-Json -Depth 5
$report|Set-Content "$Output\results.json" -Encoding UTF8
$report
