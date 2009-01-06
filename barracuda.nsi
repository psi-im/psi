;==================================================================
;
; Barracuda Networks IM Client Installer
; Create by Mike Fawcett - Barracuda Networks 2005
; http://www.barracuda.com
;
; NullSoft Install System 2 is required to make use of this script.
; http://nsis.sourceforge.net
;
;==================================================================

; Include Modern UI
!include "MUI.nsh"

!macro BIMAGE IMAGE PARMS
	Push $0
	GetTempFileName $0
	File /oname=$0 "${IMAGE}"
	SetBrandingImage ${PARMS} $0
	Delete $0
	Pop $0
!macroend

Name "Barracuda IM Client"
XPStyle on
AddBrandingImage left 100
;SetFont "Comic Sans MS" 8
Icon barracuda.ico
UninstallIcon barracuda.ico
OutFile Setup.exe
CRCCheck on
ComponentText "Ready to install the Barracuda IM Client on your computer."
;InstType Normal
DirText "You may specify your install folder."

;InstallDir $INSTDIR
InstallDir "C:\Program Files\Barracuda\"

; Automatically close the installer when done.
AutoCloseWindow true

; Hide the "show details" box
ShowInstDetails nevershow

Var runbarracuda

Page custom welcome
Page components compimage
Page directory 
Page instfiles

UninstPage uninstConfirm un.uninstimage
UninstPage instfiles

ReserveFile "welcome.ini"

LangString TEXT_IO_TITLE ${LANG_ENGLISH} "InstallOptions page"
LangString TEXT_IO_SUBTITLE ${LANG_ENGLISH} "This is a page created using the InstallOptions plug-in."

Function .onInit
    FindProcDLL::FindProc "barracuda.exe"
        StrCmp $R0 0 0 error
    Goto end
    error:
        MessageBox MB_OK|MB_ICONSTOP "Another Barracuda IM client is currently running.  You must quit the running program to continue with the Barracuda IM client installation."
        Quit
    end:

    ;Extract InstallOptions INI files
    !insertmacro MUI_INSTALLOPTIONS_EXTRACT "welcome.ini"
FunctionEnd

Function un.onInit
    FindProcDLL::FindProc "barracuda.exe"
        StrCmp $R0 0 0 error
    Goto end
    error:
        MessageBox MB_OK|MB_ICONSTOP "Another Barracuda IM client is currently running.  You must quit the running program to continue uninstalling the Barracuda IM client"
        Quit
    end:
FunctionEnd

Function welcome
    !insertmacro BIMAGE "install.bmp" /RESIZETOFIT
    !insertmacro MUI_HEADER_TEXT "$(TEXT_IO_TITLE)" "$(TEXT_IO_SUBTITLE)"
    !insertmacro MUI_INSTALLOPTIONS_DISPLAY "welcome.ini"
FunctionEnd

Function .onInstSuccess
    !define SHELLFOLDER "Software\Microsoft\Windows\CurrentVersion\Explorer"

    SetOutPath "$EXEDIR"

    ReadEnvStr $R0 "USERPROFILE"
    ReadEnvStr $R1 "HOMEDRIVE"
    ReadEnvStr $R2 "HOMEPATH"

    CreateDirectory "$R0\BarracudaData\profiles\default"
    CreateDirectory "$R0\BarracudaData\profiles\default\history"
    CopyFiles "config.xml" "$R0\BarracudaData\profiles\default\config.xml"

    CreateDirectory "$R1$R2\BarracudaData\profiles\default"
    CreateDirectory "$R1$R2\BarracudaData\profiles\default\history"
    CopyFiles "config.xml" "$R1$R2\BarracudaData\profiles\default\config.xml"

    WriteRegDWORD HKLM "SOFTWARE\Affinix\psi" "autoOpen" "1"
    WriteRegStr HKLM "SOFTWARE\Affinix\psi" "lastProfile" "default"

    StrCmp $runbarracuda "TRUE" Finish1
    Goto Finish2
    Finish1:
    SetOutPath $INSTDIR
    Exec "$INSTDIR\Barracuda.exe"
    Finish2:

FunctionEnd

Function compimage
    !insertmacro BIMAGE "install.bmp" /RESIZETOFIT
FunctionEnd

Function un.uninstimage
    !insertmacro BIMAGE "install.bmp" /RESIZETOFIT
FunctionEnd

