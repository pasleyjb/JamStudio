; Inno Setup script for JamStudio (Windows x64)
; 1. Run scripts/package-windows.ps1 first so dist\JamStudio-win64 exists
;    (package-windows.ps1 will invoke ISCC automatically when available)
; 2. Or open this file in Inno Setup Compiler and Build
; Output: dist\JamStudio-Setup-<version>.exe
;
; Override version from CLI:  ISCC /DMyAppVersion=0.9.6 scripts\windows\JamStudio.iss

#ifndef MyAppVersion
#define MyAppVersion "0.9.6"
#endif
#define MyAppName "JamStudio"
#define MyAppPublisher "JamStudio"
#define MyAppURL "https://github.com/pasleyjb/JamStudio"
#define MyAppExeName "JamStudio.exe"
#define StageDir "..\..\dist\JamStudio-win64"

[Setup]
AppId={{A8F3C2E1-9B4D-4F6A-8C1E-2D7B5A90E3F4}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=JamStudio-Setup-{#MyAppVersion}
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Per-user install by default (no admin). User can elevate via override dialog.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Stage dir must contain JamStudio.exe + FFmpeg DLLs + MSVC CRT DLLs
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
