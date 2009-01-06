IRIS_BASE = ../../..

TEMPLATE = lib
QT      -= gui
TARGET   = irisnetcore
DESTDIR  = $$IRIS_BASE/lib
windows:DLLDESTDIR = $$IRIS_BASE/bin

VERSION = 1.0.0

include(../../libbase.pri)
include(corelib.pri)

# fixme: irisnetcore builds as dll or bundled, never static?
CONFIG += create_prl
windows:!staticlib:DEFINES += IRISNET_MAKEDLL
staticlib:PRL_EXPORT_DEFINES += IRISNET_STATIC
