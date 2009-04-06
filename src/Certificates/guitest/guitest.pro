QT += xml

include(../../../conf.pri)
include(../Certificates.pri)

qca-static {
	include(../../../third-party/qca/qca.pri)
}
else {
	CONFIG += crypto
}

SOURCES += \
	$$PWD/main.cpp

RESOURCES += guitest.qrc
