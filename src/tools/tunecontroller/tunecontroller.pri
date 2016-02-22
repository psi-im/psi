INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

HEADERS += \
	$$PWD/tune.h \
	$$PWD/tunecontroller.h \
	$$PWD/pollingtunecontroller.h \
	$$PWD/filetunecontroller.h \
	$$PWD/tunecontrollerplugin.h \
	$$PWD/tunecontrollermanager.h \
	$$PWD/tunecontrollerinterface.h



SOURCES += \
	$$PWD/pollingtunecontroller.cpp \
	$$PWD/filetunecontroller.cpp \
	$$PWD/tunecontrollermanager.cpp

# iTunes
tc_itunes {
	mac {
		DEFINES += TC_ITUNES
		ITUNES_PLUGIN_PATH = $$PWD/plugins/itunes
		INCLUDEPATH += $$ITUNES_PLUGIN_PATH

		HEADERS += \
			$$PWD/itunestunecontroller.h

		SOURCES += \
			$$PWD/itunestunecontroller.cpp \
			$$ITUNES_PLUGIN_PATH/itunesplugin.cpp

		QMAKE_LFLAGS += -framework CoreFoundation
	}
}

# WinAmp
tc_winamp {
	DEFINES += TC_WINAMP
	WINAMP_PLUGIN_PATH = $$PWD/plugins/winamp
	INCLUDEPATH += $$WINAMP_PLUGIN_PATH

	HEADERS += \
		$$PWD/winamptunecontroller.h \
		$$WINAMP_PLUGIN_PATH/winampplugin.h

	SOURCES += \
		$$PWD/winamptunecontroller.cpp \
		$$WINAMP_PLUGIN_PATH/winampplugin.cpp

	LIBS += -lUser32
}

# Psi File
tc_psifile {
	DEFINES += TC_PSIFILE
	PSIFILE_PLUGIN_PATH = $$PWD/plugins/psifile
	INCLUDEPATH += $$PSIFILE_PLUGIN_PATH

	HEADERS += \
		$$PSIFILE_PLUGIN_PATH/psifileplugin.h

	SOURCES += \
		$$PSIFILE_PLUGIN_PATH/psifileplugin.cpp
}

#MPRIS
tc_mpris {
	DEFINES += TC_MPRIS
	MPRIS_PLUGIN_PATH = $$PWD/plugins/mpris
	INCLUDEPATH += $$MPRIS_PLUGIN_PATH

	HEADERS += \
		$$PWD/mpristunecontroller.h

	SOURCES += \
		$$PWD/mpristunecontroller.cpp \
		$$MPRIS_PLUGIN_PATH/mprisplugin.cpp
}

#AIMP
tc_aimp {
	DEFINES += TC_AIMP
	AIMP_PLUGIN_PATH = $$PWD/plugins/aimp
	INCLUDEPATH += $$AIMP_PLUGIN_PATH

	HEADERS += \
		$$PWD/aimptunecontroller.h

	SOURCES += \
		$$PWD/aimptunecontroller.cpp \
		$$AIMP_PLUGIN_PATH/aimpplugin.cpp
}
