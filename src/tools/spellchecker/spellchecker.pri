INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

HEADERS += $$PWD/spellchecker.h
SOURCES += $$PWD/spellchecker.cpp

contains(DEFINES, HAVE_ASPELL) {
	HEADERS += $$PWD/aspellchecker.h
	SOURCES += $$PWD/aspellchecker.cpp
}
