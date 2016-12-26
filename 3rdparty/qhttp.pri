PRJDIR    = $$PWD/qhttp
QHTTPSRC  = $$PRJDIR/src

INCLUDEPATH *= $$PRJDIR/src $$PWD

win32 {
    DEFINES *= _WINDOWS WIN32_LEAN_AND_MEAN NOMINMAX
}

DEFINES       *= QHTTP_MEMORY_LOG=0
win32:DEFINES *= QHTTP_EXPORT
# To enable client
# DEFINES     *= QHTTP_HAS_CLIENT

# Joyent http_parser
SOURCES  += $$PWD/http-parser/http_parser.c
HEADERS  += $$PWD/http-parser/http_parser.h

SOURCES  += \
    $$QHTTPSRC/qhttpabstracts.cpp \
    $$QHTTPSRC/qhttpserverconnection.cpp \
    $$QHTTPSRC/qhttpserverrequest.cpp \
    $$QHTTPSRC/qhttpserverresponse.cpp \
    $$QHTTPSRC/qhttpserver.cpp

HEADERS  += \
    $$QHTTPSRC/qhttpfwd.hpp \
    $$QHTTPSRC/qhttpabstracts.hpp \
    $$QHTTPSRC/qhttpserverconnection.hpp \
    $$QHTTPSRC/qhttpserverrequest.hpp \
    $$QHTTPSRC/qhttpserverresponse.hpp \
    $$QHTTPSRC/qhttpserver.hpp

contains(DEFINES, QHTTP_HAS_CLIENT) {
    SOURCES += \
        $$QHTTPSRC/qhttpclientrequest.cpp \
        $$QHTTPSRC/qhttpclientresponse.cpp \
        $$QHTTPSRC/qhttpclient.cpp

    HEADERS += \
        $$QHTTPSRC/qhttpclient.hpp \
        $$QHTTPSRC/qhttpclientresponse.hpp \
        $$QHTTPSRC/qhttpclientrequest.hpp
}
