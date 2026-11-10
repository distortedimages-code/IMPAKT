; IMPAKT's Windows installer (Inno Setup 6): the VST3 plug-in (with CHIMERA's bank inside its bundle) and the
; standalone app (its bank beside it). Built by the CI:
;   iscc /DVersion=0.87.0 /DArt=<IMPAKT_artefacts\Release> /DRoot=<repository> /O<output> packaging\windows\impakt.iss
; -> IMPAKT-Windows-Setup.exe. Not signed: Windows SmartScreen asks once ("More info", "Run anyway").

#ifndef Version
  #define Version "0.0.0"
#endif

[Setup]
AppId={{6F0B9E2A-6C1D-4E8B-9C47-1A2B3C4D5E6F}
AppName=IMPAKT
AppVersion={#Version}
AppVerName=IMPAKT {#Version}
AppPublisher=IMPAKT
AppPublisherURL=https://distortedimages-code.github.io/IMPAKT/
AppSupportURL=https://github.com/distortedimages-code/IMPAKT/issues
AppUpdatesURL=https://github.com/distortedimages-code/IMPAKT/releases
DefaultDirName={autopf}\IMPAKT
DefaultGroupName=IMPAKT
DisableProgramGroupPage=yes
DisableWelcomePage=no
OutputBaseFilename=IMPAKT-Windows-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0.17763
UninstallDisplayName=IMPAKT {#Version}
UninstallDisplayIcon={app}\IMPAKT.exe
SetupIconFile={#Root}\brand\impakt.ico
CloseApplications=no
SetupLogging=yes
ShowLanguageDialog=auto
LanguageDetectionMethod=uilanguage

[Languages]
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "{cm:TypeFull}"
Name: "custom"; Description: "{cm:TypeCustom}"; Flags: iscustom

[Components]
Name: "vst3"; Description: "{cm:CompVst3}"; Types: full custom
Name: "app";  Description: "{cm:CompApp}";  Types: full custom

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: app; Flags: unchecked

[Files]
Source: "{#Art}\VST3\IMPAKT.vst3\*"; DestDir: "{commoncf64}\VST3\IMPAKT.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Art}\Standalone\IMPAKT.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#Art}\Standalone\bank\*"; DestDir: "{app}\bank"; Components: app; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\IMPAKT"; Filename: "{app}\IMPAKT.exe"; Components: app
Name: "{autodesktop}\IMPAKT"; Filename: "{app}\IMPAKT.exe"; Components: app; Tasks: desktopicon

[Run]
Filename: "{app}\IMPAKT.exe"; Description: "{cm:LaunchProgram,IMPAKT}"; Components: app; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\IMPAKT.vst3"

[CustomMessages]
fr.TypeFull=Installation complète (VST3 et application)
fr.TypeCustom=Installation personnalisée
fr.CompVst3=Plugin VST3 (dans Fichiers communs\VST3)
fr.CompApp=Application autonome
fr.CloseHosts=Fermez votre logiciel de musique (Live, Bitwig, Reaper, FL Studio…) avant de continuer : IMPAKT ne peut pas être remplacé tant qu'il y est chargé.
en.TypeFull=Full installation (VST3 and app)
en.TypeCustom=Custom installation
en.CompVst3=VST3 plug-in (in Common Files\VST3)
en.CompApp=Standalone app
en.CloseHosts=Close your music software (Live, Bitwig, Reaper, FL Studio…) before going on: IMPAKT cannot be replaced while it is loaded there.

[Code]
procedure InitializeWizard();
begin
  WizardForm.WelcomeLabel2.Caption := WizardForm.WelcomeLabel2.Caption + #13#10#13#10 + CustomMessage('CloseHosts');
end;
