INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

HEADERS += $$PWD/spellchecker.h $$PWD/spellhighlighter.h
SOURCES += $$PWD/spellchecker.cpp $$PWD/spellhighlighter.cpp

mac {
	HEADERS += $$PWD/macspellchecker.h
	OBJECTIVE_SOURCES += $$PWD/macspellchecker.mm
}

!mac:contains(DEFINES, HAVE_ASPELL) {
	HEADERS += $$PWD/aspellchecker.h
	SOURCES += $$PWD/aspellchecker.cpp
}

!mac:contains(DEFINES, HAVE_ENCHANT) {
	HEADERS += $$PWD/enchantchecker.h
	SOURCES += $$PWD/enchantchecker.cpp
}
