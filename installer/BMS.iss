#ifndef MyAppName
#define MyAppName "BMS Console"
#endif

#ifndef MyAppVersion
#define MyAppVersion "2.2.0"
#endif

#ifndef MyAppPublisher
#define MyAppPublisher "antidote>_<"
#endif

#ifndef SourceDir
#define SourceDir "..\\dist\\app"
#endif

#ifndef OutputDir
#define OutputDir "..\\dist\\installer"
#endif

#define DistIconFile AddBackslash(SourceDir) + "bms_app.ico"
#if FileExists(DistIconFile)
	#define HasDistIcon
#endif

[Setup]
AppId={{D7076602-7577-4F09-A746-7311EA96F6EA}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=BMS-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#ifdef HasDistIcon
SetupIconFile={#DistIconFile}
#endif

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\\appBMS.exe"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\\appBMS.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\\appBMS.exe"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
