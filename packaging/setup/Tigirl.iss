#ifndef PackageDir
 #error PackageDir is required
#endif
#ifndef Generation
 #error Generation is required
#endif
#ifndef ProductVersion
 #define ProductVersion "2026.9.10.2"
#endif
[Setup]
AppId=NativeTiger
AppName=虎娘
AppVersion={#ProductVersion}
AppVerName=虎娘 / Tigirl {#ProductVersion}
AppPublisher=Tigirl
DefaultDirName={code:InstallDirectory}
DisableDirPage=yes
DisableProgramGroupPage=yes
UsePreviousAppDir=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.19041
WizardStyle=modern
WizardSizePercent=110
SetupIconFile={#PackageDir}\Tigirl.ico
WizardImageFile=..\..\assets\Tigirl-wizard.bmp
WizardSmallImageFile=..\..\assets\Tigirl-wizard-small.bmp
UninstallDisplayIcon={app}\Tigirl.Maintenance.exe
OutputDir={#OutputPath}
OutputBaseFilename=Tigirl-{#ProductVersion}-x64-Setup
Compression=lzma2
SolidCompression=yes
DiskSpanning=no
CloseApplications=no
RestartApplications=no
AllowCancelDuringInstall=no
SetupLogging=yes
SetupMutex=Tigirl.Setup
Uninstallable=yes
UninstallFilesDir={app}
VersionInfoVersion={#ProductVersion}
VersionInfoProductName=Tigirl
VersionInfoDescription=虎娘安装程序
DisableWelcomePage=no
[Languages]
Name: "chinesesimp"; MessagesFile: "{#ChineseMessages}"
[Messages]
WindowsVersionNotSupported=此安装程序仅支持 Intel/AMD x64 Windows 10 2004 或更新版本，不支持 ARM64 和 32 位 Windows。
OnlyOnTheseArchitectures=此安装程序仅支持 Intel/AMD x64 Windows，包含 x86 程序支持；不支持 ARM64 和 32 位 Windows。
[LangOptions]
DialogFontName=Microsoft YaHei UI
DialogFontSize=9
WelcomeFontName=Microsoft YaHei UI
[Files]
; One embedded payload. Extract for preflight, then let Inno copy/track these exact files.
Source: "{#PackageDir}\*"; DestDir: "{tmp}\payload"; Flags: dontcopy recursesubdirs createallsubdirs
Source: "{tmp}\payload\*"; DestDir: "{app}\versions\{#Generation}"; Flags: external recursesubdirs createallsubdirs onlyifdoesntexist uninsrestartdelete
Source: "{#PackageDir}\setup\Tigirl.Maintenance.exe"; DestDir: "{app}"; Flags: ignoreversion uninsrestartdelete
[Icons]
Name: "{commonprograms}\虎娘\输入设置"; Filename: "{app}\versions\{#Generation}\x64\Tigirl.exe"
[Run]
Filename: "{app}\versions\{#Generation}\x64\Tigirl.exe"; Description: "打开输入设置"; Flags: postinstall nowait skipifsilent runasoriginaluser; Check: CanOpenSettings
[UninstallDelete]
Type: files; Name: "{app}\setup-transaction.id"
Type: files; Name: "{app}\setup.log"
Type: files; Name: "{app}\committed-*.json"
Type: files; Name: "{app}\recovered-*.json"
Type: files; Name: "{app}\previous-*.json"
Type: dirifempty; Name: "{app}\versions"
Type: dirifempty; Name: "{app}"
[Code]
var
  Prepared, Applied, Committed, Failed, Deferred, RestartCleanup: Boolean;
  TxId: String;
  Info: TOutputMsgWizardPage;
  Progress: TOutputProgressWizardPage;
  LogButton: TNewButton;
function CanOpenSettings: Boolean;
begin Result:=Committed and not Failed and not Deferred; end;
function InstallDirectory(Param: String): String;
begin
  Result := ExpandConstant('{commonpf64}\Tigirl');
  if not FileExists(Result+'\install.json') and FileExists(ExpandConstant('{commonpf64}\NativeTiger\install.json')) then
    Result := ExpandConstant('{commonpf64}\NativeTiger');
end;
function PowerShell: String;
begin Result := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'); end;
function Deploy(Action, ScriptRoot: String): Boolean;
var Code: Integer; Params: String;
begin
  Params := '-NoProfile -ExecutionPolicy Bypass -File "'+ScriptRoot+'\setup\deploy.ps1" -Action '+Action+' -InstallRoot "'+ExpandConstant('{app}')+'" -Generation {#Generation} -Payload "'+ExpandConstant('{tmp}\payload')+'"';
  Result := Exec(PowerShell, Params, '', SW_HIDE, ewWaitUntilTerminated, Code);
  Log('Deploy '+Action+': '+IntToStr(Code));
  if (Code=3010) and (Action='Uninstall') then begin RestartCleanup:=True; Code:=0; end;
  Result := Result and (Code=0);
end;
function UserStep(Action: String): Integer;
var Params: String; Code: Integer;
begin
  Params := '-NoProfile -ExecutionPolicy Bypass -File "'+ExpandConstant('{app}\versions\{#Generation}\initialize.ps1')+'" -Quiet -RequireStandardUser -NoDialogs -Transaction "'+TxId+'" -TransactionAction '+Action;
  if WizardSilent then Params := Params+' -SkipConflicts';
  if not ExecAsOriginalUser(PowerShell,Params,'',SW_HIDE,ewWaitUntilTerminated,Code) then Code:=1;
  Result:=Code;
end;
function InitializeSetup: Boolean;
begin
  Result := not IsArm64;
  if not Result then SuppressibleMsgBox('此安装程序仅支持 Intel/AMD x64 Windows。ARM64 系统请使用对应版本。',mbError,MB_OK,IDOK);
end;
procedure OpenLog(Sender: TObject);
var Code: Integer;
begin
 if FileExists(ExpandConstant('{app}\setup.log')) then ShellExec('open','notepad.exe','"'+ExpandConstant('{app}\setup.log')+'"','',SW_SHOWNORMAL,ewNoWait,Code)
 else ShellExec('open','notepad.exe','"'+ExpandConstant('{log}')+'"','',SW_SHOWNORMAL,ewNoWait,Code);
end;
procedure InitializeWizard;
begin
  Info:=CreateOutputMsgPage(wpWelcome,'安装说明','将安装完整的虎娘输入法',
    '支持 x64 和 x86 应用。'+#13#10+#13#10+
    '程序安装到 Program Files，码表、设置和个人词条保存在当前用户的 %LOCALAPPDATA%\NativeTiger。'+#13#10+#13#10+
    '已有设置将保留。同名码表内容不同时，可选择覆盖或跳过；覆盖前会自动备份。'+#13#10+#13#10+
    '安装完成后请重新打开需要输入的程序。');
  Progress:=CreateOutputProgressPage('准备安装','正在校验安装文件，请稍候。');
  LogButton:=TNewButton.Create(WizardForm);LogButton.Parent:=WizardForm;LogButton.Caption:='打开日志';
  LogButton.Left:=ScaleX(12);LogButton.Top:=WizardForm.CancelButton.Top;LogButton.Width:=ScaleX(95);LogButton.Height:=WizardForm.CancelButton.Height;
  LogButton.OnClick:=@OpenLog;
end;
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result:='';
  Progress.Show;
  try
    Progress.SetText('正在解压和校验安装文件。','');
    ExtractTemporaryFiles('{tmp}\payload\*');
    if not Deploy('Begin',ExpandConstant('{tmp}\payload')) then
      Result:='安装检查未通过。可能存在不兼容的安装、版本降级或文件损坏。请查看安装日志。'
    else Prepared:=True;
  finally Progress.Hide; end;
end;
procedure Recover;
begin
  if Prepared and not Committed then begin
    if Applied then UserStep('Rollback');
    if not Deploy('Recover',ExpandConstant('{tmp}\payload')) then
      SuppressibleMsgBox('自动恢复未完成。请重新运行此安装程序并查看日志；不要手动删除旧版本。',mbError,MB_OK,IDOK);
    Prepared:=False;
  end;
end;
procedure CurStepChanged(CurStep: TSetupStep);
var Code: Integer; Text: AnsiString;
begin
  if CurStep=ssPostInstall then begin
    try
      WizardForm.StatusLabel.Caption:='正在注册 x64 / x86 输入法……';
      if not Deploy('Apply',ExpandConstant('{app}\versions\{#Generation}')) then RaiseException('输入法注册失败。');
      Applied:=True;
      { Backend writes a plain transaction-id file for the original-user handoff. }
      if not LoadStringFromFile(ExpandConstant('{app}\setup-transaction.id'),Text) then RaiseException('无法读取安装事务。');
      TxId:=Trim(String(Text));
      WizardForm.StatusLabel.Caption:='正在检查码表冲突并编译缓存……';
      Code:=UserStep('Initialize');
      Deferred:=Code=20;
      if (Code<>0) and not Deferred then RaiseException('码表初始化未完成，正在恢复原安装。');
      WizardForm.StatusLabel.Caption:='正在完成安装……';
      if not Deploy('Commit',ExpandConstant('{app}\versions\{#Generation}')) then RaiseException('安装提交失败。');
      Committed:=True;
      if not Deferred then UserStep('Complete');
    except
      Failed:=True; Recover;
      SuppressibleMsgBox(GetExceptionMessage,mbError,MB_OK,IDOK);
    end;
  end;
end;
procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID=wpFinished then begin
    if Failed then begin
      WizardForm.FinishedHeadingLabel.Caption:='安装未完成';
      WizardForm.FinishedLabel.Caption:='此次安装失败，已尝试恢复原注册和用户数据。请查看日志后重试。';
    end else if Deferred then WizardForm.FinishedLabel.Caption:='程序已安装。请用日常使用的 Windows 账号重新登录，完成码表初始化。'
    else WizardForm.FinishedLabel.Caption:='虎娘已安装。请重开微信、Word 等正在使用的程序。'+#13#10+'可以从开始菜单“虎娘 → 输入设置”调整选项。';
  end;
end;
procedure DeinitializeSetup;
begin if Prepared and not Committed then Recover; end;
function GetCustomSetupExitCode: Integer;
begin if Failed then Result:=1 else Result:=0; end;
function InitializeUninstall: Boolean;
begin
  Result:=Deploy('UninstallCheck',ExpandConstant('{app}\versions\{#Generation}'));
  if not Result then SuppressibleMsgBox('卸载检查未通过，可能涉及注册归属或安装记录、文件清单损坏。请查看安装目录中的 setup.log。',mbError,MB_OK,IDOK);
end;
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep=usUninstall then
    if not Deploy('Uninstall',ExpandConstant('{app}\versions\{#Generation}')) then RaiseException('卸载未完成，可能在撤销注册或清理程序文件时失败。请查看安装目录中的 setup.log。');
end;
function UninstallNeedRestart: Boolean;
begin Result:=RestartCleanup; end;
