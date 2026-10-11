[Setup]
AppId={{D17A83B5-026F-475C-B54D-A0E28C42611E}
AppName=DoomCraft
AppVersion=0.1.0
AppPublisher=roryekay
AppPublisherURL=https://github.com/roryekay/doomcraft
DefaultDirName={localappdata}\DoomCraft
DefaultGroupName=DoomCraft
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir=..\local\release
OutputBaseFilename=DoomCraft-Setup-0.1.0-Windows-x64
LicenseFile=..\LICENSE
UninstallDisplayIcon={app}\local\runtime\gzdoom.exe
Uninstallable=not IsSmokeTest

[Files]
Source: "..\local\installer-payload\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{userdesktop}\DoomCraft"; Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""{app}\installer\play.ps1"" -Game Doom"; IconFilename: "{app}\local\runtime\gzdoom.exe"; Check: not IsSmokeTest
Name: "{userdesktop}\DoomCraft II"; Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""{app}\installer\play.ps1"" -Game DoomII"; IconFilename: "{app}\local\runtime\gzdoom.exe"; Check: not IsSmokeTest
Name: "{group}\Set up DoomCraft"; Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""{app}\installer\setup.ps1"" -ShowUI"; Check: not IsSmokeTest
Name: "{group}\Uninstall DoomCraft"; Filename: "{uninstallexe}"; Check: not IsSmokeTest

[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""{app}\installer\setup.ps1"" -ShowUI"; Description: "Detect games and set up the Minecraft bridge"; Flags: postinstall nowait skipifsilent

[Code]
function IsSmokeTest: Boolean;
begin
  Result := ExpandConstant('{param:SMOKETEST|0}') = '1';
end;
