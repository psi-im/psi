#
# Psi qmake profile
#

# Configuration
TEMPLATE = app
TARGET   = psi
CONFIG  += qt thread x11
DESTDIR  = $$PWD/..

#CONFIG += use_crash
CONFIG += pep
#CONFIG += psi_plugins
DEFINES += QT_STATICPLUGIN

# Import several very useful Makefile targets 
# as well as set up default directories for 
# generated files
include(../qa/valgrind/valgrind.pri)
include(../qa/oldtest/unittest.pri)

# qconf

include(../conf.pri)

unix {
	# Target
	target.path = $$BINDIR
	INSTALLS += target

	# Shared files
	sharedfiles.path  = $$PSI_DATADIR
	sharedfiles.files = ../README ../COPYING ../iconsets ../sound ../certs
	INSTALLS += sharedfiles

	# Widgets
	#widgets.path = $$PSI_DATADIR/designer
	#widgets.files = ../libpsi/psiwidgets/libpsiwidgets.so
	#INSTALLS += widgets

	# icons and desktop files
	dt.path=$$PREFIX/share/applications/
	dt.files = ../psi.desktop 
	icon1.path=$$PREFIX/share/icons/hicolor/16x16/apps
	icon1.extra = cp -f ../iconsets/system/default/logo_16.png $(INSTALL_ROOT)$$icon1.path/psi.png
	icon2.path=$$PREFIX/share/icons/hicolor/32x32/apps
	icon2.extra = cp -f ../iconsets/system/default/logo_32.png $(INSTALL_ROOT)$$icon2.path/psi.png
	icon3.path=$$PREFIX/share/icons/hicolor/48x48/apps
	icon3.extra = cp -f ../iconsets/system/default/logo_48.png $(INSTALL_ROOT)$$icon3.path/psi.png
	icon4.path=$$PREFIX/share/icons/hicolor/64x64/apps
	icon4.extra = cp -f ../iconsets/system/default/logo_64.png $(INSTALL_ROOT)$$icon4.path/psi.png
	icon5.path=$$PREFIX/share/icons/hicolor/128x128/apps
	icon5.extra = cp -f ../iconsets/system/default/logo_128.png $(INSTALL_ROOT)$$icon5.path/psi.png
	INSTALLS += dt icon1 icon2 icon3 icon4 icon5
}

windows {
	LIBS += -lWSock32 -lUser32 -lShell32 -lGdi32 -lAdvAPI32
	DEFINES += QT_STATICPLUGIN
	DEFINES += NOMINMAX # suppress min/max #defines in windows headers
	INCLUDEPATH += . # otherwise MSVC will fail to find "common.h" when compiling options/* stuff
	#QTPLUGIN += qjpeg qgif
}

# Psi sources
include(src.pri)

# don't clash with unittests
SOURCES += main.cpp
HEADERS += main.h

################################################################################
# Translation
################################################################################

LANG_PATH = ../lang

TRANSLATIONS = \
	$$LANG_PATH/psi_ar.ts \
	$$LANG_PATH/psi_be.ts \
	$$LANG_PATH/psi_bg.ts \
	$$LANG_PATH/psi_br.ts \
	$$LANG_PATH/psi_ca.ts \
	$$LANG_PATH/psi_cs.ts \
	$$LANG_PATH/psi_da.ts \
	$$LANG_PATH/psi_de.ts \
	$$LANG_PATH/psi_ee.ts \
	$$LANG_PATH/psi_el.ts \
	$$LANG_PATH/psi_eo.ts \
	$$LANG_PATH/psi_es.ts \
	$$LANG_PATH/psi_et.ts \
	$$LANG_PATH/psi_fi.ts \
	$$LANG_PATH/psi_fr.ts \
	$$LANG_PATH/psi_hr.ts \
	$$LANG_PATH/psi_hu.ts \
	$$LANG_PATH/psi_it.ts \
	$$LANG_PATH/psi_ja.ts \
	$$LANG_PATH/psi_mk.ts \
	$$LANG_PATH/psi_nl.ts \
	$$LANG_PATH/psi_pl.ts \
	$$LANG_PATH/psi_pt.ts \
	$$LANG_PATH/psi_pt_BR.ts \
	$$LANG_PATH/psi_ru.ts \
	$$LANG_PATH/psi_se.ts \
	$$LANG_PATH/psi_sk.ts \
	$$LANG_PATH/psi_sl.ts \
	$$LANG_PATH/psi_sr.ts \
	$$LANG_PATH/psi_sr@latin.ts \
	$$LANG_PATH/psi_sv.ts \
	$$LANG_PATH/psi_sw.ts \
	$$LANG_PATH/psi_uk.ts \
	$$LANG_PATH/psi_ur_PK.ts \
	$$LANG_PATH/psi_vi.ts \
	$$LANG_PATH/psi_zh_CN.ts \
	$$LANG_PATH/psi_zh_TW.ts

OPTIONS_TRANSLATIONS_FILE=$$PWD/option_translations.cpp

QMAKE_EXTRA_TARGETS += translate_options
translate_options.commands = $$PWD/../admin/update_options_ts.py $$PWD/../options/default.xml > $$OPTIONS_TRANSLATIONS_FILE

# In case lupdate doesn't work
QMAKE_EXTRA_TARGETS += translate
translate.commands = lupdate . options widgets tools/grepshortcutkeydlg ../cutestuff/network ../iris/xmpp-im -ts $$TRANSLATIONS


exists($$OPTIONS_TRANSLATIONS_FILE) {
	SOURCES += $$OPTIONS_TRANSLATIONS_FILE
}
QMAKE_CLEAN += $$OPTIONS_TRANSLATIONS_FILE

################################################################################

# Resources
RESOURCES += ../psi.qrc ../iconsets.qrc

# Platform specifics
windows {
	contains(QMAKE_HOST.arch, x86_64):{
		RC_FILE = ../win32/psi_win64.rc
	} else {
		RC_FILE = ../win32/psi_win32.rc
	}

	# buggy MSVC workaround
	win32-msvc|win32-msvc.net|win32-msvc2005: QMAKE_LFLAGS += /FORCE:MULTIPLE
}
mac {
	# Universal binaries
	qc_universal:contains(QT_CONFIG,x86):contains(QT_CONFIG,x86_64) {
		CONFIG += x86 x86_64
		QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.5.sdk
		QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5
	}

	# Frameworks are specified in src.pri

	QMAKE_INFO_PLIST = ../mac/Info.plist
	RC_FILE = ../mac/application.icns
	QMAKE_POST_LINK = cp -R ../certs ../iconsets ../sound `dirname $(TARGET)`/../Resources ; echo "APPLpsi " > `dirname $(TARGET)`/../PkgInfo
}
