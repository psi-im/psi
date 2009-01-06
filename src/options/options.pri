PSIDIR_OPT = $$PWD/../../psi/src/options

# base dialog stuff
HEADERS += \
	$$PSIDIR_OPT/optionsdlg.h \
	$$PSIDIR_OPT/optionstab.h 
SOURCES += \
	$$PSIDIR_OPT/optionstab.cpp \
	$$PWD/optionsdlg.cpp 
INTERFACES += \
	$$PSIDIR_OPT/ui_options.ui


# additional tabs
HEADERS += \
	$$PWD/opt_application_b.h \
	$$PSIDIR_OPT/opt_chat.h \
	$$PSIDIR_OPT/opt_events.h \
	$$PSIDIR_OPT/opt_status.h \
	$$PSIDIR_OPT/opt_appearance.h \
	$$PSIDIR_OPT/opt_iconset.h \
	$$PSIDIR_OPT/opt_groupchat.h \
	$$PSIDIR_OPT/opt_sound.h \
	$$PSIDIR_OPT/opt_toolbars.h \
	$$PSIDIR_OPT/opt_advanced.h \
	$$PSIDIR_OPT/opt_shortcuts.h \
	$$PSIDIR_OPT/opt_tree.h

SOURCES += \
	$$PWD/opt_application.cpp \
	$$PSIDIR_OPT/opt_chat.cpp \
	$$PWD/opt_events.cpp \
	$$PWD/opt_status.cpp \
	$$PWD/opt_appearance.cpp \
	$$PWD/opt_iconset.cpp \
	$$PSIDIR_OPT/opt_groupchat.cpp \
	$$PWD/opt_sound.cpp \
	$$PSIDIR_OPT/opt_toolbars.cpp \
	$$PSIDIR_OPT/opt_advanced.cpp \
	$$PSIDIR_OPT/opt_shortcuts.cpp \
	$$PSIDIR_OPT/opt_tree.cpp


INTERFACES += \
	$$PWD/opt_application.ui \
	$$PSIDIR_OPT/opt_chat.ui \
	$$PWD/opt_events.ui \
	$$PWD/opt_status.ui \
	$$PSIDIR_OPT/opt_appearance.ui \
	$$PSIDIR_OPT/opt_appearance_misc.ui \
	$$PSIDIR_OPT/opt_sound.ui \
	$$PSIDIR_OPT/opt_advanced.ui \
	$$PSIDIR_OPT/opt_lookfeel_toolbars.ui \
	$$PSIDIR_OPT/ui_isdetails.ui \
	$$PSIDIR_OPT/opt_iconset_emo.ui \
	$$PSIDIR_OPT/opt_iconset_system.ui \
	$$PSIDIR_OPT/opt_iconset_roster.ui \
	$$PSIDIR_OPT/opt_general_groupchat.ui \
	$$PSIDIR_OPT/opt_shortcuts.ui

psi_plugins {
	INTERFACES += $$PWD/opt_plugins.ui
	SOURCES += $$PWD/opt_plugins.cpp
	HEADERS += $$PWD/opt_plugins.h
}

INCLUDEPATH += $$PWD $$PSIDIR_OPT
DEPENDPATH  += $$PWD $$PSIDIR_OPT
