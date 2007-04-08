INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

HEADERS += $$PWD/spellchecker.h $$PWD/spellhighlighter.h
SOURCES += $$PWD/spellchecker.cpp $$PWD/spellhighlighter.cpp

contains(DEFINES, HAVE_ASPELL) {
	HEADERS += $$PWD/aspellchecker.h
	SOURCES += $$PWD/aspellchecker.cpp
}
