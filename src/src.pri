QT += xml network qt3support

# modules
include($$PWD/protocol/protocol.pri)
include($$PWD/irisprotocol/irisprotocol.pri)
include($$PWD/privacy/privacy.pri)
include($$PWD/capabilities/capabilities.pri)
include($$PWD/utilities/utilities.pri)
include($$PWD/tabs/tabs.pri)

# tools
include($$PWD/tools/trayicon/trayicon.pri)
include($$PWD/tools/iconset/iconset.pri)
include($$PWD/tools/idle/idle.pri)
include($$PWD/tools/systemwatch/systemwatch.pri)
include($$PWD/tools/zip/zip.pri)
include($$PWD/tools/optionstree/optionstree.pri)
include($$PWD/tools/globalshortcut/globalshortcut.pri)
include($$PWD/tools/advwidget/advwidget.pri)
include($$PWD/tools/spellchecker/spellchecker.pri)
include($$PWD/tools/contactlist/contactlist.pri)
include($$PWD/tools/grepshortcutkeydlg/grepshortcutkeydlg.pri)
include($$PWD/tools/atomicxmlfile/atomicxmlfile.pri)

# Growl
mac {
	contains(DEFINES, HAVE_GROWL) {
		include($$PWD/tools/growlnotifier/growlnotifier.pri)
	}
}

# Mac dock
mac { include($$PWD/tools/mac_dock/mac_dock.pri) }

# Tune
pep {
	DEFINES += USE_PEP
	CONFIG += tc_psifile
	mac { CONFIG += tc_itunes }
	windows { CONFIG += tc_winamp }
}
include($$PWD/tools/tunecontroller/tunecontroller.pri)

# Crash
use_crash {
	DEFINES += USE_CRASH
	include($$PWD/tools/crash/crash.pri)
}

# qca
qca-static {
	# QCA
	DEFINES += QCA_STATIC
	include($$PWD/../third-party/qca/qca.pri)

	# QCA-OpenSSL
	contains(DEFINES, HAVE_OPENSSL) {
		include($$PWD/../third-party/qca/qca-ossl.pri)
	}
	
	# QCA-SASL
	contains(DEFINES, HAVE_CYRUSSASL) {
		include($$PWD/../third-party/qca/qca-cyrus-sasl.pri)
	}

	# QCA-GnuPG
	include($$PWD/../third-party/qca/qca-gnupg.pri)
}
else {
	CONFIG += crypto	
}

# Widgets
include($$PWD/widgets/widgets.pri)

# Google FT
google_ft {
	DEFINES += GOOGLE_FT
	HEADERS += $$PWD/googleftmanager.h
	SOURCES += $$PWD/googleftmanager.cpp
	include(../third-party/libjingle.new/libjingle.pri)
}

# Jingle
jingle {
	HEADERS += $$PWD/jinglevoicecaller.h
	SOURCES += $$PWD/jinglevoicecaller.cpp
	DEFINES += HAVE_JINGLE POSIX

	JINGLE_CPP = $$PWD/../third-party/libjingle
	LIBS += -L$$JINGLE_CPP -ljingle_psi
	INCLUDEPATH += $$JINGLE_CPP

	contains(DEFINES, HAVE_PORTAUDIO) {
		LIBS += -framework CoreAudio -framework AudioToolbox
	}
}

# include Iris XMPP library
include($$PWD/../iris/iris.pri)

