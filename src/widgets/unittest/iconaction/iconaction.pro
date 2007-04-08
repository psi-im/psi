# unittest helpers
TARGET = iconaction
CONFIG += unittest
TESTBASEDIR = ../../../../unittest
include($$TESTBASEDIR/unittest.pri)

QT += gui
DEFINES += WIDGET_PLUGIN

INCLUDEPATH += ../..
DEPENDPATH  += ../..

SOURCES += \
	testiconaction.cpp \
	../../iconaction.cpp \
	../../iconwidget.cpp
	
HEADERS += \
	../../iconaction.h \
	../../iconwidget.h \
	../../icontoolbutton.h \
	../../iconbutton.h \
	../../iconsetdisplay.h \
	../../iconsetselect.h