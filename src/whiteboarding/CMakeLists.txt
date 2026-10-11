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
    wbnewitem.cpp
    wbnewpath.cpp
    wbnewshape.cpp
    wbscene.cpp
    wbwidget.cpp
)

set(HEADERS
    wbdlg.h
    wbitem.h
    wbmanager.h
    wbscene.h
    wbwidget.h

    wbnewitem.h
    wbnewpath.h
    wbnewshape.h
)

add_library(whiteboarding STATIC ${HEADERS} ${SOURCES})
add_dependencies(whiteboarding build_ui_files)
target_link_libraries(whiteboarding ${QT_LIBRARIES} ${iris_LIB} tools widgets)
