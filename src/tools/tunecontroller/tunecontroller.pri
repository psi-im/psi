INCLUDEPATH += $$PWD 
DEPENDPATH += $$PWD

HEADERS += \
	$$PWD/tune.h \
	$$PWD/tunecontroller.h \
	$$PWD/pollingtunecontroller.h \
	$$PWD/filetunecontroller.h \
	$$PWD/combinedtunecontroller.h \
	$$PWD/tunecontrollerplugin.h \
	$$PWD/tunecontrollermanager.h \
	$$PWD/tunecontrollerinterface.h 



SOURCES += \
	$$PWD/pollingtunecontroller.cpp \
	$$PWD/filetunecontroller.cpp \
	$$PWD/combinedtunecontroller.cpp \
	$$PWD/tunecontrollermanager.cpp

# XMMS
tc_xmms {
	DEFINES += TC_XMMS
	XMMS_PLUGIN_PATH = $$PWD/plugins/xmms
	INCLUDEPATH += $$XMMS_PLUGIN_PATH

	HEADERS += \
		$$XMMS_PLUGIN_PATH/xmmscontroller.h 
		
	SOURCES += \
		$$XMMS_PLUGIN_PATH/xmmscontroller.cpp \
		$$XMMS_PLUGIN_PATH/xmmsplugin.cpp
}

# iTunes
tc_itunes {
	mac {
		DEFINES += TC_ITUNES
		ITUNES_PLUGIN_PATH = $$PWD/plugins/itunes
		INCLUDEPATH += $$ITUNES_PLUGIN_PATH

		HEADERS += \
			$$ITUNES_PLUGIN_PATH/itunescontroller.h 
			
		SOURCES += \
			$$ITUNES_PLUGIN_PATH/itunescontroller.cpp \
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
		$$WINAMP_PLUGIN_PATH/winampcontroller.h \
		$$WINAMP_PLUGIN_PATH/winampplugin.h 
		
	SOURCES += \
		$$WINAMP_PLUGIN_PATH/winampcontroller.cpp \
		$$WINAMP_PLUGIN_PATH/winampplugin.cpp
	
	LIBS += -lUser32
}

# Psi File
tc_psifile {
	DEFINES += TC_PSIFILE
	PSIFILE_PLUGIN_PATH = $$PWD/plugins/psifile
	INCLUDEPATH += $$PSIFILE_PLUGIN_PATH

	HEADERS += \
		$$PSIFILE_PLUGIN_PATH/psifilecontroller.h \
		$$PSIFILE_PLUGIN_PATH/psifileplugin.h
		
	SOURCES += \
		$$PSIFILE_PLUGIN_PATH/psifilecontroller.cpp \
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
