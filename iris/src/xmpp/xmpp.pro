IRIS_BASE = ../..

TEMPLATE = lib
#QT      -= gui
TARGET   = iris
DESTDIR  = $$IRIS_BASE/lib
CONFIG  += staticlib create_prl
OBJECTS_DIR = .obj
MOC_DIR = .moc
UI_DIR = .ui

VERSION = 1.0.0

# static targets don't pick up prl defines
DEFINES += IRISNET_STATIC

include(xmpp.pri)
