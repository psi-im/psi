# NOTE: All files are relative to the win32 subdir

#QCADIR=..\..\qca-2.0-beta1
#QCA_OPENSSLDIR=..\..\qca-openssl
ASPELLDIR=C:\aspell
QTDIR=C:\Qt\4.2.0
MINGWDIR=c:\MingW
WINDOWSDIR=C:\Winnt

TARGET_DIR=psi

.PHONY: all
all:
	cd .. && $(MAKE)
	-mkdir $(TARGET_DIR)
	copy ..\src\release\psi.exe $(TARGET_DIR)
	rem copy $(QCADIR)\lib\qca2.dll $(TARGET_DIR)
	copy $(QTDIR)\bin\QtCore4.dll $(TARGET_DIR)
	copy $(QTDIR)\bin\QtGui4.dll $(TARGET_DIR)
	copy $(QTDIR)\bin\QtNetwork4.dll $(TARGET_DIR)
	copy $(QTDIR)\bin\QtXml4.dll $(TARGET_DIR)
	copy $(QTDIR)\bin\Qt3Support4.dll $(TARGET_DIR)
	copy $(QTDIR)\bin\QtSql4.dll $(TARGET_DIR)
	copy $(MINGWDIR)\bin\mingwm10.dll $(TARGET_DIR)
	copy ..\src\libpsi\tools\idle\win32\idleui.dll $(TARGET_DIR)
	xcopy /i /e /q /y ..\certs $(TARGET_DIR)\certs
	xcopy /i /e /q /y ..\iconsets $(TARGET_DIR)\iconsets
	xcopy /i /e /q /y ..\sound $(TARGET_DIR)\sound
	copy ..\COPYING $(TARGET_DIR)
	tod ..\README $(TARGET_DIR)\ReadMe.txt
	tod ..\INSTALL $(TARGET_DIR)\Install.txt
	rem -mkdir $(TARGET_DIR)\crypto
	rem copy $(QCA_OPENSSLDIR)\release\qca-openssl.dll $(TARGET_DIR)\crypto
	copy $(WINDOWSDIR)\system32\libeay32.dll $(TARGET_DIR)
	copy $(WINDOWSDIR)\system32\ssleay32.dll $(TARGET_DIR)
	-mkdir $(TARGET_DIR)\aspell
	copy $(ASPELLDIR)\bin\aspell-15.dll $(TARGET_DIR)
	xcopy /i /e /q /y $(ASPELLDIR)\dict $(TARGET_DIR)\aspell\dict
	xcopy /i /e /q /y $(ASPELLDIR)\data $(TARGET_DIR)\aspell\data
