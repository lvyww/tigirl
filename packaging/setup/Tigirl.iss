#ifndef PackageDir
 #error PackageDir is required
#endif
#ifndef ProductVersion
 #define ProductVersion "2026.9.10.4"
#endif
[Setup]
AppId=Tigirl
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
MinVersion=10.0.17763
WizardStyle=modern
WizardSizePercent=110
SetupIconFile={#PackageDir}\Tigirl.ico
WizardImageFile=..\..\assets\Tigirl-wizard.bmp
WizardSmallImageFile=..\..\assets\Tigirl-wizard-small.bmp
UninstallDisplayIcon={app}\Tigirl.Maintenance.exe
OutputDir={#OutputPath}
OutputBaseFilename=虎娘-{#ProductVersion}-x64-x86-安装程序
; Highest standard preset; leave payload extraction and installation unchanged.
Compression=lzma2/ultra64
SolidCompression=yes
; Let 32-bit ISCC use the native compressor process for its larger dictionary.
LZMAUseSeparateProcess=yes
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
WindowsVersionNotSupported=此安装程序仅支持 Intel/AMD x64 Windows 10 1809 或更新版本，不支持 ARM64 和 32 位 Windows。
OnlyOnTheseArchitectures=此安装程序仅支持 Intel/AMD x64 Windows，包含 x86 程序支持；不支持 ARM64 和 32 位 Windows。
[LangOptions]
DialogFontName=Microsoft YaHei UI
DialogFontSize=9
WelcomeFontName=Microsoft YaHei UI
[Files]
; Full embedded payload. Version cleanup is exclusively inventory-driven.
Source: "{#PackageDir}\*"; DestDir: "{tmp}\payload"; Flags: dontcopy recursesubdirs createallsubdirs
Source: "{tmp}\payload\*"; DestDir: "{code:VersionDirectory}"; Flags: external recursesubdirs createallsubdirs uninsneveruninstall
Source: "{#PackageDir}\setup\Tigirl.Maintenance.exe"; DestDir: "{app}"; Flags: ignoreversion uninsrestartdelete
; Stable small uninstall backend: it does not depend on a retiring runtime directory.
Source: "{#PackageDir}\common.ps1"; DestDir: "{app}\maintenance"; Flags: ignoreversion uninsrestartdelete
Source: "{#PackageDir}\legacy_identity.ps1"; DestDir: "{app}\maintenance"; Flags: ignoreversion uninsrestartdelete
Source: "{#PackageDir}\data.ps1"; DestDir: "{app}\maintenance"; Flags: ignoreversion uninsrestartdelete
Source: "{#PackageDir}\retirement.ps1"; DestDir: "{app}\maintenance"; Flags: ignoreversion uninsrestartdelete
Source: "{#PackageDir}\setup\deploy.ps1"; DestDir: "{app}\maintenance\setup"; Flags: ignoreversion uninsrestartdelete
[Icons]
Name: "{commonprograms}\虎娘\输入设置"; Filename: "{code:VersionDirectory}\shared\Tigirl.exe"
[Run]
Filename: "{code:VersionDirectory}\shared\Tigirl.exe"; Description: "打开输入设置"; Flags: postinstall nowait skipifsilent runasoriginaluser; Check: CanOpenSettings
[UninstallDelete]
Type: files; Name: "{app}\setup-transaction.id"
Type: files; Name: "{app}\committed-*.json"
Type: files; Name: "{app}\recovered-*.json"
Type: files; Name: "{app}\previous-*.json"
Type: dirifempty; Name: "{app}\versions"
Type: dirifempty; Name: "{app}\retired"
Type: dirifempty; Name: "{app}\preserved"
Type: dirifempty; Name: "{app}"
[Code]
type
  TInstallationGuid = record
    D1: Cardinal;
    D2, D3: Word;
    D4: array[0..7] of Byte;
  end;
var
  Prepared, Applied, Committed, Failed, Deferred, UserInitFailed, RestartCleanup: Boolean;
  TxId, InstallGeneration: String;
  Info: TOutputMsgWizardPage;
  Progress: TOutputProgressWizardPage;
  LogButton: TNewButton;
function CoCreateGuid(var Guid: TInstallationGuid): Integer;
  external 'CoCreateGuid@ole32.dll stdcall setuponly';
function NewInstallGeneration: String;
var G: TInstallationGuid;
begin
  if CoCreateGuid(G) <> 0 then RaiseException('无法生成安装编号。');
  Result:=LowerCase(Format('%.8x%.4x%.4x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x', [G.D1,G.D2,G.D3,G.D4[0],G.D4[1],G.D4[2],G.D4[3],G.D4[4],G.D4[5],G.D4[6],G.D4[7]]));
end;
function VersionDirectory(Param: String): String;
begin Result:=ExpandConstant('{app}\versions\')+InstallGeneration; end;
function CanOpenSettings: Boolean;
begin Result:=Committed and not Failed and not Deferred and not UserInitFailed; end;
function InstallDirectory(Param: String): String;
begin
  Result := ExpandConstant('{commonpf64}\Tigirl');
end;
function PowerShell: String;
begin Result := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'); end;
function StableLogDirectory: String;
begin Result := ExpandConstant('{commonappdata}\Tigirl\Logs'); end;
function StableLogPath: String;
begin Result := StableLogDirectory+'\setup.log'; end;
function Deploy(Action, ScriptRoot: String): Boolean;
var Code: Integer; Params: String;
begin
  Params := '-NoProfile -ExecutionPolicy Bypass -File "'+ScriptRoot+'\setup\deploy.ps1" -Action '+Action+' -InstallRoot "'+ExpandConstant('{app}')+'" -Payload "'+ExpandConstant('{tmp}\payload')+'"';
  if InstallGeneration<>'' then Params:=Params+' -Generation '+InstallGeneration;
  Result := Exec(PowerShell, Params, '', SW_HIDE, ewWaitUntilTerminated, Code);
  Log('Deploy '+Action+': '+IntToStr(Code));
  if Code=3010 then begin RestartCleanup:=True; Code:=0; end;
  Result := Result and (Code=0);
end;
function UserStep(Action: String): Integer;
var Params: String; Code: Integer;
begin
  { This hidden original-user step must never wait for a secondary modal UI. }
  { Conflicting user files are therefore preserved during graphical setup. }
  Params := '-NoProfile -ExecutionPolicy Bypass -File "'+ExpandConstant('{code:VersionDirectory}\initialize.ps1')+'" -Quiet -RequireStandardUser -NoDialogs -SkipConflicts -Transaction "'+TxId+'" -TransactionAction '+Action;
  if not ExecAsOriginalUser(PowerShell,Params,'',SW_HIDE,ewWaitUntilTerminated,Code) then Code:=1;
  Result:=Code;
end;
function InitializeSetup: Boolean;
begin
  Result := not IsArm64;
  if Result then InstallGeneration:=NewInstallGeneration;
  if not Result then SuppressibleMsgBox('此安装程序仅支持 Intel/AMD x64 Windows。ARM64 系统请使用对应版本。',mbError,MB_OK,IDOK);
end;
procedure OpenLog(Sender: TObject);
var Code: Integer; Path: String;
begin
  Path:=StableLogPath;
  if not FileExists(Path) then Path:=ExpandConstant('{log}');
  if FileExists(Path) then begin
    if not ShellExec('open',ExpandConstant('{sys}\notepad.exe'),'"'+Path+'"','',SW_SHOWNORMAL,ewNoWait,Code) then
      SuppressibleMsgBox('无法打开日志。'+#13#10+#13#10+Path+#13#10+'错误码：'+IntToStr(Code),mbError,MB_OK,IDOK);
  end else if DirExists(StableLogDirectory) then begin
    if not ShellExec('open',StableLogDirectory,'','',SW_SHOWNORMAL,ewNoWait,Code) then
      SuppressibleMsgBox('无法打开日志目录。'+#13#10+#13#10+StableLogDirectory+#13#10+'错误码：'+IntToStr(Code),mbError,MB_OK,IDOK);
  end else
    SuppressibleMsgBox('日志尚未创建。安装器日志路径：'+ExpandConstant('{log}'),mbInformation,MB_OK,IDOK);
end;
procedure InitializeWizard;
begin
  Info:=CreateOutputMsgPage(wpWelcome,'安装说明','将安装完整的虎娘输入法',
    '支持 x64 和 x86 应用。'+#13#10+#13#10+
    '程序安装到 Program Files，码表、设置和个人词条保存在当前用户的 %LOCALAPPDATA%\Tigirl。'+#13#10+#13#10+
    '已有设置将保留。未修改的默认码表会更新；用户修改或来源未知时保留现有文件，不阻塞安装。'+#13#10+#13#10+
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
      Result:='安装检查未通过。只有检测到安装目录外的未知注册、版本降级或新安装包损坏时才会停止。请点击“打开日志”查看详情。'
    else Prepared:=True;
  finally Progress.Hide; end;
end;
procedure Recover;
begin
  if Prepared and not Committed then begin
    if Applied then UserStep('Rollback');
    if not Deploy('Recover',ExpandConstant('{tmp}\payload')) then
      SuppressibleMsgBox('自动恢复未完成。请重新运行此安装程序并查看日志；不要手动删除仍被程序使用的旧版本文件。',mbError,MB_OK,IDOK);
    Prepared:=False;
  end;
end;
procedure CurStepChanged(CurStep: TSetupStep);
var Code: Integer; Text: AnsiString;
begin
  if CurStep=ssPostInstall then begin
    try
      WizardForm.StatusLabel.Caption:='正在切换到新版 x64 / x86 输入法……';
      if not Deploy('Apply',ExpandConstant('{code:VersionDirectory}')) then RaiseException('输入法注册失败。');
      Applied:=True;
      { Backend writes a plain transaction-id file for the original-user handoff. }
      if not LoadStringFromFile(ExpandConstant('{app}\setup-transaction.id'),Text) then RaiseException('无法读取安装事务。');
      TxId:=Trim(String(Text));
      { Machine registration is the commit point. User data initialization must never roll it back. }
      WizardForm.StatusLabel.Caption:='正在完成程序更新……';
      if not Deploy('Commit',ExpandConstant('{code:VersionDirectory}')) then RaiseException('安装提交失败。');
      Committed:=True;
      WizardForm.StatusLabel.Caption:='正在检查码表冲突并编译缓存……';
      Code:=UserStep('Initialize');
      Deferred:=Code=20;
      if (Code<>0) and not Deferred then begin
        UserInitFailed:=True;
        UserStep('Rollback');
      end else if not Deferred then
        UserStep('Complete');
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
      WizardForm.FinishedLabel.Caption:='新版输入法注册或提交失败，已尝试恢复原注册。请点击“打开日志”后重试。';
    end else if UserInitFailed then begin
      WizardForm.FinishedHeadingLabel.Caption:='程序更新完成';
      WizardForm.FinishedLabel.Caption:='新版输入法已经安装并启用，但当前用户的码表初始化未完成；原用户数据已恢复。可重新登录后重试初始化，程序更新不会因此回退。';
    end else if Deferred then
      WizardForm.FinishedLabel.Caption:='程序已安装并切换到新版。请用日常使用的 Windows 账号重新登录，完成码表初始化。'
    else WizardForm.FinishedLabel.Caption:='虎娘已安装。请重开微信、Word 等正在使用的程序。'+#13#10+'可以从开始菜单“虎娘 → 输入设置”调整选项。';
    if RestartCleanup and not Failed then WizardForm.FinishedLabel.Caption:=WizardForm.FinishedLabel.Caption+#13#10+#13#10+'旧版文件将在重启后清理；无法登记的文件已记录日志，下次维护时重试。';
  end;
end;
function NeedRestart: Boolean;
begin Result:=RestartCleanup; end;
procedure DeinitializeSetup;
begin if Prepared and not Committed then Recover; end;
function GetCustomSetupExitCode: Integer;
begin if Failed then Result:=1 else Result:=0; end;
function InitializeUninstall: Boolean;
begin
  Result:=Deploy('UninstallCheck',ExpandConstant('{app}\maintenance'));
  if not Result then SuppressibleMsgBox('卸载安全检查发现输入法注册指向虎娘安装目录之外的未知程序，或卸载后端无法运行。为避免破坏其他软件，已停止卸载。请查看日志：'+StableLogPath,mbError,MB_OK,IDOK);
end;
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep=usUninstall then
    if not Deploy('Uninstall',ExpandConstant('{app}\maintenance')) then RaiseException('输入法注册未能完全撤销。程序文件尚未作为成功卸载处理。请查看日志：'+StableLogPath);
end;
function UninstallNeedRestart: Boolean;
begin Result:=RestartCleanup; end;
