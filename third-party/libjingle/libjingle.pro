TEMPLATE = lib
CONFIG += staticlib
CONFIG += debug
TARGET = jingle_psi

target.extra = true

exists(../../conf.pri) {
	include(../../conf.pri)
}

JINGLE_CPP = .
INCLUDEPATH += $$JINGLE_CPP $$JINGLE_CPP/talk/third_party/mediastreamer
DEFINES += POSIX
OBJECTS_DIR = $$JINGLE_CPP/.obj

# Base
SOURCES += \
	$$JINGLE_CPP/talk/base/asyncpacketsocket.cc \
	$$JINGLE_CPP/talk/base/asynctcpsocket.cc \
	$$JINGLE_CPP/talk/base/asyncudpsocket.cc \
	$$JINGLE_CPP/talk/base/base64.cc \
	$$JINGLE_CPP/talk/base/bytebuffer.cc \
	$$JINGLE_CPP/talk/base/md5c.c \
	$$JINGLE_CPP/talk/base/messagequeue.cc \
	$$JINGLE_CPP/talk/base/network.cc \
	$$JINGLE_CPP/talk/base/physicalsocketserver.cc \
	$$JINGLE_CPP/talk/base/socketadapters.cc \
	$$JINGLE_CPP/talk/base/socketaddress.cc \
	$$JINGLE_CPP/talk/base/task.cc \
	$$JINGLE_CPP/talk/base/taskrunner.cc \
	$$JINGLE_CPP/talk/base/thread.cc \
	$$JINGLE_CPP/talk/base/time.cc

# Not needed ?
#$$JINGLE_CPP/talk/base/socketaddresspair.cc \
#$$JINGLE_CPP/talk/base/host.cc \

# P2P Base
SOURCES += \
	$$JINGLE_CPP/talk/p2p/base/helpers.cc \
	$$JINGLE_CPP/talk/p2p/base/p2psocket.cc \
	$$JINGLE_CPP/talk/p2p/base/port.cc \
	$$JINGLE_CPP/talk/p2p/base/relayport.cc \
	$$JINGLE_CPP/talk/p2p/base/session.cc \
	$$JINGLE_CPP/talk/p2p/base/sessionmanager.cc \
	$$JINGLE_CPP/talk/p2p/base/socketmanager.cc \
	$$JINGLE_CPP/talk/p2p/base/stun.cc \
	$$JINGLE_CPP/talk/p2p/base/stunport.cc \
	$$JINGLE_CPP/talk/p2p/base/stunrequest.cc \
	$$JINGLE_CPP/talk/p2p/base/tcpport.cc \
	$$JINGLE_CPP/talk/p2p/base/udpport.cc
	
# P2P Client
SOURCES += \
	$$JINGLE_CPP/talk/p2p/client/basicportallocator.cc \
	$$JINGLE_CPP/talk/p2p/client/sessionclient.cc \
	$$JINGLE_CPP/talk/p2p/client/socketmonitor.cc


# XMLLite
SOURCES += \
	$$JINGLE_CPP/talk/xmllite/qname.cc \
	$$JINGLE_CPP/talk/xmllite/xmlbuilder.cc \
	$$JINGLE_CPP/talk/xmllite/xmlconstants.cc \
	$$JINGLE_CPP/talk/xmllite/xmlelement.cc \
	$$JINGLE_CPP/talk/xmllite/xmlnsstack.cc \
	$$JINGLE_CPP/talk/xmllite/xmlparser.cc \
	$$JINGLE_CPP/talk/xmllite/xmlprinter.cc

# XMPP
SOURCES += \
	$$JINGLE_CPP/talk/xmpp/constants.cc \
	$$JINGLE_CPP/talk/xmpp/jid.cc \
	$$JINGLE_CPP/talk/xmpp/saslmechanism.cc \
	$$JINGLE_CPP/talk/xmpp/xmppclient.cc \
	$$JINGLE_CPP/talk/xmpp/xmppengineimpl.cc \
	$$JINGLE_CPP/talk/xmpp/xmppengineimpl_iq.cc \
	$$JINGLE_CPP/talk/xmpp/xmpplogintask.cc \
	$$JINGLE_CPP/talk/xmpp/xmppstanzaparser.cc \
	$$JINGLE_CPP/talk/xmpp/xmpptask.cc

# Session
SOURCES += \
		$$JINGLE_CPP/talk/session/phone/call.cc \
		$$JINGLE_CPP/talk/session/phone/audiomonitor.cc \
		$$JINGLE_CPP/talk/session/phone/phonesessionclient.cc \
		$$JINGLE_CPP/talk/session/phone/channelmanager.cc \
		$$JINGLE_CPP/talk/session/phone/linphonemediaengine.cc \
		$$JINGLE_CPP/talk/session/phone/voicechannel.cc
	
#contains(DEFINES, HAVE_PORTAUDIO) {
#	SOURCES += \
#		$$JINGLE_CPP/talk/session/phone/portaudiomediaengine.cc
#}


# Mediastreamer
SOURCES += \
	$$JINGLE_CPP/talk/third_party/mediastreamer/audiostream.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/ms.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msAlawdec.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msAlawenc.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msbuffer.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/mscodec.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/mscopy.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msfdispatcher.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msfifo.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msfilter.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msilbcdec.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msilbcenc.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msMUlawdec.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msMUlawenc.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msnosync.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msossread.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msosswrite.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msqdispatcher.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msqueue.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msread.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msringplayer.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msrtprecv.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msrtpsend.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/mssoundread.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/mssoundwrite.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msspeexdec.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/msspeexenc.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/mssync.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/mstimer.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/mswrite.c \
	$$JINGLE_CPP/talk/third_party/mediastreamer/sndcard.c

contains(DEFINES, HAVE_ALSA_ASOUNDLIB_H) {
	SOURCES += $$JINGLE_CPP/talk/third_party/mediastreamer/alsacard.c
}

contains(DEFINES, HAVE_PORTAUDIO) {
	SOURCES += $$JINGLE_CPP/talk/third_party/mediastreamer/portaudiocard.c
}

#$$JINGLE_CPP/talk/third_party/mediastreamer/osscard.c \
#$$JINGLE_CPP/talk/third_party/mediastreamer/jackcard.c \
#$$JINGLE_CPP/talk/third_party/mediastreamer/hpuxsndcard.c \
	
