# libidn
LIBIDN_BASE = $$PWD/libidn
CONFIG += libidn
include($$PWD/libidn.pri)

# IrisNet
irisnet {
	include($$PWD/irisnet/irisnet.pri)
}

DEFINES += XMPP_TEST

INCLUDEPATH += $$PWD/include $$PWD/xmpp-core $$PWD/xmpp-im $$PWD/jabber
DEPENDPATH  += $$PWD/include $$PWD/xmpp-core $$PWD/xmpp-im $$PWD/jabber

HEADERS += \
	$$PWD/xmpp-core/securestream.h \
	$$PWD/xmpp-core/parser.h \
	$$PWD/xmpp-core/xmlprotocol.h \
	$$PWD/xmpp-core/protocol.h \
	$$PWD/xmpp-core/compressionhandler.h \
	$$PWD/xmpp-core/compress.h \
	$$PWD/xmpp-core/td.h \
	$$PWD/xmpp-im/xmpp_tasks.h \
	$$PWD/xmpp-im/xmpp_xmlcommon.h \
	$$PWD/xmpp-im/xmpp_vcard.h \
	$$PWD/jabber/s5b.h \
	$$PWD/jabber/xmpp_ibb.h \
	$$PWD/jabber/filetransfer.h \
	$$PWD/include/xmpp.h \
	$$PWD/include/xmpp_jid.h \
	$$PWD/include/xmpp_muc.h \
	$$PWD/include/xmpp_rosterx.h \
	$$PWD/include/xmpp_xdata.h \
	$$PWD/include/xmpp_task.h \
	$$PWD/include/xmpp_status.h \
	$$PWD/include/xmpp_features.h \
	$$PWD/include/im.h

SOURCES += \
	$$PWD/xmpp-core/connector.cpp \
	$$PWD/xmpp-core/tlshandler.cpp \
	$$PWD/xmpp-core/jid.cpp \
	$$PWD/xmpp-core/securestream.cpp \
	$$PWD/xmpp-core/parser.cpp \
	$$PWD/xmpp-core/xmlprotocol.cpp \
	$$PWD/xmpp-core/protocol.cpp \
	$$PWD/xmpp-core/compress.cpp \
	$$PWD/xmpp-core/compressionhandler.cpp \
	$$PWD/xmpp-core/stream.cpp \
	$$PWD/xmpp-core/simplesasl.cpp \
	$$PWD/xmpp-im/features.cpp \
	$$PWD/xmpp-im/types.cpp \
	$$PWD/xmpp-im/client.cpp \
	$$PWD/xmpp-im/xmpp_xdata.cpp \
	$$PWD/xmpp-im/xmpp_tasks.cpp \
	$$PWD/xmpp-im/xmpp_xmlcommon.cpp \
	$$PWD/xmpp-im/xmpp_vcard.cpp \
	$$PWD/jabber/s5b.cpp \
	$$PWD/jabber/xmpp_ibb.cpp \
	$$PWD/jabber/filetransfer.cpp

