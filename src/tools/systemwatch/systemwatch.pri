HEADERS += $$PWD/systemwatch.h
SOURCES += $$PWD/systemwatch.cpp
INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

unix:!mac {
	HEADERS += $$PWD/systemwatch_unix.h
	SOURCES += $$PWD/systemwatch_unix.cpp
}
win32: {
	HEADERS += $$PWD/systemwatch_win.h
	SOURCES += $$PWD/systemwatch_win.cpp
}
mac: {
	HEADERS += $$PWD/systemwatch_mac.h
	SOURCES += $$PWD/systemwatch_mac.cpp
}
