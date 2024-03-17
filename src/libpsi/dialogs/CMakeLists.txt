unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)
unset(EXTRA_LDFLAGS)

include_directories(libpsi/dialogs)

list(APPEND HEADERS
    grepshortcutkeydialog.h
    )

list(APPEND SOURCES
    grepshortcutkeydialog.cpp
    )

list(APPEND FORMS
    grepshortcutkeydialog.ui
    )

qt_wrap_ui(UI_FORMS ${FORMS})
add_library(libpsi_dialogs STATIC ${HEADERS} ${SOURCES} ${UI_FORMS})
target_link_libraries(libpsi_dialogs ${QT_LIBRARIES})
target_include_directories(libpsi_dialogs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
