libcricketp2pbase_la_SOURCES = stun.cc \
			       port.cc \
			       udpport.cc \
			       tcpport.cc \
			       helpers.cc \
			       sessionmanager.cc \
			       session.cc \
			       p2psocket.cc \
                               pseudotcp.cc \
			       relayport.cc \
			       stunrequest.cc \
			       stunport.cc \
			       socketmanager.cc

noinst_HEADERS =               candidate.h \
                               portallocator.h \
			       relayport.h \
		     	       session.h \
		       	       sessionmessage.h \
			       stunport.h \
			       tcpport.h \
                               helpers.h \
			       port.h \
		       	       sessionid.h \
			       socketmanager.h \
			       stunrequest.h \
			       udpport.h \
                               pseudotcp.h \
                               p2psocket.h \
			       sessiondescription.h \
			       sessionmanager.h \
			       stun.h  \
			       relayserver.h \
			       stunserver.h \
			       sessionresponsemessage.h \
			       sessionmessagefactory.h

AM_CPPFLAGS = -DPOSIX

bin_PROGRAMS = relayserver stunserver
relayserver_SOURCES = relayserver.cc relayserver_main.cc
relayserver_LDADD = ../../base/libcricketbase.la libcricketp2pbase.la -lpthread
stunserver_SOURCES = stunserver.cc stunserver_main.cc
stunserver_LDADD = ../../base/libcricketbase.la libcricketp2pbase.la -lpthread

noinst_LTLIBRARIES = libcricketp2pbase.la
