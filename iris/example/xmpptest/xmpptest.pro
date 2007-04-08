TEMPLATE = app
CONFIG += thread 
CONFIG -= app_bundle
TARGET  = xmpptest
QT += xml network qt3support                                                   
DEFINES += QT_STATICPLUGIN

MOC_DIR        = .moc
OBJECTS_DIR    = .obj
UI_DIR         = .ui

#DEFINES += CS_XMPP
DEFINES += XMPP_DEBUG

# Dependencies
include(../../../conf.pri)
windows:include(../../../conf_windows.pri)

!qca-static {
	CONFIG += crypto	
}
qca-static {
	# QCA
	DEFINES += QCA_STATIC
	QCA_CPP = ../../../third-party/qca
	INCLUDEPATH += $$QCA_CPP/include/QtCrypto
	LIBS += -L$$QCA_CPP -lqca_psi
	windows:LIBS += -lcrypt32
	mac:LIBS += -framework Security

	# QCA-OpenSSL
	contains(DEFINES, HAVE_OPENSSL) {
		include(../../../third-party/qca-openssl.pri)
	}
}

include(../../../cutestuff/cutestuff.pri)
include(../../iris.pri)
irisnet {
	include(../../irisnet/irisnet.pri)
}

SOURCES += xmpptest.cpp
INTERFACES += ui_test.ui

