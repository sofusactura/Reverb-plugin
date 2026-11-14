; Inno Setup script for the Windows installer.
; Built in CI with:  ISCC /DVersion=x.y.z /DSourceDir=<path to artefacts\Release> installer\windows.iss

#ifndef Version
  #define Version "0.1.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\build\HybridGateReverb_artefacts\Release"
#endif

[Setup]
AppId={{8F3C1F0A-5B0E-4E57-9C61-6E2B7D1A9A10}
AppName=HybridGate Reverb
AppVersion={#Version}
AppPublisher=Actura
DefaultDirName={autopf}\HybridGate Reverb
DefaultGroupName=HybridGate Reverb
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\build\installer
OutputBaseFilename=HybridGateReverb-{#Version}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Types]
Name: "full"; Description: "VST3 plugin and standalone app"
Name: "plugin"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (FL Studio, Ableton Live, Cubase, Reaper, ...)"; Types: full plugin custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full

[Files]
Source: "{#SourceDir}\VST3\HybridGate Reverb.vst3\*"; DestDir: "{commoncf64}\VST3\HybridGate Reverb.vst3"; Components: vst3; Flags: recursesubdirs ignoreversion
Source: "{#SourceDir}\Standalone\HybridGate Reverb.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\docs\MANUAL.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\interface.png"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\HybridGate Reverb"; Filename: "{app}\HybridGate Reverb.exe"; Components: standalone
Name: "{group}\User Manual"; Filename: "{app}\MANUAL.md"
