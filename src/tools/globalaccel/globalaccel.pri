HEADERS += $$PWD/globalaccelman.h
INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

unix:!mac {
	SOURCES += $$PWD/globalaccelman_x11.cpp
}
win32: {
	SOURCES += $$PWD/globalaccelman_win.cpp
}
mac: {
	SOURCES += $$PWD/globalaccelman_mac.cpp
}
