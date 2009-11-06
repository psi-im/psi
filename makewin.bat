@echo off

echo Running qmake ...
qmake psi.pro
if errorlevel 1 goto ERROR1
echo Compiling ...
call make
if errorlevel 1 goto ERROR2

mkdir barracudawin
copy src\release\barracuda.exe barracudawin\Barracuda.exe
rem copy \windows\system32\msvcr71.dll barracudawin
rem copy \windows\system32\msvcp71.dll barracudawin
rem copy "c:\program files\microsoft visual studio 8\vc\redist\x86\microsoft.vc80.crt\msvcr80.dll" barracudawin
rem copy "c:\program files\microsoft visual studio 8\vc\redist\x86\microsoft.vc80.crt\msvcp80.dll" barracudawin
rem xcopy /e "c:\program files\microsoft visual studio 8\vc\redist\x86\Microsoft.VC80.CRT" "barracudawin\Microsoft.VC80.CRT\"
rem xcopy /e "\dev\Microsoft.VC80.CRT" "barracudawin\Microsoft.VC80.CRT\"
copy C:\MinGW\bin\mingwm10.dll barracudawin
copy %QTDIR%\bin\QtCore4.dll barracudawin
copy %QTDIR%\bin\QtNetwork4.dll barracudawin
copy %QTDIR%\bin\QtXml4.dll barracudawin
copy %QTDIR%\bin\QtSql4.dll barracudawin
copy %QTDIR%\bin\QtGui4.dll barracudawin
copy %QTDIR%\bin\QtOpenGL4.dll barracudawin
copy %QTDIR%\bin\Qt3Support4.dll barracudawin
mkdir barracudawin\imageformats
copy %QTDIR%\plugins\imageformats\qjpeg4.dll barracudawin\imageformats
copy %QTDIR%\plugins\imageformats\qgif4.dll barracudawin\imageformats
copy \dev\local\bin\libeay32.dll barracudawin
copy \dev\local\bin\libssl32.dll barracudawin
copy \dev\local\bin\qca2.dll barracudawin
mkdir barracudawin\crypto
rem copy %QTDIR%\plugins\crypto\qca-logger2.dll barracudawin\crypto
copy %QTDIR%\plugins\crypto\qca-ossl2.dll barracudawin\crypto
rem copy %QTDIR%\plugins\crypto\qca-wingss2.dll barracudawin\crypto
rem copy ..\qca\qca*.dll barracudawin
rem copy psi\src\tools\idle\win32\idleui.dll barracudawin
copy win32\idleui.dll barracudawin
xcopy /e certs barracudawin\certs\
xcopy /e iconsets barracudawin\iconsets\
xcopy /e sound barracudawin\sound\
xcopy /e gfx barracudawin\gfx\
xcopy /e c:\dev\psi_snap_psimedia\* barracudawin\
win32\tod README barracudawin\Readme.txt
win32\tod INSTALL barracudawin\Install.txt
win32\tod COPYING barracudawin\Copying.txt
copy barracuda.ico barracudawin
copy welcome.ini barracudawin
copy install.bmp barracudawin
copy barracuda.nsi barracudawin

echo 'barracudawin' created successfully
goto END

:ERROR1
echo Error running qmake..  Are you sure you have Qt set up?
goto END

:ERROR2
echo Compile failed, stopping.
goto END

:END

