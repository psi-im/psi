QT *= network
INCLUDEPATH += $$PWD

# libidn
#LIBS += -lidn

include(jdns/jdns.pri)

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

SOURCES += \
	$$PWD/netinterface_unix.cpp \
	$$PWD/netnames_jdns.cpp

include(legacy/legacy.pri)

