include(../modules.pri)
include($$IRIS_XMPP_QA_UNITTEST_MODULE)
include(unittest.pri)

# FIXME: Horrible set of dependencies, due to common being a pile of
# unrelated stuff
include($$IRIS_XMPP_JID_MODULE)
include($$PSI_TOOLS_OPTIONSTREE_MODULE)
include($$PSI_TOOLS_ATOMICXMLFILE_MODULE)
QT += gui xml
INCLUDEPATH += .. ../../iris/src/xmpp/xmpp-im
DEPENDPATH += ..
SOURCES += \
	$$PWD/../common.cpp	
