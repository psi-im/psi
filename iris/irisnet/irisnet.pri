QT *= network
CONFIG *= crypto

# libidn
#LIBS += -lidn

include(jdns/jdns.pri)

INCLUDEPATH += $$PWD

HEADERS += \
	$$PWD/jdnsshared.h \
	$$PWD/irisnetexport.h \
	$$PWD/irisnetplugin.h \
	$$PWD/irisnetglobal.h \
	$$PWD/irisnetglobal_p.h \
	$$PWD/processquit.h \
	$$PWD/netinterface.h \
	$$PWD/netnames.h

SOURCES += \
	$$PWD/jdnsshared.cpp \
	$$PWD/irisnetplugin.cpp \
	$$PWD/irisnetglobal.cpp \
	$$PWD/processquit.cpp \
	$$PWD/netinterface.cpp \
	$$PWD/netnames.cpp

SOURCES += $$PWD/netnames_jdns.cpp

unix {
	SOURCES += $$PWD/netinterface_unix.cpp
}

include(legacy/legacy.pri)

