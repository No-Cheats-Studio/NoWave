#define AppName "NoWave"
#define AppVersion GetEnv("NOWAVE_RELEASE_TAG")
#define BuildDir AddBackslash(SourcePath) + "..\..\build\release\NoWave_artefacts\Release"
#define LicenseFile AddBackslash(SourcePath) + "..\..\build\release\_deps\ufbx-src\LICENSE"

#if AppVersion == ""
  #error NOWAVE_RELEASE_TAG environment variable must be set
#endif

[Setup]
AppId={{7D7228B1-05C9-4C96-BC44-CCF1A45B300A}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=NoWave
DefaultDirName={autopf}\NoWave
DefaultGroupName=NoWave
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\NoWave.exe
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputBaseFilename=NoWave-{#AppVersion}-Windows-Installer
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Files]
Source: "{#BuildDir}\Standalone\NoWave.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\VST3\NoWave.vst3\*"; DestDir: "{commoncf}\VST3\NoWave.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#LicenseFile}"; DestDir: "{app}\THIRD_PARTY_LICENSES"; DestName: "ufbx-LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\NoWave"; Filename: "{app}\NoWave.exe"
Name: "{autodesktop}\NoWave"; Filename: "{app}\NoWave.exe"; Tasks: desktopicon
