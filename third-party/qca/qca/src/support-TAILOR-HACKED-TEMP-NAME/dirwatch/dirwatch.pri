HEADERS += $$PWD/dirwatch_p.h
SOURCES += $$PWD/dirwatch.cpp

unix:!mac {
	SOURCES += $$PWD/dirwatch_unix.cpp
}
windows: {
	SOURCES += $$PWD/dirwatch_win.cpp
}
mac: {
	SOURCES += $$PWD/dirwatch_mac.cpp
}