# Header files
HEADERS += \
	$$PWD/varlist.h \ 
	$$PWD/jidutil.h \
	$$PWD/showtextdlg.h \ 
	$$PWD/profiles.h \
	$$PWD/activeprofiles.h \
	$$PWD/profiledlg.h \
	$$PWD/aboutdlg.h \
	$$PWD/desktoputil.h \
	$$PWD/fileutil.h \
	$$PWD/textutil.h \
	$$PWD/pixmaputil.h \
	$$PWD/psiaccount.h \
	$$PWD/psicon.h \
	$$PWD/accountscombobox.h \
	$$PWD/psievent.h \
	$$PWD/xmlconsole.h \
	$$PWD/contactview.h \
	$$PWD/psiiconset.h \
	$$PWD/applicationinfo.h \
	$$PWD/pgpkeydlg.h \
	$$PWD/pgputil.h \
	$$PWD/pgptransaction.h \
	$$PWD/userlist.h \
	$$PWD/mainwin.h \
	$$PWD/mainwin_p.h \
	$$PWD/psitrayicon.h \
	$$PWD/rtparse.h \
	$$PWD/systeminfo.h \
	$$PWD/common.h \
	$$PWD/proxy.h \
	$$PWD/miniclient.h \
	$$PWD/accountmanagedlg.h \
	$$PWD/accountadddlg.h \
	$$PWD/accountregdlg.h \
	$$PWD/accountmodifydlg.h \
	$$PWD/changepwdlg.h \
	$$PWD/msgmle.h \
	$$PWD/statusdlg.h \
	$$PWD/statuscombobox.h \
	$$PWD/certutil.h \
	$$PWD/eventdlg.h \
	$$PWD/chatdlg.h \
	$$PWD/psichatdlg.h \
	$$PWD/chatsplitter.h \
	$$PWD/chateditproxy.h \
	$$PWD/adduserdlg.h \
	$$PWD/groupchatdlg.h \
	$$PWD/gcuserview.h \
	$$PWD/infodlg.h \
	$$PWD/translationmanager.h \
	$$PWD/eventdb.h \
	$$PWD/historydlg.h \
	$$PWD/tipdlg.h \
	$$PWD/searchdlg.h \
	$$PWD/registrationdlg.h \
	$$PWD/psitoolbar.h \
	$$PWD/passphrasedlg.h \
	$$PWD/vcardfactory.h \
	$$PWD/sslcertdlg.h \
	$$PWD/tasklist.h \
	$$PWD/discodlg.h \
	$$PWD/alerticon.h \
	$$PWD/alertable.h \
	$$PWD/psipopup.h \
	$$PWD/psiapplication.h \
	$$PWD/filetransdlg.h \
	$$PWD/avatars.h \
	$$PWD/actionlist.h \
	$$PWD/serverinfomanager.h \
	$$PWD/psiactionlist.h \
	$$PWD/xdata_widget.h \
	$$PWD/statuspreset.h \
	$$PWD/lastactivitytask.h \
	$$PWD/mucmanager.h \
	$$PWD/mucjoindlg.h \
	$$PWD/mucconfigdlg.h \
	$$PWD/mucaffiliationsmodel.h \
	$$PWD/mucaffiliationsproxymodel.h \
	$$PWD/mucaffiliationsview.h \
	$$PWD/rosteritemexchangetask.h \
	$$PWD/mood.h \
	$$PWD/moodcatalog.h \
	$$PWD/mooddlg.h \
	$$PWD/geolocation.h \
	$$PWD/physicallocation.h \
	$$PWD/urlbookmark.h \
	$$PWD/conferencebookmark.h \
	$$PWD/bookmarkmanager.h \
	$$PWD/pepmanager.h \
	$$PWD/pubsubsubscription.h \
	$$PWD/rc.h \
	$$PWD/psihttpauthrequest.h \
	$$PWD/httpauthmanager.h \
	$$PWD/ahcommand.h \
	$$PWD/pongserver.h \
	$$PWD/ahcommandserver.h \
	$$PWD/ahcommanddlg.h \
	$$PWD/ahcformdlg.h \
	$$PWD/ahcexecutetask.h \
	$$PWD/ahcservermanager.h \
	$$PWD/serverlistquerier.h \
	$$PWD/psioptionseditor.h \
	$$PWD/psioptions.h \
	$$PWD/voicecaller.h \
	$$PWD/voicecalldlg.h \
	$$PWD/resourcemenu.h \
	$$PWD/shortcutmanager.h \
	$$PWD/psicontactlist.h \
	$$PWD/accountlabel.h \
	$$PWD/psiactions.h \
	$$PWD/bookmarkmanagedlg.h

