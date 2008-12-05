OPTIONSTREE_CPP = ..
CONFIG += optionstree
include(../optionstree.pri)
include(../../atomicxmlfile/atomicxmlfile.pri)
include(../../../../iris/src/xmpp/base64/base64.pri)

SOURCES += \
	$$PWD/OptionsTreeMainTest.cpp
