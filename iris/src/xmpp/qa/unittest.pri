#
# Declares all common settings/includes for a unittest module.
#
# Include this file from your module's unittest project file to create a 
# standalone checker for the module.
#

include($$PWD/qttestutil/qttestutil.pri)
include($$PWD/../common.pri)

QT += testlib
QT -= gui
CONFIG -= app_bundle

INCLUDEPATH *= $$PWD
DEPENDPATH *= $$PWD

TARGET = checker

SOURCES += \
	$$PWD/checker.cpp

QMAKE_EXTRA_TARGETS = check
check.commands = \$(MAKE) && ./checker

QMAKE_CLEAN += $(QMAKE_TARGET)
