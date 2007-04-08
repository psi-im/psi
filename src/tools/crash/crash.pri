INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD
HEADERS     += $$PWD/crash.h

unix {
	SOURCES     += $$PWD/crash_sigsegv.cpp
	HEADERS     += $$PWD/crash_sigsegv.h
			
	!isEmpty(KDE) {
		SOURCES += $$PWD/crash_kde.cpp
		LIBS    += -L$$KDE/lib -lkdecore
		INCLUDEPATH += $$KDE/include
		DEPENDPATH  += $$KDE/include
	}
	else {
		SOURCES +=  $$PWD/crash.cpp 
	}
}
else {
	SOURCES += $$PWD/crash_dummy.cpp
}
