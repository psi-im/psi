include(../../modules.pri)
include($$IRIS_XMPP_JID_MODULE)
include($$IRIS_XMPP_QA_UNITTEST_MODULE)
include($$PSI_CAPABILITIES_MODULE)
include($$PSI_UTILITIES_MODULE)
include($$PSI_PROTOCOL_MODULE)
include(unittest.pri)

QT += xml

# FIXME
INCLUDEPATH += \
	$$PWD/../../../iris/src/xmpp/xmpp-im 
DEPENDPATH += \
	$$PWD/../../../iris/src/xmpp/xmpp-im 

SOURCES += \
	$$PWD/../../../iris/src/xmpp/xmpp-im/xmpp_features.cpp \
	$$PWD/../../../iris/src/xmpp/xmpp-im/xmpp_discoitem.cpp