Section "Barracuda Base"
SectionIn 1 RO
    SetOutPath "$INSTDIR"
    File barracuda.exe
    File barracuda.ico
    ;File msvcr80.dll
    ;File msvcp80.dll
    File idleui.dll
    File QtCore4.dll
    File QtNetwork4.dll
    File QtXml4.dll
    File QtSql4.dll
    File QtGui4.dll
    File QtOpenGL4.dll
    File Qt3Support4.dll
    File libeay32.dll
    File ssleay32.dll
    File qca2.dll
    ;File msvcr71d.dll
    ;File msvcp71d.dll
    ;File msvcrtd.dll
    File Readme.txt
    File Copying.txt
    ;File *.qm
    SetOutPath "$INSTDIR\Microsoft.VC80.CRT\"
    File Microsoft.VC80.CRT\Microsoft.VC80.CRT.manifest
    File Microsoft.VC80.CRT\msvcm80.dll
    File Microsoft.VC80.CRT\msvcp80.dll
    File Microsoft.VC80.CRT\msvcr80.dll
    SetOutPath "$INSTDIR\imageformats\"
    File imageformats\qjpeg4.dll
    File imageformats\qgif4.dll
    SetOutPath "$INSTDIR\crypto\"
    File crypto\qca-logger2.dll
    File crypto\qca-ossl2.dll
    File crypto\qca-wingss2.dll
    SetOutPath "$INSTDIR\certs\"
    File certs\README
    ;File certs\*.pem
    File certs\*.crt
    ;SetOutPath "$INSTDIR\crypto\"
    ;File crypto\qca-tls.dll
    SetOutPath "$INSTDIR\iconsets\"
    SetOutPath "$INSTDIR\iconsets\emoticons\"
    File iconsets\emoticons\*.jisp
    SetOutPath "$INSTDIR\iconsets\emoticons\default\"
    File iconsets\emoticons\default\icondef.xml
    File iconsets\emoticons\default\*.png
    SetOutPath "$INSTDIR\iconsets\roster\"
    File iconsets\roster\*.*
    SetOutPath "$INSTDIR\iconsets\roster\default\"
    File iconsets\roster\default\*.*
    SetOutPath "$INSTDIR\iconsets\system\"
    File iconsets\system\README
    SetOutPath "$INSTDIR\iconsets\system\default\"
    File iconsets\system\default\icondef.xml
    File iconsets\system\default\*.png
    SetOutPath "$INSTDIR\sound\"
    File sound\*.wav
    ;SetOutPath "$INSTDIR\docs\"
    ;File docs\*
    SetOutPath "$INSTDIR\gfx\"
    File gfx\*.png
    WriteUninstaller "$INSTDIR\uninst.exe"
SectionEnd

Section "Start Menu Items"
SectionIn 1
    SetOutPath $INSTDIR
    CreateShortCut "$SMPROGRAMS\Barracuda IM Client.lnk"\
                   "$INSTDIR\barracuda.exe"
SectionEnd

Section "Desktop Shortcuts"
SectionIn 1
    SetOutPath $INSTDIR
    CreateShortCut "$DESKTOP\Barracuda IM Client.lnk" \
                   "$INSTDIR\barracuda.exe"
SectionEnd

Section "Run Barracuda IM Client at boot"
SectionIn 1
    CreateShortCut "$SMSTARTUP\Barracuda IM Client.lnk"\
                   "$INSTDIR\barracuda.exe"
SectionEnd

Section "Run Barracuda IM Client after install"
SectionIn 1
    StrCpy $runbarracuda "TRUE"
SectionEnd

; Open readme file upon install completion
Section -post
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BarracudaIM"\
                     "DisplayName" "Barracuda Networks IM"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BarracudaIM"\
                     "UninstallString" "$INSTDIR\uninst.exe"
    MessageBox MB_OK "Thank you for installing the Barracuda IM Client."
;    Exec '"explorer.exe" "$INSTDIR\docs\readme.html"'
SectionEnd

UninstallText "Please click on the 'Uninstall' button to remove the Barracuda IM Client"

