#
# Psi qmake profile
#

# Configuration
TEMPLATE = app
TARGET   = psi
CONFIG  += qt thread x11
DESTDIR  = $$top_builddir

#CONFIG += use_crash
CONFIG += pep
#CONFIG += psi_plugins
DEFINES += QT_STATICPLUGIN

greaterThan(QT_MAJOR_VERSION, 4) {
	CONFIG += c++11
} else {
	QMAKE_CXXFLAGS += -std=c++11
}

win32:CONFIG(debug, debug|release):DEFINES += ALLOW_QT_PLUGINS_DIR

# Import several very useful Makefile targets
# as well as set up default directories for
# generated files
include(../qa/valgrind/valgrind.pri)
include(../qa/oldtest/unittest.pri)

# qconf

include($$top_builddir/conf.pri)

unix {
	DEFINES += APP_PREFIX=$$PREFIX
	DEFINES += APP_BIN_NAME=$$TARGET
	# Target
	target.path = $$BINDIR
	INSTALLS += target

	# Shared files
	sharedfiles.path  = $$PSI_DATADIR
	sharedfiles.files = ../README ../COPYING ../client_icons.txt ../iconsets ../sound ../certs
	qtwebkit|qtwebengine {
		sharedfiles.files += ../themes
	}

	INSTALLS += sharedfiles

	# Widgets
	#widgets.path = $$PSI_DATADIR/designer
	#widgets.files = ../libpsi/psiwidgets/libpsiwidgets.so
	#INSTALLS += widgets

	# icons and desktop files
	dt.path=$$PREFIX/share/applications/
	dt.files = ../psi.desktop
	ad.path=$$PREFIX/share/appdata/
	ad.file = ../psi.appdata.xml
	icon1.path=$$PREFIX/share/icons/hicolor/16x16/apps
	icon1.extra = cp -f $$top_srcdir/iconsets/system/default/logo_16.png $(INSTALL_ROOT)$$icon1.path/psi.png
	icon2.path=$$PREFIX/share/icons/hicolor/32x32/apps
	icon2.extra = cp -f $$top_srcdir/iconsets/system/default/logo_32.png $(INSTALL_ROOT)$$icon2.path/psi.png
	icon3.path=$$PREFIX/share/icons/hicolor/48x48/apps
	icon3.extra = cp -f $$top_srcdir/iconsets/system/default/logo_48.png $(INSTALL_ROOT)$$icon3.path/psi.png
	icon4.path=$$PREFIX/share/icons/hicolor/64x64/apps
	icon4.extra = cp -f $$top_srcdir/iconsets/system/default/logo_64.png $(INSTALL_ROOT)$$icon4.path/psi.png
	icon5.path=$$PREFIX/share/icons/hicolor/128x128/apps
	icon5.extra = cp -f $$top_srcdir/iconsets/system/default/logo_128.png $(INSTALL_ROOT)$$icon5.path/psi.png
	INSTALLS += dt ad icon1 icon2 icon3 icon4 icon5

	psi_plugins {
		pluginsfiles.path  = $$PSI_DATADIR/plugins
		pluginsfiles.files = plugins/plugins.pri plugins/psiplugin.pri plugins/include $$top_builddir/pluginsconf.pri

		INSTALLS += pluginsfiles
	}
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

# this assumes the translations repo is checked out next to psi
LANG_PATH = ../../psi-translations

TRANSLATIONS = \
        $$LANG_PATH/psi_xx.ts \
	#$$LANG_PATH/ar/psi_ar.ts \
	$$LANG_PATH/be/psi_be.ts \
	#$$LANG_PATH/bg/psi_bg.ts \
	#$$LANG_PATH/br/psi_br.ts \
	$$LANG_PATH/ca/psi_ca.ts \
	$$LANG_PATH/cs/psi_cs.ts \
	#$$LANG_PATH/da/psi_da.ts \
	$$LANG_PATH/de/psi_de.ts \
	#$$LANG_PATH/ee/psi_ee.ts \
	#$$LANG_PATH/el/psi_el.ts \
	#$$LANG_PATH/eo/psi_eo.ts \
	$$LANG_PATH/es/psi_es.ts \
	$$LANG_PATH/es_ES/psi_es_ES.ts \
	#$$LANG_PATH/et/psi_et.ts \
	#$$LANG_PATH/fi/psi_fi.ts \
	$$LANG_PATH/fr/psi_fr.ts \
	#$$LANG_PATH/hr/psi_hr.ts \
	#$$LANG_PATH/hu/psi_hu.ts \
	$$LANG_PATH/it/psi_it.ts \
	$$LANG_PATH/ja/psi_ja.ts \
	$$LANG_PATH/mk/psi_mk.ts \
	#$$LANG_PATH/nl/psi_nl.ts \
	$$LANG_PATH/pl/psi_pl.ts \
	#$$LANG_PATH/pt/psi_pt.ts \
	$$LANG_PATH/pt_BR/psi_pt_BR.ts \
	$$LANG_PATH/ru/psi_ru.ts \
	#$$LANG_PATH/se/psi_se.ts \
	#$$LANG_PATH/sk/psi_sk.ts \
	$$LANG_PATH/sl/psi_sl.ts \
	#$$LANG_PATH/sr/psi_sr.ts \
	#$$LANG_PATH/sr@latin/psi_sr@latin.ts \
	$$LANG_PATH/sv/psi_sv.ts \
	#$$LANG_PATH/sw/psi_sw.ts \
	$$LANG_PATH/uk/psi_uk.ts \
	$$LANG_PATH/ur_PK/psi_ur_PK.ts \
	$$LANG_PATH/vi/psi_vi.ts \
	$$LANG_PATH/zh_CN/psi_zh_CN.ts \
	$$LANG_PATH/zh_TW/psi_zh_TW.ts

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
    RC_ICONS = ../win32/app.ico
	VERSION = $$PSI_VERSION
	QMAKE_TARGET_PRODUCT = "Psi"
	QMAKE_TARGET_COMPANY = psi-im.org
	QMAKE_TARGET_DESCRIPTION = "A cross-platform XMPP client designed for the power user."
	QMAKE_TARGET_COPYRIGHT = "GNU GPL v2"
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
