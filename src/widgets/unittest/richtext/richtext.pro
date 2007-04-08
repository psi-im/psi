# unittest helpers
TARGET = richtext
CONFIG += unittest
TESTBASEDIR = ../../../../unittest
include($$TESTBASEDIR/unittest.pri)

QT += gui
DEFINES += WIDGET_PLUGIN

SOURCES += main.cpp