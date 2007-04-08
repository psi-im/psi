TEMPLATE = app
CONFIG += qt thread console
TARGET = conntest
QT += qt3support network xml

# Dependencies
CONFIG += crypto
include(../../../cutestuff/cutestuff.pri)
include(../../iris.pri)
irisnet {
	include(../../irisnet/irisnet.pri)
}

SOURCES += conntest.cpp

