# linking half of psi :-/
INCLUDEPATH += \
	../..

# Yay! Almost all Psi sources are here!
PSI_CPP = ../..
include($$PSI_CPP/src.pri)

# iconsets
RESOURCES += $$PSI_CPP/../iconsets.qrc

CONFIG += unittest crypto
QT += gui network xml qt3support

DEFINES += UNIT_TEST

# unittest helpers
TESTBASEDIR = $$PSI_CPP/../unittest
include($$TESTBASEDIR/unittest.pri)

