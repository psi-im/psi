Unicode true
RequestExecutionLevel admin
ManifestDPIAware true
SetCompressor /SOLID lzma

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"

!ifdef LEGACY_WIN7
  ManifestSupportedOS Win7 Win8 Win8.1 Win10
  !define REQUIRED_OS "Windows 7 SP1 or newer"
!else
  ManifestSupportedOS Win10
  !define REQUIRED_OS "Windows 10 or newer"
!endif

!ifndef APP_VERSION
  !define APP_VERSION "development"
!endif
!ifndef APP_VERSION_NUMERIC
  !define APP_VERSION_NUMERIC "0.0.0.0"
!endif
!ifndef SOURCE_DIR
  !error "SOURCE_DIR must point to the staged Psi runtime tree"
!endif
!ifndef UNINSTALL_MANIFEST
  !error "UNINSTALL_MANIFEST must point to generated uninstall commands"
!endif
!ifndef OUTPUT_FILE
  !define OUTPUT_FILE "psi-win64-setup.exe"
!endif
!ifndef APP_ICON
  !error "APP_ICON must point to the Psi .ico file"
!endif
!ifndef LICENSE_FILE
  !error "LICENSE_FILE must point to the Psi license text"
!endif

!define APP_NAME "Psi"
!define APP_PUBLISHER "Psi Team"
!define APP_URL "https://psi-im.org/"
!define APP_REGKEY "Software\psi-im.org\Psi"
!define APP_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Psi"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\Psi"
InstallDirRegKey HKLM "${APP_REGKEY}" "InstallLocation"
BrandingText "${APP_NAME} XMPP Client"

VIProductVersion "${APP_VERSION_NUMERIC}"
VIFileVersion "${APP_VERSION_NUMERIC}"
VIAddVersionKey /LANG=1033 "ProductName" "${APP_NAME}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1033 "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1033 "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey /LANG=1033 "FileDescription" "${APP_NAME} x64 Installer"
VIAddVersionKey /LANG=1033 "LegalCopyright" "GNU GPL v2 or later"

!define MUI_ABORTWARNING
!define MUI_ICON "${APP_ICON}"
!define MUI_UNICON "${APP_ICON}"
!define MUI_FINISHPAGE_RUN "$INSTDIR\psi.exe"
!define MUI_FINISHPAGE_LINK "Visit the Psi website"
!define MUI_FINISHPAGE_LINK_LOCATION "${APP_URL}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${LICENSE_FILE}"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "Psi" SecPsi
  SectionIn RO
  SetShellVarContext all
  SetRegView 64

!ifdef VC_REDIST
  SetOutPath "$TEMP"
  File /oname=psi-vc_redist.x64.exe "${VC_REDIST}"
  ExecWait '"$TEMP\psi-vc_redist.x64.exe" /install /quiet /norestart' $0
  Delete "$TEMP\psi-vc_redist.x64.exe"
  ${If} $0 != 0
  ${AndIf} $0 != 3010
    MessageBox MB_OK|MB_ICONSTOP "Microsoft Visual C++ Redistributable installation failed (exit code $0)."
    Abort
  ${EndIf}
!endif

  SetOutPath "$INSTDIR"
  File /r "${SOURCE_DIR}\*"
  WriteUninstaller "$INSTDIR\uninstall.exe"

  WriteRegStr HKLM "${APP_REGKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${APP_REGKEY}" "Version" "${APP_VERSION}"

  WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "URLInfoAbout" "${APP_URL}"
  WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\psi.exe"
  WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKLM "${APP_UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${APP_UNINSTALL_KEY}" "NoRepair" 1

  CreateDirectory "$SMPROGRAMS\Psi"
  CreateShortcut "$SMPROGRAMS\Psi\Psi.lnk" "$INSTDIR\psi.exe"
  CreateShortcut "$SMPROGRAMS\Psi\Uninstall Psi.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section /o "Desktop shortcut" SecDesktop
  SetShellVarContext all
  CreateShortcut "$DESKTOP\Psi.lnk" "$INSTDIR\psi.exe"
SectionEnd

