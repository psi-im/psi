PSIDIR_SRC = $$PSIDIR/src

QT += xml network qt3support

# modules
include($$PSIDIR_SRC/protocol/protocol.pri)
include($$PSIDIR_SRC/irisprotocol/irisprotocol.pri)
include($$PWD/privacy/privacy.pri)
include($$PSIDIR_SRC/capabilities/capabilities.pri)
include($$PSIDIR_SRC/utilities/utilities.pri)
include($$PSIDIR_SRC/tabs/tabs.pri)

# tools
include($$PSIDIR_SRC/tools/trayicon/trayicon.pri)
include($$PSIDIR_SRC/tools/iconset/iconset.pri)
include($$PSIDIR_SRC/tools/idle/idle.pri)
include($$PSIDIR_SRC/tools/systemwatch/systemwatch.pri)
include($$PSIDIR_SRC/tools/zip/zip.pri)
include($$PSIDIR_SRC/tools/optionstree/optionstree.pri)
include($$PSIDIR_SRC/tools/globalshortcut/globalshortcut.pri)
include($$PSIDIR_SRC/tools/advwidget/advwidget.pri)
include($$PSIDIR_SRC/tools/spellchecker/spellchecker.pri)
include($$PSIDIR_SRC/tools/contactlist/contactlist.pri)
include($$PSIDIR_SRC/tools/grepshortcutkeydlg/grepshortcutkeydlg.pri)
include($$PSIDIR_SRC/tools/atomicxmlfile/atomicxmlfile.pri)

# psimedia
include($$PWD/psimedia/psimedia.pri)

# audio/video calls
include($$PWD/avcall/avcall.pri)

INCLUDEPATH += $$PWD/libpsi/widgets
HEADERS += $$PWD/libpsi/widgets/groupchatbrowsewindow.h
SOURCES += $$PWD/libpsi/widgets/groupchatbrowsewindow.cpp

# Growl
mac {
	contains(DEFINES, HAVE_GROWL) {
		include($$PSIDIR_SRC/tools/growlnotifier/growlnotifier.pri)
	}
}

# Mac dock
mac { include($$PSIDIR_SRC/tools/mac_dock/mac_dock.pri) }

# Tune
pep {
	DEFINES += USE_PEP
	CONFIG += tc_psifile
	mac { CONFIG += tc_itunes }
	windows { CONFIG += tc_winamp }
}
include($$PSIDIR_SRC/tools/tunecontroller/tunecontroller.pri)

# Crash
use_crash {
	DEFINES += USE_CRASH
	include($$PSIDIR_SRC/tools/crash/crash.pri)
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
	include($$PSIDIR/third-party/qca/qca-gnupg.pri)
}
else {
	CONFIG += crypto	
}

# Widgets
include($$PSIDIR_SRC/widgets/widgets.pri)

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
#	$$PSIDIR_SRC/maybe.h \ 
	$$PSIDIR_SRC/varlist.h \ 
	$$PSIDIR_SRC/jidutil.h \
	$$PSIDIR_SRC/showtextdlg.h \ 
	$$PWD/profiles_b.h \
	$$PSIDIR_SRC/activeprofiles.h \
	$$PSIDIR_SRC/profiledlg.h \
	$$PSIDIR_SRC/aboutdlg.h \
	$$PSIDIR_SRC/desktoputil.h \
	$$PSIDIR_SRC/fileutil.h \
	$$PSIDIR_SRC/textutil.h \
	$$PSIDIR_SRC/pixmaputil.h \
	$$PWD/psiaccount_b.h \
	$$PWD/psicon_b.h \
	$$PSIDIR_SRC/accountscombobox.h \
	$$PSIDIR_SRC/psievent.h \
	$$PSIDIR_SRC/xmlconsole.h \
	$$PWD/contactview_b.h \
	$$PSIDIR_SRC/psiiconset.h \
	$$PSIDIR_SRC/applicationinfo.h \
	$$PSIDIR_SRC/pgpkeydlg.h \
	$$PSIDIR_SRC/pgputil.h \
	$$PSIDIR_SRC/pgptransaction.h \
	$$PSIDIR_SRC/userlist.h \
	$$PWD/mainwin_b.h \
	$$PWD/mainwin_p_b.h \
	$$PSIDIR_SRC/psitrayicon.h \
	$$PSIDIR_SRC/rtparse.h \
	$$PSIDIR_SRC/systeminfo.h \
	$$PWD/common_b.h \
	$$PSIDIR_SRC/proxy.h \
	$$PSIDIR_SRC/miniclient.h \
	$$PSIDIR_SRC/accountmanagedlg.h \
	$$PSIDIR_SRC/accountadddlg.h \
	$$PSIDIR_SRC/accountregdlg.h \
	$$PSIDIR_SRC/accountmodifydlg.h \
	$$PSIDIR_SRC/changepwdlg.h \
	$$PSIDIR_SRC/msgmle.h \
	$$PSIDIR_SRC/statusdlg.h \
	$$PSIDIR_SRC/statuscombobox.h \
	$$PSIDIR_SRC/certutil.h \
	$$PSIDIR_SRC/eventdlg.h \
	$$PWD/chatdlg_b.h \
	$$PWD/psichatdlg_b.h \
	$$PSIDIR_SRC/chatsplitter.h \
	$$PSIDIR_SRC/chateditproxy.h \
