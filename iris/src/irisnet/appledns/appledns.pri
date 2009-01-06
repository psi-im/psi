QT *= network

HEADERS += $$PWD/qdnssd.h
SOURCES += $$PWD/qdnssd.cpp $$PWD/appledns.cpp

!mac:LIBS += -ldns_sd
