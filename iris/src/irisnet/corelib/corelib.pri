QT *= network

# libidn
#LIBS += -lidn

include(../../jdns/jdns.pri)
INCLUDEPATH += $$PWD/../../jdns

HEADERS += \
	$$PWD/jdnsshared.h \
	$$PWD/objectsession.h \
	$$PWD/irisnetexport.h \
	$$PWD/irisnetplugin.h \
	$$PWD/irisnetglobal.h \
	$$PWD/irisnetglobal_p.h \
	$$PWD/netinterface.h \
	$$PWD/netavailability.h \
	$$PWD/netnames.h

SOURCES += \
	$$PWD/jdnsshared.cpp \
	$$PWD/objectsession.cpp \
	$$PWD/irisnetplugin.cpp \
	$$PWD/irisnetglobal.cpp \
	$$PWD/netinterface.cpp \
	$$PWD/netavailability.cpp \
	$$PWD/netnames.cpp

unix {
	SOURCES += \
		$$PWD/netinterface_unix.cpp
}

windows {
	SOURCES += \
		$$PWD/netinterface_win.cpp
}

SOURCES += \
	$$PWD/netnames_jdns.cpp

#include(legacy/legacy.pri)

appledns:appledns_bundle {
	DEFINES += APPLEDNS_STATIC
	include(../appledns/appledns.pri)
}
