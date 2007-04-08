TEMPLATE = lib
CONFIG += staticlib
CONFIG += debug
TARGET = jingle_psi

target.extra = true

include(../../../conf.pri)

JINGLE_CPP = .
INCLUDEPATH += $$JINGLE_CPP 
DEFINES += POSIX
OBJECTS_DIR = $$JINGLE_CPP/.obj

# Setup
mac {
	DEFINES += OSX
}

# Base
SOURCES += \
	$$JINGLE_CPP/talk/base/asynchttprequest.cc \
	$$JINGLE_CPP/talk/base/asyncpacketsocket.cc \
	$$JINGLE_CPP/talk/base/asynctcpsocket.cc \
	$$JINGLE_CPP/talk/base/asyncudpsocket.cc \
	$$JINGLE_CPP/talk/base/base64.cc \
	$$JINGLE_CPP/talk/base/bytebuffer.cc \
	$$JINGLE_CPP/talk/base/common.cc \
	$$JINGLE_CPP/talk/base/diskcache.cc \
	$$JINGLE_CPP/talk/base/fileutils.cc \
	$$JINGLE_CPP/talk/base/firewallsocketserver.cc \
	$$JINGLE_CPP/talk/base/helpers.cc \
	$$JINGLE_CPP/talk/base/host.cc \
	$$JINGLE_CPP/talk/base/httpbase.cc \
	$$JINGLE_CPP/talk/base/httpclient.cc \
	$$JINGLE_CPP/talk/base/httpcommon.cc \
	$$JINGLE_CPP/talk/base/httpserver.cc \
	$$JINGLE_CPP/talk/base/logging.cc \
	$$JINGLE_CPP/talk/base/md5c.c \
	$$JINGLE_CPP/talk/base/messagequeue.cc \
	$$JINGLE_CPP/talk/base/natsocketfactory.cc \
	$$JINGLE_CPP/talk/base/nattypes.cc \
	$$JINGLE_CPP/talk/base/network.cc \
	$$JINGLE_CPP/talk/base/pathutils.cc \
	$$JINGLE_CPP/talk/base/physicalsocketserver.cc \
	$$JINGLE_CPP/talk/base/proxydetect.cc \
	$$JINGLE_CPP/talk/base/proxyinfo.cc \
	$$JINGLE_CPP/talk/base/signalthread.cc \
	$$JINGLE_CPP/talk/base/socketadapters.cc \
	$$JINGLE_CPP/talk/base/socketaddress.cc \
	$$JINGLE_CPP/talk/base/socketpool.cc \
	$$JINGLE_CPP/talk/base/ssladapter.cc \
	$$JINGLE_CPP/talk/base/stream.cc \
	$$JINGLE_CPP/talk/base/streamutils.cc \
	$$JINGLE_CPP/talk/base/stringdigest.cc \
	$$JINGLE_CPP/talk/base/stringencode.cc \
	$$JINGLE_CPP/talk/base/stringutils.cc \
	$$JINGLE_CPP/talk/base/tarstream.cc \
	$$JINGLE_CPP/talk/base/task.cc \
	$$JINGLE_CPP/talk/base/taskrunner.cc \
	$$JINGLE_CPP/talk/base/thread.cc \
	$$JINGLE_CPP/talk/base/time.cc \
	$$JINGLE_CPP/talk/base/urlencode.cc

unix {
	SOURCES += \
		$$JINGLE_CPP/talk/base/diskcachestd.cc \
		$$JINGLE_CPP/talk/base/openssladapter.cc \
		$$JINGLE_CPP/talk/base/unixfilesystem.cc 
}
win32 {
	SOURCES += \
		$$JINGLE_CPP/talk/base/diskcache_win32.cc \
		$$JINGLE_CPP/talk/base/schanneladapter.cc \
		$$JINGLE_CPP/talk/base/win32filesystem.cc
}

# Not needed ?
#$$JINGLE_CPP/talk/base/socketaddresspair.cc \
#$$JINGLE_CPP/talk/base/host.cc \

# P2P Base
SOURCES += \
	$$JINGLE_CPP/talk/p2p/base/constants.cc \
	$$JINGLE_CPP/talk/p2p/base/p2ptransport.cc \
	$$JINGLE_CPP/talk/p2p/base/p2ptransportchannel.cc \
	$$JINGLE_CPP/talk/p2p/base/port.cc \
	$$JINGLE_CPP/talk/p2p/base/pseudotcp.cc \
	$$JINGLE_CPP/talk/p2p/base/rawtransport.cc \
	$$JINGLE_CPP/talk/p2p/base/rawtransportchannel.cc \
	$$JINGLE_CPP/talk/p2p/base/relayport.cc \
	$$JINGLE_CPP/talk/p2p/base/session.cc \
	$$JINGLE_CPP/talk/p2p/base/sessionmanager.cc \
	$$JINGLE_CPP/talk/p2p/base/stun.cc \
	$$JINGLE_CPP/talk/p2p/base/stunport.cc \
	$$JINGLE_CPP/talk/p2p/base/stunrequest.cc \
	$$JINGLE_CPP/talk/p2p/base/tcpport.cc \
	$$JINGLE_CPP/talk/p2p/base/transport.cc \
	$$JINGLE_CPP/talk/p2p/base/transportchannel.cc \
	$$JINGLE_CPP/talk/p2p/base/transportchannelproxy.cc \
	$$JINGLE_CPP/talk/p2p/base/udpport.cc
	
# P2P Client
SOURCES += \
	$$JINGLE_CPP/talk/p2p/client/basicportallocator.cc \
	$$JINGLE_CPP/talk/p2p/client/httpportallocator.cc \
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

################################################################################
# File transfer support
################################################################################

google_ft {
	SOURCES += \
		$$JINGLE_CPP/talk/session/fileshare/fileshare.cc
		
	SOURCES += \
		$$JINGLE_CPP/talk/session/tunnel/pseudotcpchannel.cc \
		$$JINGLE_CPP/talk/session/tunnel/tunnelsessionclient.cc 
}

################################################################################
# Voice support
################################################################################

google_voice {
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
	INCLUDEPATH += $$JINGLE_CPP/talk/third_party/mediastreamer

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
}
	
