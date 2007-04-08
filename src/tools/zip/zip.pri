HEADERS += \
	$$PWD/zip.h

SOURCES += \
	$$PWD/zip.cpp \
	$$PWD/minizip/unzip.c

INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

win32: {
	INCLUDEPATH += $$PWD/minizip/win32
	DEPENDPATH  += $$PWD/minizip/win32
	LIBS += $$PWD/minizip/win32/libz.a
}
mac: {
	QMAKE_LFLAGS += -lz
}
