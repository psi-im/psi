IRIS_BASE = ../..
include(../../confapp.pri)

CONFIG += console
CONFIG -= app_bundle
QT -= gui
QT += network
DESTDIR = ../../bin

CONFIG *= depend_prl

INCLUDEPATH += ../../include ../../include/iris

iris_bundle:{
	include(../../src/irisnet/noncore/noncore.pri)
}
else {
	LIBS += -L$$IRIS_BASE/lib -lirisnet
}

# qt < 4.4 doesn't enable link_prl by default.  we could just enable it,
#   except that in 4.3 or earlier the link_prl feature is too aggressive and
#   pulls in unnecessary deps.  so, for 4.3 and earlier, we'll just explicitly
#   specify the stuff the prl should have given us.
# also, mingw seems to have broken prl support??
win32-g++|contains($$list($$[QT_VERSION]), 4.0.*|4.1.*|4.2.*|4.3.*) {
	DEFINES += IRISNET_STATIC             # from irisnet
	windows:LIBS += -lWs2_32 -lAdvapi32   # from jdns
}

SOURCES += main.cpp
