INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

HEADERS += $$PWD/growlnotifier.h
SOURCES += $$PWD/growlnotifier.cpp
QMAKE_LFLAGS += -framework Growl -framework CoreFoundation
