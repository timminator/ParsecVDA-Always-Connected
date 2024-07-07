#define MyAppName "ParsecVDA - Always Connected"
#define MyAppVersion "1.4.0"
#define MyAppURL "https://github.com/timminator/ParsecVDA-Always-Connected"
#define MyAppExeName "ParsecVDA - Always Connected.exe"

#define _Major
#define _Minor
#define _Rev
#define _Build
#define VddVersion GetVersionComponents(".\parsec-vdd-setup.exe", _Major, _Minor, _Rev, _Build), Str(_Major) + "." + Str(_Minor)

[Setup]
SignTool=signtool $f
AppId={{4EC2E655-53B0-4B4A-A3C9-42C445FA22CE}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commonpf64}\{#MyAppName}
UsePreviousAppDir=yes
LicenseFile=..\..\LICENSE
DisableProgramGroupPage=yes
PrivilegesRequired=admin
OutputBaseFilename={#MyAppName}-v{#MyAppVersion}-setup-x64
SetupIconFile=..\..\parsec.ico
Compression=lzma
SolidCompression=yes
WizardStyle=classic
UninstallDisplayName={#MyAppName}   
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SelectTasksLabel2=Select the additional tasks you would like Setup to perform.%nParsecVDA - Always Connected requires the Parsec Virtual Display Driver. If the driver is already installed, you can uncheck this task. To continue, click Next.
FinishedLabel=Setup has finished installing [name] on your computer.%n%nAdditional notice: If you are using this software in combination with the Parsec App, make sure that under Settings > Host > "Fallback to Virtual Display" is set to off.

[Dirs]
Name: "{app}"; Permissions: everyone-full

[Files]
Source: ".\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion;
Source: ".\parsec-vdd-setup.exe"; DestDir: "{app}\driver";
Source: ".\Setup.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: ".\CheckPrerequisites.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: ".\ParsecVDAAC.xml"; DestDir: "{app}"; Flags: onlyifdoesntexist
Source: ".\ParsecVDAAC.exe"; DestDir: "{app}"; Flags: onlyifdoesntexist
Source: ".\LICENSE_parsec-vdd.txt"; Flags: dontcopy
Source: ".\LICENSE_winsw.txt"; Flags: dontcopy

[Code]
procedure RunBatchFile(FileName: String; WorkingDir: String);
var
  ResultCode: Integer;
begin
    Exec(ExpandConstant(FileName), '', WorkingDir, SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
    ExtractTemporaryFile('CheckPrerequisites.bat');
    RunBatchFile('{tmp}\CheckPrerequisites.bat', ExpandConstant('{app}'));
    Result := '';
end;

var
  LicenseAcceptedRadioButtons: array of TRadioButton;

procedure CheckLicenseAccepted(Sender: TObject);
begin
  // Update Next button when user (un)accepts the license
  WizardForm.NextButton.Enabled :=
    LicenseAcceptedRadioButtons[TComponent(Sender).Tag].Checked;
end;

procedure LicensePageActivate(Sender: TWizardPage);
begin
  // Update Next button when user gets to second license page
  CheckLicenseAccepted(LicenseAcceptedRadioButtons[Sender.Tag]);
end;

function CloneLicenseRadioButton(
  Page: TWizardPage; Source: TRadioButton): TRadioButton;
begin
  Result := TRadioButton.Create(WizardForm);
  Result.Parent := Page.Surface;
  Result.Caption := Source.Caption;
  Result.Left := Source.Left;
  Result.Top := Source.Top;
  Result.Width := Source.Width;
  Result.Height := Source.Height;
  // Needed for WizardStyle=modern / WizardResizable=yes
  Result.Anchors := Source.Anchors;
  Result.OnClick := @CheckLicenseAccepted;
  Result.Tag := Page.Tag;
end;

var
  LicenseAfterPage: Integer;

procedure AddLicensePage(LicenseFileName: string);
var
  Idx: Integer;
  Page: TOutputMsgMemoWizardPage;
  LicenseFilePath: string;
  RadioButton: TRadioButton;
begin
  Idx := GetArrayLength(LicenseAcceptedRadioButtons);
  SetArrayLength(LicenseAcceptedRadioButtons, Idx + 1);

  Page :=
    CreateOutputMsgMemoPage(
      LicenseAfterPage, SetupMessage(msgWizardLicense),
      SetupMessage(msgLicenseLabel), SetupMessage(msgLicenseLabel3), '');
  Page.Tag := Idx;

  // Shrink license box to make space for radio buttons
  Page.RichEditViewer.Height := WizardForm.LicenseMemo.Height;
  Page.OnActivate := @LicensePageActivate;

  // Load license
  // Loading ex-post, as Lines.LoadFromFile supports UTF-8,
  // contrary to LoadStringFromFile.
  ExtractTemporaryFile(LicenseFileName);
  LicenseFilePath := ExpandConstant('{tmp}\' + LicenseFileName);
  Page.RichEditViewer.Lines.LoadFromFile(LicenseFilePath);
  DeleteFile(LicenseFilePath);

  // Clone accept/do not accept radio buttons
  RadioButton :=
    CloneLicenseRadioButton(Page, WizardForm.LicenseAcceptedRadio);
  LicenseAcceptedRadioButtons[Idx] := RadioButton;

  RadioButton :=
    CloneLicenseRadioButton(Page, WizardForm.LicenseNotAcceptedRadio);
  // Initially not accepted
  RadioButton.Checked := True;

  LicenseAfterPage := Page.ID;
end;

procedure InitializeWizard();
begin
  LicenseAfterPage := wpLicense;
  AddLicensePage('LICENSE_parsec-vdd.txt');
  AddLicensePage('LICENSE_winsw.txt');
end;

[Tasks]
Name: install_vdd; Description: "Install Parsec VDD v{#VddVersion}"

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\driver\parsec-vdd-setup.exe"; Parameters: "/S"; Flags: runascurrentuser; Tasks: install_vdd
Filename: "{app}\Setup.bat"; Parameters: "install"; Flags: runhidden

[UninstallRun]
Filename: "{app}\Setup.bat"; Parameters: "uninstall"; Flags: runhidden