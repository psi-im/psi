TEMPLATE = lib
CONFIG += staticlib
CONFIG += debug
DESTDIR=../..
TARGET = jingle_xmpp_psi

# Main settings
include(../../../../../conf.pri)

# Jingle settings
JINGLE_CPP = ../../
INCLUDEPATH += $$JINGLE_CPP 
DEFINES += POSIX
DEFINES += _DEBUG


# Sources
SOURCES += \
	$$JINGLE_CPP/talk/xmpp/constants.cc \
	$$JINGLE_CPP/talk/xmpp/jid.cc \
	$$JINGLE_CPP/talk/xmpp/ratelimitmanager.cc \
	$$JINGLE_CPP/talk/xmpp/saslmechanism.cc \
	$$JINGLE_CPP/talk/xmpp/xmppclient.cc \
	$$JINGLE_CPP/talk/xmpp/xmppengineimpl.cc \
	$$JINGLE_CPP/talk/xmpp/xmppengineimpl_iq.cc \
	$$JINGLE_CPP/talk/xmpp/xmpplogintask.cc \
	$$JINGLE_CPP/talk/xmpp/xmppstanzaparser.cc \
	$$JINGLE_CPP/talk/xmpp/xmpptask.cc
