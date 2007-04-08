TEMPLATE = app
INCLUDEPATH = ../../..
DEFINES += POSIX

include(../../../../../conf.pri)

# Input
SOURCES += \
	relayserver.cc \
	relayserver_main.cc \
	../../base/host.cc \
	../../base/socketaddresspair.cc

LIBS += ../../../liblibjingle.a
