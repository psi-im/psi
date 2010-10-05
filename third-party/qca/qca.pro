TEMPLATE = lib
QT      -= gui
CONFIG += staticlib
DEFINES += QCA_STATIC
TARGET = qca_psi

QCA_BASE = qca
QCA_INCBASE = $$QCA_BASE/include
QCA_SRCBASE = $$QCA_BASE/src

MOC_DIR        = .moc
OBJECTS_DIR    = .obj

QCA_INC = $$QCA_INCBASE/QtCrypto
QCA_CPP = $$QCA_SRCBASE
INCLUDEPATH += $$QCA_INC $$QCA_CPP

windows {
	# Explicitly remove d_and_r,  so the lib gets built in the right place
	CONFIG -= debug_and_release

	# Set explicit targets, to ensure a correct name for MSVC
}

# botantools
include($$QCA_SRCBASE/botantools/botantools.pri)

PRIVATE_HEADERS += \
	$$QCA_CPP/qca_plugin.h \
	$$QCA_CPP/qca_safeobj.h \
	$$QCA_CPP/qca_systemstore.h

PUBLIC_HEADERS += \
	$$QCA_INC/qca_export.h \
	$$QCA_INC/qca_support.h \
	$$QCA_INC/qca_tools.h \
	$$QCA_INC/qca_core.h \
	$$QCA_INC/qca_textfilter.h \
	$$QCA_INC/qca_basic.h \
	$$QCA_INC/qca_publickey.h \
	$$QCA_INC/qca_cert.h \
	$$QCA_INC/qca_keystore.h \
	$$QCA_INC/qca_securelayer.h \
	$$QCA_INC/qca_securemessage.h \
	$$QCA_INC/qcaprovider.h \
	$$QCA_INC/qpipe.h

HEADERS += $$PRIVATE_HEADERS $$PUBLIC_HEADERS

# do support first
SOURCES += \
	$$QCA_CPP/support/syncthread.cpp \
	$$QCA_CPP/support/logger.cpp \
	$$QCA_CPP/support/synchronizer.cpp \
	$$QCA_CPP/support/dirwatch.cpp

SOURCES += \
	$$QCA_CPP/qca_tools.cpp \
	$$QCA_CPP/qca_core.cpp \
	$$QCA_CPP/qca_textfilter.cpp \
	$$QCA_CPP/qca_plugin.cpp \
	$$QCA_CPP/qca_basic.cpp \
	$$QCA_CPP/qca_publickey.cpp \
	$$QCA_CPP/qca_cert.cpp \
	$$QCA_CPP/qca_keystore.cpp \
	$$QCA_CPP/qca_securelayer.cpp \
	$$QCA_CPP/qca_safeobj.cpp \
	$$QCA_CPP/qca_securemessage.cpp \
	$$QCA_CPP/qca_default.cpp \
	$$QCA_CPP/support/qpipe.cpp \
	$$QCA_CPP/support/console.cpp

unix:!mac: {
	SOURCES += $$QCA_CPP/qca_systemstore_flatfile.cpp
}
windows: {
	SOURCES += $$QCA_CPP/qca_systemstore_win.cpp
}
mac: {
	SOURCES += $$QCA_CPP/qca_systemstore_mac.cpp
}

include(../../conf.pri)

qc_universal:contains(QT_CONFIG,x86):contains(QT_CONFIG,ppc) {
	CONFIG += x86 ppc
	QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.5.sdk
	QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5
}
