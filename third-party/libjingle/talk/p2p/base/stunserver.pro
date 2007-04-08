TEMPLATE = app
INCLUDEPATH = ../../..
DEFINES += POSIX

include(../../../../../conf.pri)

# Input
SOURCES += \
	stunserver.cc \
	stunserver_main.cc \
	../../base/host.cc #\
#	../../base/socketaddresspair.cc

LIBS += ../../../liblibjingle.a
