INCLUDEPATH += $$PWD/util $$PWD/network
DEPENDPATH += $$PWD/util $$PWD/network

HEADERS += \
	$$PWD/util/bytestream.h \
	$$PWD/network/bsocket.h \
	$$PWD/network/httpconnect.h \
	$$PWD/network/httppoll.h \
	$$PWD/network/socks.h

SOURCES += \
	$$PWD/util/bytestream.cpp \
	$$PWD/network/bsocket.cpp \
	$$PWD/network/httpconnect.cpp \
	$$PWD/network/httppoll.cpp \
	$$PWD/network/socks.cpp
	
!irisnet {
	INCLUDEPATH += $$PWD/legacy
	HEADERS += \ 
		$$PWD/legacy/safedelete.h \
		$$PWD/legacy/ndns.h \
		$$PWD/legacy/srvresolver.h \
		$$PWD/legacy/servsock.h \
		
	SOURCES += \
		$$PWD/legacy/safedelete.cpp \
		$$PWD/legacy/ndns.cpp \
		$$PWD/legacy/srvresolver.cpp \
		$$PWD/legacy/servsock.cpp \

}