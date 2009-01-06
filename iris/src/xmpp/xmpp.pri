IRIS_BASE = $PWD/../..
include(../libbase.pri)
include($$PWD/modules.pri)

QT *= xml network

INCLUDEPATH += $$PWD/../irisnet/corelib $$PWD/../irisnet/noncore $$PWD/../irisnet/noncore/legacy
iris_bundle:{
	include(../irisnet/noncore/noncore.pri)
}
else {
	LIBS += -L$$IRIS_BASE/lib -lirisnet
}

include($$PWD/cutestuff/cutestuff.pri)
include($$IRIS_XMPP_BASE_MODULE)
include($$IRIS_XMPP_BASE64_MODULE)
include($$IRIS_XMPP_ZLIB_MODULE)
include($$IRIS_XMPP_JID_MODULE)
include($$IRIS_XMPP_SASL_MODULE)

DEFINES += XMPP_TEST

INCLUDEPATH += \
	$$PWD/ \
	$$PWD/.. \
  $$PWD/xmpp-core \
  $$PWD/xmpp-im

HEADERS += \
	$$PWD/xmpp-core/securestream.h \
	$$PWD/xmpp-core/parser.h \
	$$PWD/xmpp-core/xmlprotocol.h \
	$$PWD/xmpp-core/protocol.h \
	$$PWD/xmpp-core/compressionhandler.h \
	$$PWD/xmpp-core/td.h \
	$$PWD/xmpp-im/xmpp_tasks.h \
	$$PWD/xmpp-im/xmpp_discoinfotask.h \
	$$PWD/xmpp-im/xmpp_xmlcommon.h \
	$$PWD/xmpp-im/xmpp_vcard.h \
	$$PWD/xmpp-im/s5b.h \
	$$PWD/xmpp-im/xmpp_ibb.h \
	$$PWD/xmpp-im/filetransfer.h \
	$$PWD/xmpp-core/xmpp.h \
	$$PWD/xmpp-im/xmpp_url.h \
	$$PWD/xmpp-im/xmpp_chatstate.h \
	$$PWD/xmpp-im/xmpp_receipts.h \
	$$PWD/xmpp-im/xmpp_client.h \
	$$PWD/xmpp-core/xmpp_clientstream.h \
	$$PWD/xmpp-core/xmpp_stanza.h \
	$$PWD/xmpp-core/xmpp_stream.h \
	$$PWD/xmpp-im/xmpp_address.h \
	$$PWD/xmpp-im/xmpp_htmlelement.h \
	$$PWD/xmpp-im/xmpp_muc.h \
	$$PWD/xmpp-im/xmpp_message.h \
	$$PWD/xmpp-im/xmpp_pubsubitem.h \
	$$PWD/xmpp-im/xmpp_resource.h \
	$$PWD/xmpp-im/xmpp_roster.h \
	$$PWD/xmpp-im/xmpp_rosterx.h \
	$$PWD/xmpp-im/xmpp_xdata.h \
	$$PWD/xmpp-im/xmpp_rosteritem.h \
	$$PWD/xmpp-im/xmpp_liveroster.h \
	$$PWD/xmpp-im/xmpp_liverosteritem.h \
	$$PWD/xmpp-im/xmpp_resourcelist.h \
	$$PWD/xmpp-im/xmpp_task.h \
	$$PWD/xmpp-im/xmpp_httpauthrequest.h \
	$$PWD/xmpp-im/xmpp_status.h \
	$$PWD/xmpp-im/xmpp_features.h \
	$$PWD/xmpp-im/xmpp_agentitem.h \
	$$PWD/xmpp-im/xmpp_discoitem.h \
	$$PWD/xmpp-im/im.h

SOURCES += \
	$$PWD/xmpp-core/connector.cpp \
	$$PWD/xmpp-core/tlshandler.cpp \
	$$PWD/xmpp-core/securestream.cpp \
	$$PWD/xmpp-core/parser.cpp \
	$$PWD/xmpp-core/xmlprotocol.cpp \
	$$PWD/xmpp-core/protocol.cpp \
	$$PWD/xmpp-core/compressionhandler.cpp \
	$$PWD/xmpp-core/stream.cpp \
	$$PWD/xmpp-core/simplesasl.cpp \
	$$PWD/xmpp-core/xmpp_stanza.cpp \
	$$PWD/xmpp-im/types.cpp \
	$$PWD/xmpp-im/client.cpp \
	$$PWD/xmpp-im/xmpp_features.cpp \
	$$PWD/xmpp-im/xmpp_discoitem.cpp \
	$$PWD/xmpp-im/xmpp_discoinfotask.cpp \
	$$PWD/xmpp-im/xmpp_xdata.cpp \
	$$PWD/xmpp-im/xmpp_task.cpp \
	$$PWD/xmpp-im/xmpp_tasks.cpp \
	$$PWD/xmpp-im/xmpp_xmlcommon.cpp \
	$$PWD/xmpp-im/xmpp_vcard.cpp \
	$$PWD/xmpp-im/s5b.cpp \
	$$PWD/xmpp-im/xmpp_ibb.cpp \
	$$PWD/xmpp-im/filetransfer.cpp
