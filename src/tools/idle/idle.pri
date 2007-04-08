HEADERS += $$PWD/idle.h
SOURCES += $$PWD/idle.cpp
INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

unix:!mac {
	SOURCES += $$PWD/idle_x11.cpp
}
win32: {
	SOURCES += $$PWD/idle_win.cpp
}
mac: {
	SOURCES += $$PWD/idle_mac.cpp
}
