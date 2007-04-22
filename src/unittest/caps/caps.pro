CONFIG += qtestlib
CONFIG -= app_bundle
QT += xml

# The actual sources
SOURCES += \
	testcaps.cpp \
	stringprep_stubs.c

# Dependent source code
IRIS_PATH = ../../../iris
PSI_PATH = ../..
INCLUDEPATH += $$IRIS_PATH/include $$PSI_PATH $$IRIS_PATH/libidn

HEADERS += \
	$$IRIS_PATH/include/xmpp_features.h \
	$$IRIS_PATH/include/xmpp_agentitem.h \
	$$IRIS_PATH/include/xmpp_discoitem.h \
	$$IRIS_PATH/include/xmpp_jid.h \
	$$PSI_PATH/capsspec.h \
	$$PSI_PATH/capsregistry.h \
	$$PSI_PATH/capsmanager.h 

SOURCES += \
	$$IRIS_PATH/xmpp-im/xmpp_features.cpp \
	$$IRIS_PATH/xmpp-im/xmpp_discoitem.cpp \
	$$IRIS_PATH/xmpp-core/jid.cpp \
	$$PSI_PATH/capsspec.cpp \
	$$PSI_PATH/capsregistry.cpp \
	$$PSI_PATH/capsmanager.cpp