#	$$PWD/tabdlg.h \
	$$PSIDIR_SRC/adduserdlg.h \
	$$PWD/groupchatdlg_b.h \
	$$PSIDIR_SRC/gcuserview.h \
	$$PSIDIR_SRC/infodlg.h \
	$$PSIDIR_SRC/translationmanager.h \
	$$PSIDIR_SRC/eventdb.h \
	$$PSIDIR_SRC/historydlg.h \
	$$PSIDIR_SRC/tipdlg.h \
	$$PSIDIR_SRC/searchdlg.h \
	$$PSIDIR_SRC/registrationdlg.h \
	$$PSIDIR_SRC/psitoolbar.h \
	$$PSIDIR_SRC/passphrasedlg.h \
	$$PSIDIR_SRC/vcardfactory.h \
	$$PSIDIR_SRC/sslcertdlg.h \
	$$PSIDIR_SRC/tasklist.h \
	$$PSIDIR_SRC/discodlg.h \
#	$$PSIDIR_SRC/capsspec.h \
#	$$PSIDIR_SRC/capsregistry.h \
#	$$PSIDIR_SRC/capsmanager.h \
	$$PSIDIR_SRC/alerticon.h \
	$$PSIDIR_SRC/alertable.h \
	$$PSIDIR_SRC/psipopup.h \
	$$PSIDIR_SRC/psiapplication.h \
	$$PSIDIR_SRC/filetransdlg.h \
	$$PSIDIR_SRC/avatars.h \
	$$PSIDIR_SRC/actionlist.h \
	$$PWD/serverinfomanager_b.h \
	$$PSIDIR_SRC/psiactionlist.h \
	$$PSIDIR_SRC/xdata_widget.h \
	$$PSIDIR_SRC/statuspreset.h \
	$$PSIDIR_SRC/lastactivitytask.h \
	$$PSIDIR_SRC/mucmanager.h \
	$$PSIDIR_SRC/mucjoindlg.h \
	$$PSIDIR_SRC/mucconfigdlg.h \
	$$PSIDIR_SRC/mucaffiliationsmodel.h \
	$$PSIDIR_SRC/mucaffiliationsproxymodel.h \
	$$PSIDIR_SRC/mucaffiliationsview.h \
	$$PSIDIR_SRC/rosteritemexchangetask.h \
	$$PSIDIR_SRC/mood.h \
	$$PSIDIR_SRC/moodcatalog.h \
	$$PSIDIR_SRC/mooddlg.h \
	$$PSIDIR_SRC/geolocation.h \
	$$PSIDIR_SRC/physicallocation.h \
	$$PSIDIR_SRC/urlbookmark.h \
	$$PSIDIR_SRC/conferencebookmark.h \
	$$PSIDIR_SRC/bookmarkmanager.h \
	$$PSIDIR_SRC/pepmanager.h \
	$$PSIDIR_SRC/pubsubsubscription.h \
	$$PSIDIR_SRC/rc.h \
	$$PSIDIR_SRC/psihttpauthrequest.h \
	$$PSIDIR_SRC/httpauthmanager.h \
