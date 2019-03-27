unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)

set(HEADERS
    tabdlg.h
    tabmanager.h
    tabbablewidget.h
)

set(SOURCES
    tabdlg.cpp
    tabmanager.cpp
    tabbablewidget.cpp
)

add_library(tabs STATIC  ${HEADERS} ${SOURCES})
target_link_libraries(tabs ${QT_LIBRARIES} ${iris_LIB} tools widgets)
target_include_directories(tabs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