Section "Uninstall"
    ;Delete $INSTDIR\*.qm
    Delete $INSTDIR\uninst.exe
    Delete $INSTDIR\barracuda.exe
    Delete $INSTDIR\barracuda.ico
    Delete $INSTDIR\tmp-config.xml
    Delete $INSTDIR\Readme.txt
    ;Delete $INSTDIR\msvcr80.dll
    ;Delete $INSTDIR\msvcp80.dll
    Delete $INSTDIR\idleui.dll
    Delete $INSTDIR\QtCore4.dll
    Delete $INSTDIR\QtNetwork4.dll
    Delete $INSTDIR\QtXml4.dll
    Delete $INSTDIR\QtSql4.dll
    Delete $INSTDIR\QtGui4.dll
    Delete $INSTDIR\QtOpenGL4.dll
    Delete $INSTDIR\Qt3Support4.dll
    Delete $INSTDIR\libeay32.dll
    Delete $INSTDIR\ssleay32.dll
    Delete $INSTDIR\qca2.dll
    ;Delete $INSTDIR\msvcr71d.dll
    ;Delete $INSTDIR\msvcp71d.dll
    ;Delete $INSTDIR\msvcrtd.dll
    Delete $INSTDIR\Microsoft.VC80.CRT\Microsoft.VC80.CRT.manifest
    Delete $INSTDIR\Microsoft.VC80.CRT\msvcm80.dll
    Delete $INSTDIR\Microsoft.VC80.CRT\msvcp80.dll
    Delete $INSTDIR\Microsoft.VC80.CRT\msvcr80.dll
    Delete $INSTDIR\imageformats\qjpeg4.dll
    Delete $INSTDIR\imageformats\qgif4.dll
    Delete $INSTDIR\Readme.txt
    Delete $INSTDIR\Copying.txt
    Delete $INSTDIR\certs\*
    Delete $INSTDIR\iconsets\emoticons\default\*
    Delete $INSTDIR\iconsets\emoticons\*
    Delete $INSTDIR\iconsets\lightbulb\*
    Delete $INSTDIR\iconsets\stellar\*
    Delete $INSTDIR\iconsets\roster\aim\*
    Delete $INSTDIR\iconsets\roster\default\*
    Delete $INSTDIR\iconsets\roster\gadugadu\*
    Delete $INSTDIR\iconsets\roster\icq\*
    Delete $INSTDIR\iconsets\roster\msn\*
    Delete $INSTDIR\iconsets\roster\sms\*
    Delete $INSTDIR\iconsets\roster\stellar-icq\* 
    Delete $INSTDIR\iconsets\roster\transport\*
    Delete $INSTDIR\iconsets\roster\yahoo\*
    Delete $INSTDIR\iconsets\roster\README
    Delete $INSTDIR\iconsets\roster\*
    Delete $INSTDIR\iconsets\system\default\*
    Delete $INSTDIR\iconsets\roster\aqualight\*
    Delete $INSTDIR\iconsets\roster\aquaploum\*
    Delete $INSTDIR\iconsets\roster\bluekeramik\*
    Delete $INSTDIR\iconsets\roster\businessblack\*
    Delete $INSTDIR\iconsets\roster\psi_dudes\*
    Delete $INSTDIR\iconsets\system\aqualight\*
    Delete $INSTDIR\iconsets\system\aquaploum\*
    Delete $INSTDIR\iconsets\system\bluekeramik\*
    Delete $INSTDIR\iconsets\system\businessblack\*
    Delete $INSTDIR\iconsets\system\psi_dudes\*
    Delete $INSTDIR\iconsets\system\*
    Delete $INSTDIR\image\*
    Delete $INSTDIR\sound\*
    Delete $INSTDIR\crypto\*
    ;Delete $INSTDIR\docs\*
    Delete $INSTDIR\gfx\*
    RMDir $INSTDIR\certs
    RMDir $INSTDIR\iconsets\emoticons\default
    RMDir $INSTDIR\iconsets\emoticons
    RMDir $INSTDIR\iconsets\roster\aim
    RMDir $INSTDIR\iconsets\roster\default
    RMDir $INSTDIR\iconsets\roster\gadugadu
    RMDir $INSTDIR\iconsets\roster\icq
    RMDir $INSTDIR\iconsets\roster\msn
    RMDir $INSTDIR\iconsets\roster\sms
    RMDir $INSTDIR\iconsets\roster\stellar-icq
    RMDir $INSTDIR\iconsets\roster\transport
    RMDir $INSTDIR\iconsets\roster\yahoo
    RMDir $INSTDIR\iconsets\roster\aqualight
    RMDir $INSTDIR\iconsets\roster\aquaploum
    RMDir $INSTDIR\iconsets\roster\bluekeramik
    RMDir $INSTDIR\iconsets\roster\businessblack
    RMDir $INSTDIR\iconsets\roster\psi_dudes
    RMDir $INSTDIR\iconsets\roster
    RMDir $INSTDIR\iconsets\system\default
    RMDir $INSTDIR\iconsets\system\aqualight
    RMDir $INSTDIR\iconsets\system\aquaploum
    RMDir $INSTDIR\iconsets\system\bluekeramik
    RMDir $INSTDIR\iconsets\system\businessblack
    RMDir $INSTDIR\iconsets\system\psi_dudes
    RMDir $INSTDIR\iconsets\system
    RMDir $INSTDIR\iconsets\lightbulb
    RMDir $INSTDIR\iconsets\stellar
    RMDir $INSTDIR\iconsets
    RMDir $INSTDIR\image
    RMDir $INSTDIR\sound
    RMDir $INSTDIR\crypto
    ;RMDir $INSTDIR\docs
    RMDir $INSTDIR\gfx
    RMDir $INSTDIR\imageformats
    RMDir $INSTDIR\Microsoft.VC80.CRT
    RMDir $INSTDIR
    Delete "$SMPROGRAMS\Barracuda IM Client.lnk"
    Delete "$SMSTARTUP\Barracuda IM Client.lnk"
    Delete "$DESKTOP\Barracuda IM Client.lnk"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BarracudaIM"
SectionEnd
