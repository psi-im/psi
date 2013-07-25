INCLUDEPATH *= $$PWD/..
DEPENDPATH *= $$PWD/..

HEADERS += \
	$$PWD/AutoUpdater.h

SOURCES += \
	$$PWD/AutoUpdater.cpp

Sparkle {
	HEADERS += \
		$$PWD/SparkleAutoUpdater.h

	OBJECTIVE_SOURCES += \
		$$PWD/SparkleAutoUpdater.mm

	LIBS += -framework Sparkle
}
