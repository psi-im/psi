HEADERS += $$PWD/trayicon.h
SOURCES += $$PWD/trayicon.cpp
INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

unix:!mac {
	SOURCES += $$PWD/trayicon_x11.cpp
}
win32: {
	SOURCES += $$PWD/trayicon_win.cpp
}
mac: {
	SOURCES += $$PWD/trayicon_mac.cpp
}