# Source files
SOURCES += \
	$$PWD/varlist.cpp \
	$$PWD/jidutil.cpp \
	$$PWD/showtextdlg.cpp \
	$$PWD/psi_profiles.cpp \
	$$PWD/activeprofiles.cpp \
	$$PWD/profiledlg.cpp \
	$$PWD/aboutdlg.cpp \
	$$PWD/desktoputil.cpp \
	$$PWD/fileutil.cpp \
	$$PWD/textutil.cpp \
	$$PWD/pixmaputil.cpp \
	$$PWD/accountscombobox.cpp \
	$$PWD/psievent.cpp \
	$$PWD/xmlconsole.cpp \
	$$PWD/contactview.cpp \
	$$PWD/psiiconset.cpp \
	$$PWD/applicationinfo.cpp \
	$$PWD/pgpkeydlg.cpp \
	$$PWD/pgputil.cpp \
	$$PWD/pgptransaction.cpp \
	$$PWD/serverinfomanager.cpp \
	$$PWD/userlist.cpp \
	$$PWD/mainwin.cpp \
	$$PWD/mainwin_p.cpp \
	$$PWD/psitrayicon.cpp \
	$$PWD/rtparse.cpp \
	$$PWD/systeminfo.cpp \
	$$PWD/common.cpp \
	$$PWD/proxy.cpp \
	$$PWD/miniclient.cpp \
	$$PWD/accountmanagedlg.cpp \
	$$PWD/accountadddlg.cpp \
	$$PWD/accountregdlg.cpp \
	$$PWD/accountmodifydlg.cpp \
	$$PWD/changepwdlg.cpp \
	$$PWD/msgmle.cpp \
	$$PWD/statusdlg.cpp \
	$$PWD/statuscombobox.cpp \
	$$PWD/eventdlg.cpp \
	$$PWD/chatdlg.cpp \
	$$PWD/psichatdlg.cpp \
	$$PWD/chatsplitter.cpp \
	$$PWD/chateditproxy.cpp \
	$$PWD/tipdlg.cpp \
	$$PWD/adduserdlg.cpp \
	$$PWD/groupchatdlg.cpp \
	$$PWD/gcuserview.cpp \
	$$PWD/infodlg.cpp \
	$$PWD/translationmanager.cpp \
	$$PWD/certutil.cpp \
	$$PWD/eventdb.cpp \
	$$PWD/historydlg.cpp \
	$$PWD/searchdlg.cpp \
	$$PWD/registrationdlg.cpp \
	$$PWD/psitoolbar.cpp \
	$$PWD/passphrasedlg.cpp \
	$$PWD/vcardfactory.cpp \
	$$PWD/sslcertdlg.cpp \
	$$PWD/discodlg.cpp \
	$$PWD/alerticon.cpp \
	$$PWD/alertable.cpp \
	$$PWD/psipopup.cpp \
	$$PWD/psiapplication.cpp \
	$$PWD/filetransdlg.cpp \
	$$PWD/avatars.cpp \
	$$PWD/actionlist.cpp \
	$$PWD/psiactionlist.cpp \
	$$PWD/xdata_widget.cpp \
	$$PWD/lastactivitytask.cpp \
	$$PWD/statuspreset.cpp \
	$$PWD/mucmanager.cpp \
	$$PWD/mucjoindlg.cpp \
	$$PWD/mucconfigdlg.cpp \
	$$PWD/mucaffiliationsmodel.cpp \
	$$PWD/mucaffiliationsproxymodel.cpp \
	$$PWD/mucaffiliationsview.cpp \
	$$PWD/rosteritemexchangetask.cpp \
	$$PWD/mood.cpp \
	$$PWD/moodcatalog.cpp \
	$$PWD/mooddlg.cpp \
	$$PWD/geolocation.cpp \
	$$PWD/physicallocation.cpp \
	$$PWD/urlbookmark.cpp \
	$$PWD/conferencebookmark.cpp \
	$$PWD/bookmarkmanager.cpp \
	$$PWD/pepmanager.cpp \
	$$PWD/pubsubsubscription.cpp \
	$$PWD/rc.cpp \
	$$PWD/httpauthmanager.cpp \
	$$PWD/ahcommand.cpp \
	$$PWD/pongserver.cpp \
	$$PWD/ahcommandserver.cpp \
 	$$PWD/ahcommanddlg.cpp \
	$$PWD/ahcformdlg.cpp \
	$$PWD/ahcexecutetask.cpp \
 	$$PWD/ahcservermanager.cpp \
	$$PWD/serverlistquerier.cpp \
	$$PWD/psioptions.cpp \
	$$PWD/psioptionseditor.cpp \
	$$PWD/voicecalldlg.cpp \
	$$PWD/resourcemenu.cpp \
	$$PWD/shortcutmanager.cpp \
	$$PWD/psicontactlist.cpp \
	$$PWD/psicon.cpp \
	$$PWD/psiaccount.cpp \
	$$PWD/accountlabel.cpp \
	$$PWD/bookmarkmanagedlg.cpp

