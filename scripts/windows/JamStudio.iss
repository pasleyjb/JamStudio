; Inno Setup script for JamStudio (Windows x64)
; 1. Run scripts/package-windows.ps1 first so dist\JamStudio-win64 exists
; 2. Open this file in Inno Setup Compiler and Build
; Output: dist\JamStudio-Setup-<version>.exe

#define MyAppName "JamStudio"
#define MyAppVersion "0.9.6"
#define MyAppPublisher "JamStudio"
#define MyAppURL "https://github.com/pasleyjb/JamStudio"
#define MyAppExeName "JamStudio.exe"
#define StageDir "..\..\dist\JamStudio-win64"

[Setup]
AppId={{A8F3C2E1-9B4D-4F6A-8C1E-2D7B5A90E3F4}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=JamStudio-Setup-{#MyAppVersion}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
