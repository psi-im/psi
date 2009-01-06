unix:include(confapp_unix.pri)
windows:include(confapp_win.pri)

include(common.pri)

mac:QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.3
