CONFIG -= app_bundle

MOC_DIR = ../../src/.moc
OBJECTS_DIR = ../../src/.obj
UI_DIR = ../../src/.ui

CONFIG += pep
DEFINES += QT_STATICPLUGIN

include(../../conf.pri)
include(../../src/src.pri)
include(../../third-party/cppunit/cppunit.pri)

INCLUDEPATH += $$PWD

SOURCES += \
	unittest.cpp \
	unittestutil.cpp

include(../../src/capabilities/unittest/unittest.pri)
include(../../src/privacy/unittest/unittest.pri)
include(../../src/utilities/unittest/unittest.pri)
include(../../src/unittest/unittest.pri)

QMAKE_EXTRA_TARGETS = check
check.commands = \$(MAKE) && ./unittest

QMAKE_CLEAN += $(QMAKE_TARGET)

windows {
	include(../../conf_windows.pri)
	LIBS += -lWSock32 -lUser32 -lShell32 -lGdi32 -lAdvAPI32
	DEFINES += QT_STATICPLUGIN
	INCLUDEPATH += . # otherwise MSVC will fail to find "common.h" when compiling options/* stuff
}
