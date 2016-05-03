# base dialog stuff
HEADERS += \
	$$PWD/optionstab.h \
    $$PWD/optionsdlgbase.h \
    $$PWD/optionsdlg.h
SOURCES += \
	$$PWD/optionstab.cpp \
    $$PWD/optionsdlgbase.cpp \
    $$PWD/optionsdlg.cpp
FORMS += \
	$$PWD/ui_options.ui


# additional tabs
HEADERS += \
	$$PWD/opt_application.h \
	$$PWD/opt_roster.h \
	$$PWD/opt_chat.h \
	$$PWD/opt_events.h \
	$$PWD/opt_popups.h \
	$$PWD/opt_status.h \
	$$PWD/opt_statusgeneral.h \
	$$PWD/opt_statusauto.h \
	$$PWD/opt_appearance.h \
	$$PWD/opt_iconset.h \
	$$PWD/opt_theme.h \
	$$PWD/opt_groupchat.h \
	$$PWD/opt_sound.h \
	$$PWD/opt_avcall.h \
	$$PWD/opt_toolbars.h \
	$$PWD/opt_advanced.h \
	$$PWD/opt_shortcuts.h \
	$$PWD/opt_statuspep.h

HEADERS += $$PWD/opt_tree.h

SOURCES += \
	$$PWD/opt_application.cpp \
	$$PWD/opt_roster.cpp \
	$$PWD/opt_chat.cpp \
	$$PWD/opt_events.cpp \
	$$PWD/opt_popups.cpp \
	$$PWD/opt_status.cpp \
	$$PWD/opt_statusgeneral.cpp \
	$$PWD/opt_statusauto.cpp \
	$$PWD/opt_appearance.cpp \
	$$PWD/opt_iconset.cpp \
	$$PWD/opt_theme.cpp \
	$$PWD/opt_groupchat.cpp \
	$$PWD/opt_sound.cpp \
	$$PWD/opt_avcall.cpp \
	$$PWD/opt_toolbars.cpp \
	$$PWD/opt_advanced.cpp \
	$$PWD/opt_shortcuts.cpp \
	$$PWD/opt_statuspep.cpp

SOURCES += $$PWD/opt_tree.cpp

FORMS += \
	$$PWD/opt_application.ui \
	$$PWD/opt_roster.ui \
	$$PWD/opt_chat.ui \
	$$PWD/opt_events.ui \
	$$PWD/opt_popups.ui \
	$$PWD/opt_statusgeneral.ui \
	$$PWD/opt_statusauto.ui \
	$$PWD/opt_appearance.ui \
	$$PWD/opt_appearance_misc.ui \
	$$PWD/opt_theme.ui \
	$$PWD/opt_sound.ui \
	$$PWD/opt_avcall.ui \
	$$PWD/opt_advanced.ui \
	$$PWD/opt_lookfeel_toolbars.ui \
	$$PWD/ui_isdetails.ui \
	$$PWD/opt_iconset_emo.ui \
	$$PWD/opt_iconset_mood.ui \
	$$PWD/opt_iconset_activity.ui \
	$$PWD/opt_iconset_client.ui \
	$$PWD/opt_iconset_affiliation.ui \
	$$PWD/opt_iconset_system.ui \
	$$PWD/opt_iconset_roster.ui \
	$$PWD/opt_general_groupchat.ui \
	$$PWD/opt_shortcuts.ui \
	$$PWD/opt_statuspep.ui \
	$$PWD/plugininfodialog.ui

psi_plugins {
	FORMS += $$PWD/opt_plugins.ui
	SOURCES += $$PWD/opt_plugins.cpp
	HEADERS += $$PWD/opt_plugins.h
}

INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD
