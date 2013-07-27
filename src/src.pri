QT += xml network

greaterThan(QT_MAJOR_VERSION, 4) {
  QT += widgets multimedia
  DEFINES += HAVE_QT5
}
unix:!mac:DEFINES += HAVE_X11

# modules
include($$PWD/protocol/protocol.pri)
include($$PWD/irisprotocol/irisprotocol.pri)
include($$PWD/privacy/privacy.pri)
include($$PWD/capabilities/capabilities.pri)
include($$PWD/tabs/tabs.pri)
include($$PWD/Certificates/Certificates.pri)

# tools
# include($$PWD/tools/trayicon/trayicon.pri)
include($$PWD/libpsi/tools/tools.pri)
include($$PWD/tools/iconset/iconset.pri)
include($$PWD/tools/optionstree/optionstree.pri)
include($$PWD/tools/advwidget/advwidget.pri)
include($$PWD/libpsi/dialogs/grepshortcutkeydialog.pri)
INCLUDEPATH += $$PWD/libpsi/tools

# psimedia
include($$PWD/psimedia/psimedia.pri)

# audio/video calls
include($$PWD/avcall/avcall.pri)

# Tune
pep {
	DEFINES += USE_PEP
	CONFIG += tc_psifile
	mac { CONFIG += tc_itunes }
	windows { CONFIG += tc_winamp }
	unix:dbus:!mac { CONFIG += tc_mpris }
}
include($$PWD/tools/tunecontroller/tunecontroller.pri)

# Crash
use_crash {
	DEFINES += USE_CRASH
	include($$PWD/tools/crash/crash.pri)
}

# AutoUpdater
include($$PWD/AutoUpdater/AutoUpdater.pri)

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
	$$PWD/homedirmigration.h \
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
	$$PWD/psiiconset.h \
	$$PWD/applicationinfo.h \
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
	$$PWD/eventdlg.h \
	$$PWD/chatdlg.h \
	$$PWD/psichatdlg.h \
	$$PWD/chatsplitter.h \
	$$PWD/chateditproxy.h \
	$$PWD/adduserdlg.h \
	$$PWD/minicmd.h \
	$$PWD/mcmdmanager.h \
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
	$$PWD/tasklist.h \
	$$PWD/discodlg.h \
	$$PWD/alerticon.h \
	$$PWD/alertable.h \
	$$PWD/psipopup.h \
	$$PWD/popupmanager.h \
	$$PWD/psipopupinterface.h \
	$$PWD/psiapplication.h \
	$$PWD/filecache.h \
	$$PWD/avatars.h \
	$$PWD/actionlist.h \
	$$PWD/serverinfomanager.h \
	$$PWD/psiactionlist.h \
	$$PWD/xdata_widget.h \
	$$PWD/statuspreset.h \
	$$PWD/lastactivitytask.h \
	$$PWD/bobfilecache.h \
	$$PWD/mucmanager.h \
	$$PWD/mucconfigdlg.h \
	$$PWD/mucaffiliationsmodel.h \
	$$PWD/mucaffiliationsproxymodel.h \
	$$PWD/mucaffiliationsview.h \
	$$PWD/mucreasonseditor.h \
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
	$$PWD/bosskey.h \
	$$PWD/psicontactlist.h \
	$$PWD/accountlabel.h \
	$$PWD/psiactions.h \
	$$PWD/bookmarkmanagedlg.h \
	$$PWD/vcardphotodlg.h \
	$$PWD/psicli.h \
	$$PWD/coloropt.h


HEADERS += tabcompletion.h
SOURCES += tabcompletion.cpp

HEADERS += alertmanager.h
SOURCES += alertmanager.cpp

HEADERS += accountloginpassword.h
SOURCES += accountloginpassword.cpp

HEADERS += mcmdcompletion.h
SOURCES += mcmdcompletion.cpp

# Source files
SOURCES += \
	$$PWD/varlist.cpp \
	$$PWD/jidutil.cpp \
	$$PWD/showtextdlg.cpp \
	$$PWD/psi_profiles.cpp \
	$$PWD/activeprofiles.cpp \
	$$PWD/profiledlg.cpp \
	$$PWD/homedirmigration.cpp \
	$$PWD/aboutdlg.cpp \
	$$PWD/desktoputil.cpp \
	$$PWD/fileutil.cpp \
	$$PWD/textutil.cpp \
	$$PWD/pixmaputil.cpp \
	$$PWD/accountscombobox.cpp \
	$$PWD/psievent.cpp \
	$$PWD/xmlconsole.cpp \
	$$PWD/psiiconset.cpp \
	$$PWD/applicationinfo.cpp \
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
	$$PWD/mcmdmanager.cpp \
	$$PWD/mcmdsimplesite.cpp \
	$$PWD/infodlg.cpp \
	$$PWD/translationmanager.cpp \
	$$PWD/eventdb.cpp \
	$$PWD/historydlg.cpp \
	$$PWD/searchdlg.cpp \
	$$PWD/registrationdlg.cpp \
	$$PWD/psitoolbar.cpp \
	$$PWD/passphrasedlg.cpp \
	$$PWD/vcardfactory.cpp \
	$$PWD/discodlg.cpp \
	$$PWD/alerticon.cpp \
	$$PWD/alertable.cpp \
	$$PWD/psipopup.cpp \
	$$PWD/popupmanager.cpp \
	$$PWD/psipopupinterface.cpp \
	$$PWD/psiapplication.cpp \
	$$PWD/filecache.cpp \
	$$PWD/avatars.cpp \
	$$PWD/actionlist.cpp \
	$$PWD/psiactionlist.cpp \
	$$PWD/xdata_widget.cpp \
	$$PWD/lastactivitytask.cpp \
	$$PWD/bobfilecache.cpp \
	$$PWD/statuspreset.cpp \
	$$PWD/mucmanager.cpp \
	$$PWD/mucconfigdlg.cpp \
	$$PWD/mucaffiliationsmodel.cpp \
	$$PWD/mucaffiliationsproxymodel.cpp \
	$$PWD/mucaffiliationsview.cpp \
	$$PWD/mucreasonseditor.cpp \
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
	$$PWD/bosskey.cpp \
	$$PWD/psicontactlist.cpp \
	$$PWD/psicon.cpp \
	$$PWD/psiaccount.cpp \
	$$PWD/accountlabel.cpp \
	$$PWD/bookmarkmanagedlg.cpp \
	$$PWD/vcardphotodlg.cpp \
	$$PWD/coloropt.cpp

CONFIG += filetransfer
filetransfer {
	DEFINES += FILETRANSFER

	HEADERS += \
		$$PWD/filetransdlg.h

	SOURCES += \
		$$PWD/filetransdlg.cpp

	FORMS += \
		$$PWD/filetrans.ui
}

CONFIG += groupchat
groupchat {
	DEFINES += GROUPCHAT

	HEADERS += \
		$$PWD/groupchatdlg.h \
		$$PWD/gcuserview.h \
		$$PWD/mucjoindlg.h

	SOURCES += \
		$$PWD/groupchatdlg.cpp \
		$$PWD/gcuserview.cpp \
		$$PWD/mucjoindlg.cpp

	FORMS += \
		$$PWD/groupchatdlg.ui \
		$$PWD/mucjoin.ui
}

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

	include($$PWD/CocoaUtilities/CocoaUtilities.pri)
}

CONFIG += newcontactlist
newcontactlist {
	DEFINES += NEWCONTACTLIST
	# DEFINES += CONTACTLIST_NESTED_GROUPS
	HEADERS += \
		$$PWD/contactlistview.h \
		$$PWD/contactlistdragview.h \
		$$PWD/hoverabletreeview.h \
		$$PWD/contactlistmodel.h \
		$$PWD/contactlistmodelselection.h \
		$$PWD/contactlistdragmodel.h \
		$$PWD/contactlistviewdelegate.h \
		$$PWD/contactlistmodelupdater.h \
		$$PWD/contactlistproxymodel.h \
		$$PWD/psicontact.h \
		$$PWD/psiselfcontact.h \
		$$PWD/psicontactmenu.h \
		$$PWD/contactlistgroupstate.h \
		$$PWD/contactlistgroupcache.h \
		$$PWD/contactlistgroup.h \
		$$PWD/contactlistnestedgroup.h \
		$$PWD/contactlistaccountgroup.h \
		$$PWD/contactlistspecialgroup.h \
		$$PWD/contactlistgroupmenu.h \
		$$PWD/contactlistaccountmenu.h \
		$$PWD/contactlistitem.h \
		$$PWD/contactlistitemmenu.h \
		$$PWD/contactlistutil.h \
		$$PWD/contactlistitemproxy.h \
		$$PWD/contactupdatesmanager.h \
		$$PWD/statusmenu.h \
		$$PWD/psirosterwidget.h \
		$$PWD/psifilteredcontactlistview.h

	SOURCES += \
		$$PWD/contactlistview.cpp \
		$$PWD/contactlistdragview.cpp \
		$$PWD/hoverabletreeview.cpp \
		$$PWD/contactlistmodel.cpp \
		$$PWD/contactlistmodelselection.cpp \
		$$PWD/contactlistdragmodel.cpp \
		$$PWD/contactlistviewdelegate.cpp \
		$$PWD/contactlistmodelupdater.cpp \
		$$PWD/contactlistproxymodel.cpp \
		$$PWD/psicontact.cpp \
		$$PWD/psiselfcontact.cpp \
		$$PWD/psicontactmenu.cpp \
		$$PWD/contactlistgroupstate.cpp \
		$$PWD/contactlistgroupcache.cpp \
		$$PWD/contactlistgroup.cpp \
		$$PWD/contactlistnestedgroup.cpp \
		$$PWD/contactlistaccountgroup.cpp \
		$$PWD/contactlistspecialgroup.cpp \
		$$PWD/contactlistgroupmenu.cpp \
		$$PWD/contactlistaccountmenu.cpp \
		$$PWD/contactlistitem.cpp \
		$$PWD/contactlistitemmenu.cpp \
		$$PWD/contactlistutil.cpp \
		$$PWD/contactlistitemproxy.cpp \
		$$PWD/contactupdatesmanager.cpp \
		$$PWD/statusmenu.cpp \
		$$PWD/psirosterwidget.cpp \
		$$PWD/psifilteredcontactlistview.cpp

	!yapsi {
		HEADERS += \
			$$PWD/psicontactlistview.h \
			$$PWD/psicontactlistviewdelegate.h \
			$$PWD/psicontactlistmodel.h

		SOURCES += \
			$$PWD/psicontactlistview.cpp \
			$$PWD/psicontactlistviewdelegate.cpp \
			$$PWD/psicontactlistmodel.cpp
	}
}
!newcontactlist {
	QT += qt3support
	HEADERS += \
		$$PWD/legacypsiaccount.h \
		$$PWD/contactview.h

	SOURCES += \
		$$PWD/legacypsiaccount.cpp \
		$$PWD/contactview.cpp
}

CONFIG += pgputil
pgputil {
	DEFINES += HAVE_PGPUTIL
	HEADERS += \
		$$PWD/pgputil.h \
		$$PWD/pgpkeydlg.h

	SOURCES += \
		$$PWD/pgputil.cpp \
		$$PWD/pgpkeydlg.cpp

	FORMS += \
		$$PWD/pgpkey.ui
}

HEADERS += \
	$$PWD/removeconfirmationmessagebox.h \
	$$PWD/globaleventqueue.h \
	$$PWD/dummystream.h

SOURCES += \
	$$PWD/removeconfirmationmessagebox.cpp \
	$$PWD/globaleventqueue.cpp \
	$$PWD/dummystream.cpp

# Qt Designer forms
FORMS += \
	$$PWD/profileopen.ui \
	$$PWD/profilemanage.ui \
	$$PWD/profilenew.ui \
	$$PWD/homedirmigration.ui \
	$$PWD/proxy.ui \
	$$PWD/accountmanage.ui \
	$$PWD/accountadd.ui \
	$$PWD/accountreg.ui \
	$$PWD/accountremove.ui \
	$$PWD/accountmodify.ui \
	$$PWD/changepw.ui \
	$$PWD/addurl.ui \
	$$PWD/adduser.ui \
	$$PWD/info.ui \
	$$PWD/search.ui \
	$$PWD/about.ui \
	$$PWD/history.ui \
	$$PWD/optioneditor.ui \
	$$PWD/passphrase.ui \
	$$PWD/mucconfig.ui \
	$$PWD/mucreasonseditor.ui \
	$$PWD/xmlconsole.ui \
	$$PWD/disco.ui \
	$$PWD/tip.ui \
	$$PWD/mood.ui \
	$$PWD/voicecall.ui \
	$$PWD/chatdlg.ui \
	$$PWD/bookmarkmanage.ui \
	$$PWD/ahcommanddlg.ui \
	$$PWD/ahcformdlg.ui

# options dialog
include($$PWD/options/options.pri)

# Plugins
psi_plugins {
	DEFINES += PSI_PLUGINS

	HEADERS += \
		$$PWD/pluginmanager.h \
		$$PWD/pluginhost.h

	SOURCES += \
		$$PWD/pluginmanager.cpp \
		$$PWD/pluginhost.cpp

	include($$PWD/plugins/plugins.pri)
}

dbus {
	HEADERS += 	$$PWD/dbus.h \
			$$PWD/psidbusnotifier.h
	SOURCES += 	$$PWD/dbus.cpp
	SOURCES +=	$$PWD/activeprofiles_dbus.cpp \
			$$PWD/psidbusnotifier.cpp
	DEFINES += USE_DBUS
	QT += dbus
}

win32:!dbus {
	SOURCES += $$PWD/activeprofiles_win.cpp
	LIBS += -lUser32
}


unix:!dbus {
	SOURCES += $$PWD/activeprofiles_stub.cpp
}

mac {
	QMAKE_LFLAGS += -framework Carbon -framework IOKit -framework AppKit
}

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