Section /o "Start Psi with Windows" SecAutostart
  SetShellVarContext current
  CreateShortcut "$SMSTARTUP\Psi.lnk" "$INSTDIR\psi.exe"
  SetShellVarContext all
SectionEnd

Section "Uninstall"
  SetShellVarContext all
  SetRegView 64

  Delete "$DESKTOP\Psi.lnk"
  SetShellVarContext current
  Delete "$SMSTARTUP\Psi.lnk"
  SetShellVarContext all

  Delete "$SMPROGRAMS\Psi\Psi.lnk"
  Delete "$SMPROGRAMS\Psi\Uninstall Psi.lnk"
  RMDir "$SMPROGRAMS\Psi"

  !include "${UNINSTALL_MANIFEST}"

  DeleteRegKey HKLM "${APP_UNINSTALL_KEY}"
  DeleteRegKey HKLM "${APP_REGKEY}"

  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd

Function CheckPsiNotRunning
  Push $0
  Push $1
check_again:
  nsExec::ExecToStack 'cmd /C tasklist /FI "IMAGENAME eq psi.exe" /NH | find /I "psi.exe" >NUL'
  Pop $0
  Pop $1
  ${If} $0 == 0
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION \
      "Psi is running. Exit Psi before continuing." \
      IDRETRY check_again IDCANCEL cancel_install
  ${EndIf}
  Pop $1
  Pop $0
  Return
cancel_install:
  Pop $1
  Pop $0
  Abort
FunctionEnd

Function DetectPreviousInstall
  Push $0
  ReadRegStr $0 HKLM "${APP_REGKEY}" "InstallLocation"
  ${If} $0 == ""
    ReadRegStr $0 HKLM "${APP_REGKEY}" ""
  ${EndIf}
  ${If} $0 == ""
    ReadRegStr $0 HKCU "${APP_REGKEY}" "InstallLocation"
  ${EndIf}
  ${If} $0 == ""
    ReadRegStr $0 HKCU "${APP_REGKEY}" ""
  ${EndIf}
  ${If} $0 != ""
    StrCpy $INSTDIR $0
  ${EndIf}
  Pop $0
FunctionEnd

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "This package contains only the 64-bit build of Psi."
    Abort
  ${EndIf}

!ifdef LEGACY_WIN7
  ${IfNot} ${AtLeastWin7}
    MessageBox MB_OK|MB_ICONSTOP "This Psi package requires ${REQUIRED_OS}."
    Abort
  ${EndIf}
!else
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_OK|MB_ICONSTOP "This Psi package requires ${REQUIRED_OS}. Use the legacy Windows 7 package on older systems."
    Abort
  ${EndIf}
!endif

  SetRegView 64
  SetShellVarContext all
  Call DetectPreviousInstall
  Call CheckPsiNotRunning

  IfFileExists "$INSTDIR\uninstall.exe" 0 no_previous_install
  MessageBox MB_YESNO|MB_ICONQUESTION \
    "An existing Psi installation was found. Remove it before installing ${APP_VERSION}?" \
    IDYES remove_previous IDNO cancel_upgrade
remove_previous:
  ExecWait '"$INSTDIR\uninstall.exe" /S _?=$INSTDIR' $0
  ${If} $0 != 0
    MessageBox MB_OK|MB_ICONSTOP "The previous Psi installation could not be removed (exit code $0)."
    Abort
  ${EndIf}
  Goto no_previous_install
cancel_upgrade:
  Abort
no_previous_install:
FunctionEnd

Function un.onInit
  SetRegView 64
  SetShellVarContext all
  Call un.CheckPsiNotRunning
FunctionEnd

Function un.CheckPsiNotRunning
  Push $0
  Push $1
un_check_again:
  nsExec::ExecToStack 'cmd /C tasklist /FI "IMAGENAME eq psi.exe" /NH | find /I "psi.exe" >NUL'
  Pop $0
  Pop $1
  ${If} $0 == 0
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION \
      "Psi is running. Exit Psi before uninstalling." \
      IDRETRY un_check_again IDCANCEL un_cancel
  ${EndIf}
  Pop $1
  Pop $0
  Return
un_cancel:
  Pop $1
  Pop $0
  Abort
FunctionEnd
