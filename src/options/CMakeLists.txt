unset(MOC_SOURCES)
unset(HEADERS)
unset(PLAIN_HEADERS)
unset(FORMS)
unset(SOURCES)
unset(PLAIN_SOURCES)
unset(UI_FORMS)

include_directories(..)

list(APPEND HEADERS 
	optionsdlg.h
	opt_status.h
	opt_statuspep.h
	opt_toolbars.h
	opt_shortcuts.h
	opt_statusauto.h
	opt_iconset.h
	optionsdlgbase.h
	opt_appearance.h
	opt_advanced.h
	opt_events.h
	opt_popups.h
	opt_roster.h
	opt_groupchat.h
	opt_sound.h
	opt_avcall.h
	opt_chat.h
	optionstab.h
	opt_tree.h
	opt_application.h
	opt_plugins.h
	opt_statusgeneral.h
	opt_theme.h
)

list(APPEND SOURCES

	optionsdlgbase.cpp
	optionstab.cpp
	opt_plugins.cpp
)

list(APPEND PLAIN_SOURCES
	optionsdlg.cpp
	opt_advanced.cpp
	opt_appearance.cpp
	opt_application.cpp
	opt_avcall.cpp
	opt_chat.cpp
	opt_events.cpp
	opt_groupchat.cpp
	opt_iconset.cpp
	opt_popups.cpp
	opt_roster.cpp
	opt_shortcuts.cpp
	opt_sound.cpp
	opt_status.cpp
	opt_statusauto.cpp
	opt_statusgeneral.cpp
	opt_statuspep.cpp
	opt_theme.cpp
	opt_toolbars.cpp
	opt_tree.cpp
)

list(APPEND FORMS 
	opt_advanced.ui
	opt_appearance.ui
	opt_appearance_misc.ui
	opt_application.ui
	opt_avcall.ui
	opt_chat.ui
	opt_events.ui
	opt_general_groupchat.ui
	opt_iconset_activity.ui
	opt_iconset_affiliation.ui
	opt_iconset_client.ui
	opt_iconset_emo.ui
	opt_iconset_mood.ui
	opt_iconset_roster.ui
	opt_iconset_system.ui
	opt_lookfeel_toolbars.ui
	opt_plugins.ui
	opt_popups.ui
	opt_roster.ui
	opt_shortcuts.ui
	opt_sound.ui
	opt_statusauto.ui
	opt_statusgeneral.ui
	opt_statuspep.ui
	opt_theme.ui
	plugininfodialog.ui
	ui_isdetails.ui
	ui_options.ui
)

qt4_wrap_ui(UI_FORMS ${FORMS})
qt_wrap_cpp(MOC_SOURCES ${SOURCES} ${HEADERS})
add_library(options STATIC ${SOURCES} ${HEADERS} ${MOC_SOURCES} ${UI_FORMS} ${PLAIN_SOURCES})
target_link_libraries(options ${QT_LIBRARIES} tools widgets tabs libpsi_dialogs)
target_include_directories(options PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
