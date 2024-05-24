#define MyAppName "ParsecVDA - Always Connected"
#define MyAppVersion "1.2.0"
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
Source: ".\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: ".\parsec-vdd-setup.exe"; DestDir: "{app}\driver"; Flags: ignoreversion
Source: ".\Setup.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: ".\ParsecVDAAC.xml"; DestDir: "{app}"; Flags: ignoreversion
Source: ".\ParsecVDAAC.exe"; DestDir: "{app}"; Flags: ignoreversion

[Tasks]
Name: install_vdd; Description: "Install Parsec VDD v{#VddVersion}"

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\driver\parsec-vdd-setup.exe"; Parameters: "/S"; Flags: runascurrentuser; Tasks: install_vdd
Filename: "{app}\Setup.bat"; Parameters: "install"; Flags: runhidden

[UninstallRun]
Filename: "{app}\Setup.bat"; Parameters: "uninstall"; Flags: runhidden