#	$$PSIDIR_SRC/privacylistitem.h \
#	$$PSIDIR_SRC/privacylist.h \
#	$$PSIDIR_SRC/privacylistmodel.h \
#	$$PSIDIR_SRC/privacylistblockedmodel.h \
#	$$PSIDIR_SRC/privacymanager.h \
#	$$PSIDIR_SRC/privacydlg.h \
#	$$PSIDIR_SRC/privacyruledlg.h \
	$$PSIDIR_SRC/ahcommand.h \
	$$PSIDIR_SRC/pongserver.h \
	$$PSIDIR_SRC/ahcommandserver.h \
	$$PSIDIR_SRC/ahcommanddlg.h \
	$$PSIDIR_SRC/ahcformdlg.h \
	$$PSIDIR_SRC/ahcexecutetask.h \
	$$PSIDIR_SRC/ahcservermanager.h \
	$$PSIDIR_SRC/serverlistquerier.h \
	$$PSIDIR_SRC/psioptionseditor.h \
	$$PSIDIR_SRC/psioptions.h \
	$$PSIDIR_SRC/voicecaller.h \
	$$PSIDIR_SRC/voicecalldlg.h \
	$$PSIDIR_SRC/resourcemenu.h \
	$$PSIDIR_SRC/shortcutmanager.h \
	$$PSIDIR_SRC/psicontactlist.h \
	$$PSIDIR_SRC/accountlabel.h \
	$$PSIDIR_SRC/psiactions.h \
	$$PSIDIR_SRC/bookmarkmanagedlg.h \
	$$PWD/adduserwizard.h \
	$$PWD/transportsetupdlg.h \
	$$PWD/cudaskin.h \
	$$PWD/cudatasks.h \
	$$PWD/simplesearch.h \
	$$PWD/simpleprivacymanager.h \
	$$PWD/invitedlg.h \
	$$PWD/ringsound.h \
	$$PWD/whatismyip.h \
	$$PWD/verticaltabbar.h \
	$$PWD/groupchatbrowsecontroller.h

# Source files
SOURCES += \
	$$PSIDIR_SRC/varlist.cpp \
	$$PSIDIR_SRC/jidutil.cpp \
	$$PSIDIR_SRC/showtextdlg.cpp \
	$$PWD/psi_profiles.cpp \
	$$PSIDIR_SRC/activeprofiles.cpp \
	$$PSIDIR_SRC/profiledlg.cpp \
	$$PWD/aboutdlg.cpp \
	$$PSIDIR_SRC/desktoputil.cpp \
	$$PSIDIR_SRC/fileutil.cpp \
	$$PSIDIR_SRC/textutil.cpp \
	$$PSIDIR_SRC/pixmaputil.cpp \
	$$PSIDIR_SRC/accountscombobox.cpp \
	$$PSIDIR_SRC/psievent.cpp \
	$$PSIDIR_SRC/xmlconsole.cpp \
	$$PWD/contactview.cpp \
	$$PSIDIR_SRC/psiiconset.cpp \
	$$PWD/applicationinfo.cpp \
	$$PSIDIR_SRC/pgpkeydlg.cpp \
	$$PSIDIR_SRC/pgputil.cpp \
	$$PSIDIR_SRC/pgptransaction.cpp \
	$$PWD/serverinfomanager.cpp \
	$$PSIDIR_SRC/userlist.cpp \
	$$PWD/mainwin.cpp \
	$$PWD/mainwin_p.cpp \
	$$PSIDIR_SRC/psitrayicon.cpp \
	$$PSIDIR_SRC/rtparse.cpp \
	$$PSIDIR_SRC/systeminfo.cpp \
	$$PWD/common.cpp \
	$$PSIDIR_SRC/proxy.cpp \
	$$PSIDIR_SRC/miniclient.cpp \
	$$PSIDIR_SRC/accountmanagedlg.cpp \
	$$PSIDIR_SRC/accountadddlg.cpp \
	$$PSIDIR_SRC/accountregdlg.cpp \
	$$PWD/accountmodifydlg.cpp \
	$$PSIDIR_SRC/changepwdlg.cpp \
	$$PSIDIR_SRC/msgmle.cpp \
	$$PWD/statusdlg.cpp \
	$$PSIDIR_SRC/statuscombobox.cpp \
	$$PSIDIR_SRC/eventdlg.cpp \
	$$PWD/chatdlg.cpp \
	$$PWD/psichatdlg.cpp \
	$$PSIDIR_SRC/chatsplitter.cpp \
	$$PSIDIR_SRC/chateditproxy.cpp \
	$$PSIDIR_SRC/tipdlg.cpp \
