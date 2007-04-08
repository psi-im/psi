TEMPLATE = app

HEADERS += growlnotifier.h
SOURCES += growlnotifier.cpp growltest.cpp

QMAKE_LFLAGS += -framework Growl -framework CoreFoundation
