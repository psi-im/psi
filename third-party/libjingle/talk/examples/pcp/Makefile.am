bin_PROGRAMS = pcp
pcp_CXXFLAGS = $(AM_CXXFLAGS)
pcp_SOURCES = pcp_main.cc
pcp_LDADD = $(top_srcdir)/talk/examples/login/libcricketexampleslogin.la \
             $(top_srcdir)/talk/session/tunnel/libcricketsessiontunnel.la \
             $(top_srcdir)/talk/p2p/client/libcricketp2pclient.la \
             $(top_srcdir)/talk/p2p/base/libcricketp2pbase.la \
             $(top_srcdir)/talk/xmpp/libcricketxmpp.la \
             $(top_srcdir)/talk/xmllite/libcricketxmllite.la \
             $(top_srcdir)/talk/base/libcricketbase.la \
             $(EXPAT_LIBS) -lpthread -lssl
AM_CPPFLAGS = -DPOSIX
EXTRA_DIST = pcp.vcproj
