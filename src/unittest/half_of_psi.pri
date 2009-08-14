# Yay! Almost all Psi sources are here!
PSI_CPP = $$PWD/..
include($$PSI_CPP/src.pri)

# linking half of psi :-/
INCLUDEPATH += $$PSI_CPP

# iconsets
RESOURCES += $$PSI_CPP/../iconsets.qrc

CONFIG += unittest crypto
QT += gui network xml

DEFINES += UNIT_TEST

# unittest helpers
TESTBASEDIR = $$PSI_CPP/../unittest
include($$TESTBASEDIR/unittest.pri)

