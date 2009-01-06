INCLUDEPATH *= $$PWD/../..
DEPENDPATH *= $$PWD/../..

HEADERS += \
	$$PWD/jid.h

SOURCES += \
	$$PWD/jid.cpp

# Normally, we shouldn't include other modules, but since Jid is the only
# user of libidn, and many modules include this module, we include this
# module here
include($$PWD/../../libidn/libidn.pri)
