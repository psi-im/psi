IRIS_BASE = ../../..

TEMPLATE = lib
QT      -= gui
TARGET   = irisnet
DESTDIR  = $$IRIS_BASE/lib
CONFIG  += staticlib create_prl

VERSION = 1.0.0

include(../../libbase.pri)
include(noncore.pri)

windows:!staticlib:DEFINES += IRISNET_MAKEDLL
staticlib:PRL_EXPORT_DEFINES += IRISNET_STATIC
