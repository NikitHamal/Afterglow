; Afterglow — Windows installer (Inno Setup 6). Built by CI:
;   ISCC /DAppVersion=0.2.N /DSrcDir=<dist\Afterglow> /O<outdir> afterglow.iss
#define AppName "Afterglow"
#ifndef AppVersion
  #define AppVersion "0.2.0"
#endif
#ifndef SrcDir
  #define SrcDir "..\build\bin"
#endif

[Setup]
AppId={{8C3C2D7E-5B1A-4E5F-9A41-AF7E2C0B1A11}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Afterglow
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\Afterglow.exe
UninstallDisplayName={#AppName}
OutputBaseFilename=Afterglow-Setup
SetupIconFile=afterglow.ico
LicenseFile=LICENSE-18PLUS.txt
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SrcDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "LICENSE-18PLUS.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\Afterglow.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\Afterglow.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Afterglow.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
