TEMPLATE = app
QT += network
CONFIG += debug

CONFIG += Sparkle

include(../../AutoUpdater.pri)
include(../../../CocoaUtilities/CocoaUtilities.pri)

SOURCES += main.cpp
QMAKE_INFO_PLIST = Info.plist
RESOURCES += testapp.qrc
