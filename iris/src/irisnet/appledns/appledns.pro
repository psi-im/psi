IRIS_BASE = ../../..
include(../../libbase.pri)

TEMPLATE = lib
CONFIG += plugin
QT -= gui
DESTDIR = $$IRIS_BASE/plugins

VERSION = 1.0.0

INCLUDEPATH *= $$PWD/../corelib
LIBS += -L$$IRIS_BASE/lib -lirisnetcore

include(appledns.pri)
