INCLUDEPATH *= $$PWD/..
DEPENDPATH *= $$PWD $$PWD/..

HEADERS += \
	$$PWD/CocoaInitializer.h \
	$$PWD/cocoacommon.h \
	$$PWD/CocoaTrayClick.h

OBJECTIVE_SOURCES += \
	$$PWD/CocoaInitializer.mm \
        $$PWD/cocoacommon.mm

SOURCES += \
        $$PWD/CocoaTrayClick.cpp