whiteboarding {
	# Whiteboarding support. Still experimental.
	DEFINES += WHITEBOARDING
	QT += svg

	HEADERS += \
		$$PWD/sxe/sxemanager.h \
		$$PWD/sxe/sxesession.h \
		$$PWD/sxe/sxeedit.h \
		$$PWD/sxe/sxenewedit.h \
		$$PWD/sxe/sxeremoveedit.h \
		$$PWD/sxe/sxerecordedit.h \
 		$$PWD/sxe/sxerecord.h \
		$$PWD/whiteboarding/wbmanager.h \
		$$PWD/whiteboarding/wbdlg.h \
		$$PWD/whiteboarding/wbwidget.h \
		$$PWD/whiteboarding/wbscene.h \
		$$PWD/whiteboarding/wbitem.h \
		$$PWD/whiteboarding/wbnewitem.h \
		$$PWD/whiteboarding/wbnewpath.h \
		$$PWD/whiteboarding/wbnewimage.h
	
	SOURCES += \
		$$PWD/sxe/sxemanager.cpp \
		$$PWD/sxe/sxesession.cpp \
		$$PWD/sxe/sxeedit.cpp \
		$$PWD/sxe/sxenewedit.cpp \
		$$PWD/sxe/sxeremoveedit.cpp \
		$$PWD/sxe/sxerecordedit.cpp \
		$$PWD/sxe/sxerecord.cpp \
		$$PWD/whiteboarding/wbmanager.cpp \
		$$PWD/whiteboarding/wbdlg.cpp \
		$$PWD/whiteboarding/wbwidget.cpp \
		$$PWD/whiteboarding/wbscene.cpp \
		$$PWD/whiteboarding/wbitem.cpp \
		$$PWD/whiteboarding/wbnewitem.cpp \
		$$PWD/whiteboarding/wbnewpath.cpp \
		$$PWD/whiteboarding/wbnewimage.cpp
}

mac {
	contains( DEFINES, HAVE_GROWL ) {
		HEADERS += $$PWD/psigrowlnotifier.h 
		SOURCES += $$PWD/psigrowlnotifier.cpp 
	}

	HEADERS += $$PWD/cocoautil.h
	OBJECTIVE_SOURCES += $$PWD/cocoautil.mm
}

# Qt Designer interfaces
INTERFACES += \
	$$PWD/profileopen.ui \
	$$PWD/profilemanage.ui \
	$$PWD/profilenew.ui \
	$$PWD/proxy.ui \
	$$PWD/pgpkey.ui \
	$$PWD/accountmanage.ui \
	$$PWD/accountadd.ui \
	$$PWD/accountreg.ui \
	$$PWD/accountremove.ui \
	$$PWD/accountmodify.ui \
	$$PWD/changepw.ui \
	$$PWD/addurl.ui \
	$$PWD/adduser.ui \
	$$PWD/mucjoin.ui \
	$$PWD/info.ui \
	$$PWD/search.ui \
	$$PWD/about.ui \
	$$PWD/optioneditor.ui \
	$$PWD/passphrase.ui \
	$$PWD/sslcert.ui \
	$$PWD/mucconfig.ui \
	$$PWD/xmlconsole.ui \
	$$PWD/disco.ui \
	$$PWD/tip.ui \
	$$PWD/filetrans.ui \
	$$PWD/mood.ui \
	$$PWD/voicecall.ui \
	$$PWD/chatdlg.ui \
	$$PWD/groupchatdlg.ui \
	$$PWD/bookmarkmanage.ui

# options dialog
include($$PWD/options/options.pri)

# Plugins
psi_plugins {
	HEADERS += $$PWD/pluginmanager.h \
				$$PWD/psiplugin.h
	SOURCES += $$PWD/pluginmanager.cpp
}

dbus {
	HEADERS += 	$$PWD/dbus.h
	SOURCES += 	$$PWD/dbus.cpp
	SOURCES += $$PWD/activeprofiles_dbus.cpp
	DEFINES += USE_DBUS
	CONFIG += qdbus
}

win32:!dbus {
	SOURCES += $$PWD/activeprofiles_win.cpp
	LIBS += -lUser32
}


unix:!dbus {
	SOURCES += $$PWD/activeprofiles_stub.cpp
}

mac {
	QMAKE_LFLAGS += -framework Carbon -framework IOKit
}

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