#	$$PWD/tabdlg.cpp \
	$$PSIDIR_SRC/adduserdlg.cpp \
	$$PWD/groupchatdlg.cpp \
	$$PSIDIR_SRC/gcuserview.cpp \
	$$PWD/infodlg.cpp \
	$$PSIDIR_SRC/translationmanager.cpp \
	$$PSIDIR_SRC/certutil.cpp \
	$$PSIDIR_SRC/eventdb.cpp \
	$$PSIDIR_SRC/historydlg.cpp \
	$$PSIDIR_SRC/searchdlg.cpp \
	$$PSIDIR_SRC/registrationdlg.cpp \
	$$PSIDIR_SRC/psitoolbar.cpp \
	$$PSIDIR_SRC/passphrasedlg.cpp \
	$$PSIDIR_SRC/vcardfactory.cpp \
	$$PSIDIR_SRC/sslcertdlg.cpp \
	$$PSIDIR_SRC/discodlg.cpp \
#	$$PSIDIR_SRC/capsspec.cpp \
#	$$PSIDIR_SRC/capsregistry.cpp \
#	$$PWD/capsmanager.cpp \
	$$PSIDIR_SRC/alerticon.cpp \
	$$PSIDIR_SRC/alertable.cpp \
	$$PWD/psipopup.cpp \
	$$PSIDIR_SRC/psiapplication.cpp \
	$$PWD/filetransdlg.cpp \
	$$PSIDIR_SRC/avatars.cpp \
	$$PSIDIR_SRC/actionlist.cpp \
	$$PWD/psiactionlist.cpp \
	$$PSIDIR_SRC/xdata_widget.cpp \
	$$PSIDIR_SRC/lastactivitytask.cpp \
	$$PSIDIR_SRC/statuspreset.cpp \
	$$PSIDIR_SRC/mucmanager.cpp \
	$$PWD/mucjoindlg.cpp \
	$$PWD/mucconfigdlg.cpp \
	$$PSIDIR_SRC/mucaffiliationsmodel.cpp \
	$$PSIDIR_SRC/mucaffiliationsproxymodel.cpp \
	$$PSIDIR_SRC/mucaffiliationsview.cpp \
	$$PSIDIR_SRC/rosteritemexchangetask.cpp \
	$$PSIDIR_SRC/mood.cpp \
	$$PSIDIR_SRC/moodcatalog.cpp \
	$$PSIDIR_SRC/mooddlg.cpp \
	$$PSIDIR_SRC/geolocation.cpp \
	$$PSIDIR_SRC/physicallocation.cpp \
	$$PSIDIR_SRC/urlbookmark.cpp \
	$$PSIDIR_SRC/conferencebookmark.cpp \
	$$PSIDIR_SRC/bookmarkmanager.cpp \
	$$PSIDIR_SRC/pepmanager.cpp \
	$$PSIDIR_SRC/pubsubsubscription.cpp \
	$$PSIDIR_SRC/rc.cpp \
	$$PSIDIR_SRC/httpauthmanager.cpp \
#	$$PSIDIR_SRC/privacylistitem.cpp \
#	$$PSIDIR_SRC/privacylist.cpp \
#	$$PSIDIR_SRC/privacylistmodel.cpp \
#	$$PSIDIR_SRC/privacylistblockedmodel.cpp \
#	$$PSIDIR_SRC/privacymanager.cpp \
#	$$PSIDIR_SRC/privacydlg.cpp \
#	$$PSIDIR_SRC/privacyruledlg.cpp \
	$$PSIDIR_SRC/ahcommand.cpp \
	$$PSIDIR_SRC/pongserver.cpp \
	$$PSIDIR_SRC/ahcommandserver.cpp \
	$$PSIDIR_SRC/ahcommanddlg.cpp \
	$$PSIDIR_SRC/ahcformdlg.cpp \
	$$PSIDIR_SRC/ahcexecutetask.cpp \
	$$PSIDIR_SRC/ahcservermanager.cpp \
	$$PSIDIR_SRC/serverlistquerier.cpp \
	$$PWD/psioptions.cpp \
	$$PSIDIR_SRC/psioptionseditor.cpp \
	$$PSIDIR_SRC/voicecalldlg.cpp \
	$$PSIDIR_SRC/resourcemenu.cpp \
	$$PSIDIR_SRC/shortcutmanager.cpp \
	$$PSIDIR_SRC/psicontactlist.cpp \
	$$PWD/psicon.cpp \
	$$PWD/psiaccount.cpp \
	$$PSIDIR_SRC/accountlabel.cpp \
	$$PSIDIR_SRC/bookmarkmanagedlg.cpp \
	$$PWD/adduserwizard.cpp \
	$$PWD/transportsetupdlg.cpp \
	$$PWD/cudaskin.cpp \
	$$PWD/cudatasks.cpp \
	$$PWD/simplesearch.cpp \
	$$PWD/simpleprivacymanager.cpp \
	$$PWD/invitedlg.cpp \
	$$PWD/ringsound.cpp \
	$$PWD/whatismyip.cpp \
	$$PWD/verticaltabbar.cpp \
	$$PWD/groupchatbrowsecontroller.cpp

