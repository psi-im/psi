unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)
unset(EXTRA_LDFLAGS)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

list(APPEND HEADERS
    contactmanagerdlg.h
    contactmanagermodel.h
    contactmanagerview.h
)

list(APPEND SOURCES
    contactmanagerdlg.cpp
    contactmanagermodel.cpp
    contactmanagerview.cpp
)

list(APPEND FORMS
    contactmanagerdlg.ui
)

qt5_wrap_ui(UI_FORMS ${FORMS})
add_library(contactmanager STATIC ${HEADERS} ${SOURCES} ${UI_FORMS})
target_link_libraries(contactmanager ${QT_LIBRARIES} widgets)
target_include_directories(contactmanager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
