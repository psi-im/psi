noinst_LTLIBRARIES= libcricketexampleslogin.la
libcricketexampleslogin_la_SOURCES = xmppsocket.cc \
				     xmppauth.cc \
				     xmppthread.cc \
				     xmpppump.cc 
noinst_HEADERS = xmppauth.h  xmpppump.h  xmppsocket.h  xmppthread.h
bin_PROGRAMS = login
login_CXXFLAGS = $(AM_CXXFLAGS)
login_SOURCES = login_main.cc xmppsocket.cc xmppthread.cc xmpppump.cc xmppauth.cc
login_LDADD =  $(top_srcdir)/talk/xmpp/libcricketxmpp.la \
	       $(top_srcdir)/talk/xmllite/libcricketxmllite.la \
	       $(top_srcdir)/talk/base/libcricketbase.la \
	       $(EXPAT_LIBS) -lpthread -lssl
AM_CPPFLAGS  = -DPOSIX