whiteboarding {
	# Whiteboarding support. Still experimental.
	DEFINES += WHITEBOARDING

	HEADERS += \
		$$PWD/wbmanager.h \
		$$PWD/wbdlg.h \
		$$PWD/wbwidget.h \
		$$PWD/wbscene.h \
		$$PWD/wbitems.h
	
	SOURCES += \
		$$PWD/wbmanager.cpp \
		$$PWD/wbdlg.cpp \
		$$PWD/wbwidget.cpp \
		$$PWD/wbscene.cpp \
		$$PWD/wbitems.cpp
}

mac {
	contains( DEFINES, HAVE_GROWL ) {
		HEADERS += $$PSIDIR_SRC/psigrowlnotifier.h 
		SOURCES += $$PWD/psigrowlnotifier.cpp 
	}

	HEADERS += $$PSIDIR_SRC/cocoautil.h
	OBJECTIVE_SOURCES += $$PSIDIR_SRC/cocoautil.mm
}

# Qt Designer interfaces
INTERFACES += \
	$$PSIDIR_SRC/profileopen.ui \
	$$PSIDIR_SRC/profilemanage.ui \
	$$PSIDIR_SRC/profilenew.ui \
	$$PSIDIR_SRC/proxy.ui \
	$$PSIDIR_SRC/pgpkey.ui \
	$$PSIDIR_SRC/accountmanage.ui \
	$$PSIDIR_SRC/accountadd.ui \
	$$PSIDIR_SRC/accountreg.ui \
	$$PSIDIR_SRC/accountremove.ui \
	$$PWD/accountmodify.ui \
	$$PSIDIR_SRC/changepw.ui \
	$$PSIDIR_SRC/addurl.ui \
	$$PSIDIR_SRC/adduser.ui \
	$$PSIDIR_SRC/mucjoin.ui \
	$$PWD/info.ui \
	$$PSIDIR_SRC/search.ui \
	$$PWD/about.ui \
	$$PSIDIR_SRC/optioneditor.ui \
	$$PSIDIR_SRC/passphrase.ui \
	$$PSIDIR_SRC/sslcert.ui \
	$$PSIDIR_SRC/mucconfig.ui \
	$$PSIDIR_SRC/xmlconsole.ui \
	$$PSIDIR_SRC/disco.ui \
	$$PSIDIR_SRC/tip.ui \
	$$PSIDIR_SRC/filetrans.ui \
	#$$PSIDIR_SRC/privacy.ui \
	#$$PSIDIR_SRC/privacyrule.ui \
	$$PSIDIR_SRC/mood.ui \
	$$PSIDIR_SRC/voicecall.ui \
	$$PSIDIR_SRC/chatdlg.ui \
	$$PSIDIR_SRC/groupchatdlg.ui \
	$$PSIDIR_SRC/bookmarkmanage.ui \
	$$PWD/transportsetup.ui \
	$$PWD/invite.ui \
	$$PWD/libpsi/widgets/groupchatbrowsewindow.ui

# options dialog
include($$PWD/options/options.pri)

# Plugins
psi_plugins {
	HEADERS += $$PWD/pluginmanager.h \
				$$PWD/psiplugin.h
	SOURCES += $$PWD/pluginmanager.cpp
}

mac {
	QMAKE_LFLAGS += -framework Carbon -framework IOKit
}

dbus {
	HEADERS +=	$$PSIDIR_SRC/dbus.h
	SOURCES +=	$$PSIDIR_SRC/dbus.cpp
	SOURCES += $$PSIDIR_SRC/activeprofiles_dbus.cpp
	DEFINES += USE_DBUS
	CONFIG += qdbus
}

win32:!dbus {
	SOURCES += $$PSIDIR_SRC/activeprofiles_win.cpp
	LIBS += -lUser32
}


unix:!dbus {
	SOURCES += $$PSIDIR_SRC/activeprofiles_stub.cpp
}

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
