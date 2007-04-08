# cutestuff
include($$PWD/../cutestuff/cutestuff.pri)

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

# Growl
mac {
	contains(DEFINES, HAVE_GROWL) {
		include($$PWD/tools/growlnotifier/growlnotifier.pri)
	}
}

# Mac dock
mac { include($$PWD/tools/mac_dock/mac_dock.pri) }

# Tune
CONFIG += tc_psifile
mac { CONFIG += tc_itunes }
windows { CONFIG += tc_winamp }
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
	QCA_CPP = $$PWD/../third-party/qca
	INCLUDEPATH += $$QCA_CPP/include/QtCrypto
	LIBS += -L$$QCA_CPP -lqca_psi
	windows:LIBS += -lcrypt32
	mac:LIBS += -framework Security

	# QCA-OpenSSL
	contains(DEFINES, HAVE_OPENSSL) {
		include($$PWD/../third-party/qca-openssl.pri)
	}
	
	# QCA-SASL
	contains(DEFINES, HAVE_CYRUSSASL) {
		include($$PWD/../third-party/qca-sasl.pri)
	}

	# QCA-GnuPG
	include($$PWD/../third-party/qca-gnupg.pri)
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
	$$PWD/maybe.h \ 
	$$PWD/varlist.h \ 
	$$PWD/jidutil.h \
	$$PWD/showtextdlg.h \ 
	$$PWD/profiles.h \
	$$PWD/profiledlg.h \
	$$PWD/aboutdlg.h \
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
	$$PWD/certutil.h \
	$$PWD/eventdlg.h \
	$$PWD/chatdlg.h \
	$$PWD/chatsplitter.h \
	$$PWD/chateditproxy.h \
	$$PWD/tabdlg.h \
	$$PWD/adduserdlg.h \
	$$PWD/groupchatdlg.h \
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
#	$$PWD/qwextend.h \
	$$PWD/tasklist.h \
	$$PWD/discodlg.h \
	$$PWD/capsspec.h \
	$$PWD/capsregistry.h \
	$$PWD/capsmanager.h \
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
 	$$PWD/privacylistitem.h \
 	$$PWD/privacylist.h \
 	$$PWD/privacylistmodel.h \
 	$$PWD/privacylistblockedmodel.h \
 	$$PWD/privacymanager.h \
 	$$PWD/privacydlg.h \
 	$$PWD/privacyruledlg.h \
	$$PWD/ahcommand.h \
	$$PWD/ahcommandserver.h \
	$$PWD/ahcommanddlg.h \
	$$PWD/ahcformdlg.h \
	$$PWD/ahcexecutetask.h \
	$$PWD/ahcservermanager.h \
	$$PWD/serverlistquerier.h \
	$$PWD/psioptions.h \
	$$PWD/voicecaller.h \
	$$PWD/voicecalldlg.h \
	$$PWD/resourcemenu.h \
	$$PWD/shortcutmanager.h \
	$$PWD/psicontactlist.h \
	$$PWD/accountlabel.h

# Source files
SOURCES += \
	$$PWD/varlist.cpp \
	$$PWD/jidutil.cpp \
	$$PWD/showtextdlg.cpp \
	$$PWD/psi_profiles.cpp \
	$$PWD/profiledlg.cpp \
	$$PWD/aboutdlg.cpp \
	$$PWD/textutil.cpp \
	$$PWD/pixmaputil.cpp \
	$$PWD/psiaccount.cpp \
	$$PWD/psicon.cpp \
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
	$$PWD/eventdlg.cpp \
	$$PWD/chatdlg.cpp \
	$$PWD/chatsplitter.cpp \
	$$PWD/chateditproxy.cpp \
	$$PWD/tipdlg.cpp \
	$$PWD/tabdlg.cpp \
	$$PWD/adduserdlg.cpp \
	$$PWD/groupchatdlg.cpp \
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
#	$$PWD/qwextend.cpp \
	$$PWD/discodlg.cpp \
	$$PWD/capsspec.cpp \
	$$PWD/capsregistry.cpp \
	$$PWD/capsmanager.cpp \
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
 	$$PWD/privacylistitem.cpp \
 	$$PWD/privacylist.cpp \
 	$$PWD/privacylistmodel.cpp \
 	$$PWD/privacylistblockedmodel.cpp \
 	$$PWD/privacymanager.cpp \
 	$$PWD/privacydlg.cpp \
 	$$PWD/privacyruledlg.cpp \
	$$PWD/ahcommand.cpp \
	$$PWD/ahcommandserver.cpp \
 	$$PWD/ahcommanddlg.cpp \
	$$PWD/ahcformdlg.cpp \
	$$PWD/ahcexecutetask.cpp \
 	$$PWD/ahcservermanager.cpp \
	$$PWD/serverlistquerier.cpp \
	$$PWD/psioptions.cpp \
	$$PWD/voicecalldlg.cpp \
	$$PWD/resourcemenu.cpp \
	$$PWD/shortcutmanager.cpp \
	$$PWD/psicontactlist.cpp \
	$$PWD/accountlabel.cpp

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
		$$PWD/voicecalldlg.cpp \
		$$PWD/wbmanager.cpp \
		$$PWD/wbdlg.cpp \
		$$PWD/wbwidget.cpp \
		$$PWD/wbscene.cpp \
		$$PWD/wbitems.cpp
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
	$$PWD/passphrase.ui \
	$$PWD/sslcert.ui \
	$$PWD/mucconfig.ui \
	$$PWD/xmlconsole.ui \
	$$PWD/disco.ui \
	$$PWD/tip.ui \
	$$PWD/filetrans.ui \
	$$PWD/privacy.ui \
	$$PWD/privacyrule.ui \
	$$PWD/mood.ui \
	$$PWD/voicecall.ui \
	$$PWD/chatdlg.ui

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

