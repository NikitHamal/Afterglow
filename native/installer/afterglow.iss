; Afterglow — Windows installer (Inno Setup 6). Built by CI:
;   ISCC /DAppVersion=0.N.0 /DSrcDir=<dist\Afterglow> /O<outdir> afterglow.iss
; Per-user install (no UAC): %LOCALAPPDATA%\Programs\Afterglow, so the in-game updater can upgrade silently:
;   Afterglow-Setup.exe /VERYSILENT /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS /NORESTART /RELAUNCH
#define AppName "Afterglow"
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SrcDir
  #define SrcDir "..\build\bin"
#endif

[Setup]
AppId={{8C3C2D7E-5B1A-4E5F-9A41-AF7E2C0B1A11}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
VersionInfoVersion={#AppVersion}.0
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} setup
AppPublisher=Afterglow
AppPublisherURL=https://github.com/NikitHamal/Afterglow
AppUpdatesURL=https://github.com/NikitHamal/Afterglow/releases/tag/nightly
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableDirPage=auto
DisableReadyPage=yes
UsePreviousAppDir=yes
PrivilegesRequired=lowest
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
; the running game holds this mutex: setup waits for / asks it to close (and so does the uninstaller)
AppMutex=AfterglowGameMutex
CloseApplications=force
CloseApplicationsFilter=*.exe
RestartApplications=no
SetupMutex=AfterglowSetupMutex

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[InstallDelete]
; upgrades replace the asset folder wholesale (no stale files from older builds)
Type: filesandordirs; Name: "{app}\assets"

[Files]
Source: "{#SrcDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "LICENSE-18PLUS.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\Afterglow.exe"
Name: "{autoprograms}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\Afterglow.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Afterglow.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
; silent update from inside the game: start it again afterwards
Filename: "{app}\Afterglow.exe"; Flags: nowait; Check: ShouldRelaunch

[UninstallDelete]
Type: filesandordirs; Name: "{app}\assets"
Type: filesandordirs; Name: "{userappdata}\Afterglow\logs"
Type: filesandordirs; Name: "{userappdata}\Afterglow\tmp"
Type: filesandordirs; Name: "{localappdata}\Temp\afterglow"
Type: files; Name: "{userappdata}\Afterglow\.instance.lock"
Type: dirifempty; Name: "{app}"

[Code]
const
  OldKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{8C3C2D7E-5B1A-4E5F-9A41-AF7E2C0B1A11}_is1';

// Early nightlies installed for all users (Program Files). This build installs per user so it can update
// itself without admin rights: offer to remove the old copy so there aren't two Afterglows.
function InitializeSetup(): Boolean;
var
  Uninst: String;
  Code: Integer;
begin
  Result := True;
  if WizardSilent then
    Exit;
  if RegQueryStringValue(HKLM64, OldKey, 'UninstallString', Uninst) or RegQueryStringValue(HKLM32, OldKey, 'UninstallString', Uninst) then
  begin
    if MsgBox('An older Afterglow is installed for all users.' + #13#10 + #13#10 +
              'This version installs just for you and keeps itself up to date. Remove the old copy now?' + #13#10 +
              '(Windows will ask for permission. Your saves are kept.)', mbConfirmation, MB_YESNO) = IDYES then
    begin
      Uninst := RemoveQuotes(Uninst);
      ShellExec('runas', Uninst, '/SILENT /NORESTART', '', SW_SHOW, ewWaitUntilTerminated, Code);
    end;
  end;
end;

function ShouldRelaunch(): Boolean;
var
  I: Integer;
begin
  Result := False;
  for I := 1 to ParamCount do
    if CompareText(ParamStr(I), '/RELAUNCH') = 0 then
      Result := True;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Data: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    Data := ExpandConstant('{userappdata}\Afterglow');
    if DirExists(Data) then
    begin
      if UninstallSilent then
        Exit;
      if MsgBox('Keep your saved nights, characters and settings?' + #13#10 + #13#10 +
                'Choose No to remove everything Afterglow stored on this PC.',
                mbConfirmation, MB_YESNO) = IDNO then
        DelTree(Data, True, True, True);
    end;
  end;
end;
