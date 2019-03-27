unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)
unset(EXTRA_LDFLAGS)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SOURCES
    wbdlg.cpp
    wbitem.cpp
    wbmanager.cpp
    wbnewimage.cpp
    wbnewitem.cpp
    wbnewpath.cpp
    wbscene.cpp
    wbwidget.cpp
)

set(HEADERS
    wbdlg.h
    wbitem.h
    wbmanager.h
    wbscene.h
    wbwidget.h

    wbnewimage.h
    wbnewitem.h
    wbnewpath.h
)

add_library(whiteboarding STATIC ${HEADERS} ${SOURCES})
add_dependencies(whiteboarding build_ui_files)
target_link_libraries(whiteboarding ${QT_LIBRARIES} tools widgets)
