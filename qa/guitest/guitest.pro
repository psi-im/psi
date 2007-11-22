CONFIG -= app_bundle

MOC_DIR = ../../src/.moc
OBJECTS_DIR = ../../src/.obj
UI_DIR = ../../src/.ui

CONFIG += pep
DEFINES += QT_STATICPLUGIN

include(../../conf.pri)
include(../../src/src.pri)

SOURCES += \
	guitest.cpp \
	guitestmanager.cpp

include(../../src/privacy/guitest/guitest.pri)

QMAKE_CLEAN += ${QMAKE_TARGET}
