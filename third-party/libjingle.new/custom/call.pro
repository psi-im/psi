TEMPLATE = app
INCLUDEPATH = ../../..
DEFINES += POSIX

include(../../../../../conf.pri)

# Input
SOURCES += \
	call_main.cc \
	callclient.cc \
	console.cc \
	presenceouttask.cc \
	presencepushtask.cc \
	../login/xmppauth.cc \
	../login/xmpppump.cc \
	../login/xmppsocket.cc \
	../login/xmppthread.cc

LIBS += ../../../liblibjingle.a
