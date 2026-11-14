#define SourceRoot ".."
#define AppVersion "1.0.0"

[Setup]
AppId={{E916D992-28A9-4FF9-A10B-8E589BCFAD95}
AppName=CXADC Tools
AppVersion={#AppVersion}
AppPublisher=Samuelzin-jgs
DefaultDirName={autopf}\CXADC Tools
DefaultGroupName=CXADC Tools
OutputDir={#SourceRoot}\release
OutputBaseFilename=CXADC-Tools-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
SetupLogging=yes
UninstallDisplayIcon={app}\Panel\Cxadc.ControlPanel.exe
LicenseFile={#SourceRoot}\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[CustomMessages]
english.DriverWarningTitle=Unsigned experimental driver
english.DriverWarning=The cxadc-win driver is experimental and unsigned. This setup will not change Secure Boot or Windows driver-signing settings. If Windows rejects the driver, the control panel and tools will still be installed.
english.DriverInstallFailed=The control panel and tools were installed, but Windows did not install the cxadc driver (pnputil exit code %1). Review the driver requirements in the installed README.
english.DriverLaunchFailed=The control panel and tools were installed, but Windows could not start pnputil to install the cxadc driver. Install it later from the Driver folder or Device Manager.
brazilianportuguese.DriverWarningTitle=Driver experimental sem assinatura
brazilianportuguese.DriverWarning=O driver cxadc-win é experimental e não tem assinatura. Este instalador não altera o Secure Boot nem as configurações de assinatura de drivers do Windows. Se o Windows rejeitar o driver, o painel e as ferramentas ainda serão instalados.
brazilianportuguese.DriverInstallFailed=O painel e as ferramentas foram instalados, mas o Windows não instalou o driver cxadc (código de saída do pnputil: %1). Consulte os requisitos do driver no README instalado.
brazilianportuguese.DriverLaunchFailed=O painel e as ferramentas foram instalados, mas o Windows não conseguiu iniciar o pnputil para instalar o driver cxadc. Instale-o depois pela pasta Driver ou pelo Gerenciador de Dispositivos.

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceRoot}\release\driver\*"; DestDir: "{app}\Driver"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceRoot}\release\app\*"; DestDir: "{app}\Panel"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceRoot}\release\bin\*"; DestDir: "{app}\Tools"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceRoot}\release\clockgen_firmware\*"; DestDir: "{app}\ClockgenFirmware"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceRoot}\release\license\*"; DestDir: "{app}\Licenses"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceRoot}\README.md"; DestDir: "{app}"; DestName: "README.md"; Flags: ignoreversion

[Icons]
Name: "{group}\CXADC Control Panel"; Filename: "{app}\Panel\Cxadc.ControlPanel.exe"; WorkingDir: "{app}\Panel"
Name: "{autodesktop}\CXADC Control Panel"; Filename: "{app}\Panel\Cxadc.ControlPanel.exe"; WorkingDir: "{app}\Panel"; Tasks: desktopicon

[Code]
procedure InitializeWizard;
var
  WarningPage: TOutputMsgWizardPage;
begin
  WarningPage := CreateOutputMsgPage(wpWelcome,
    CustomMessage('DriverWarningTitle'),
    CustomMessage('DriverWarningTitle'),
    CustomMessage('DriverWarning'));
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
  DriverInf: String;
  Parameters: String;
begin
  if CurStep = ssPostInstall then
  begin
    DriverInf := ExpandConstant('{app}\Driver\cxadc-win.inf');
    if FileExists(DriverInf) then
    begin
      Parameters := '/add-driver "' + DriverInf + '" /install';
      if Exec(ExpandConstant('{sys}\pnputil.exe'), Parameters, '', SW_HIDE,
        ewWaitUntilTerminated, ResultCode) then
      begin
        if (ResultCode <> 0) and (not WizardSilent) then
          MsgBox(FmtMessage(CustomMessage('DriverInstallFailed'), [IntToStr(ResultCode)]),
            mbError, MB_OK);
      end
      else if not WizardSilent then
        MsgBox(CustomMessage('DriverLaunchFailed'), mbError, MB_OK);
    end;
  end;
end;
